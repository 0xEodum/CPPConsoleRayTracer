#include "app/Cli.hpp"

#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/App.hpp"
#include "app/Files.hpp"
#include "core/Timer.hpp"
#include "optics/LightTracer.hpp"
#include "optics/Overlay.hpp"
#include "optics/Presets.hpp"
#include "rt/PathTracer.hpp"
#include "rt/Presets.hpp"
#include "ui/Widgets.hpp"

#ifndef CRT_VERSION
#define CRT_VERSION "dev"
#endif

namespace crt::app {
namespace {

const char* const kUsage = R"(Console Ray Tracer )" CRT_VERSION R"( - 2D optics sandbox and 3D path tracer in your terminal

USAGE
  raytracer [options]                  interactive mode (2D optics lab, Tab switches to 3D)
  raytracer render2d [options]         render a 2D optics scene to PNG
  raytracer render3d [options]         render a 3D scene to PNG
  raytracer bench [--seconds S]        measure rendering throughput
  raytracer list                       list the built-in scenes

INTERACTIVE OPTIONS
  --3d                start in the 3D path tracer
  --scene NAME|FILE   2D/3D demo name or a 2D scene file (also the save/load path)
  --ascii             ASCII-art image style instead of half-block pixels
  --256               use the 256-colour palette (for terminals without true colour)
  --threads N         worker threads (default: all cores)

RENDER OPTIONS
  --scene NAME|FILE   scene to render (default: dispersion / showcase)
  -o, --output FILE   output PNG (default: timestamped name)
  --width W           image width   (default: 1600 for 2D, 1280 for 3D)
  --height H          image height  (default: from the scene's aspect, 720 for 3D)
  --rays N            2D: number of light rays (default: 20000000)
  --spp N             3D: samples per pixel (default: 256)
  --bounces N         3D: maximum path length (default: 8)
  --ev E              exposure compensation in stops (default: 0)
  --no-overlay        2D: do not draw object outlines

  -h, --help          show this help          --version   show the version
)";

struct Args {
    std::string command;
    std::map<std::string, std::string> values;
    std::set<std::string> flags;
};

bool parseArgs(int argc, char** argv, Args& args, std::string& error) {
    static const std::set<std::string> valued = {"--scene", "-o", "--output", "--width", "--height", "--rays",
                                                  "--spp",   "--bounces", "--ev", "--threads", "--seconds"};
    static const std::set<std::string> flags = {"--3d", "--ascii", "--256", "--no-overlay", "-h", "--help", "--version"};
    static const std::set<std::string> commands = {"render2d", "render3d", "bench", "list", "help"};
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (valued.count(a)) {
            if (i + 1 >= argc) {
                error = "missing value for " + a;
                return false;
            }
            args.values[a == "-o" ? "--output" : a] = argv[++i];
        } else if (flags.count(a)) {
            args.flags.insert(a == "-h" ? "--help" : a);
        } else if (args.command.empty() && commands.count(a)) {
            args.command = a;
        } else {
            error = "unknown argument '" + a + "'";
            return false;
        }
    }
    return true;
}

bool numberArg(const Args& args, const std::string& key, double& out, std::string& error) {
    const auto it = args.values.find(key);
    if (it == args.values.end()) return true;
    try {
        std::size_t used = 0;
        out = std::stod(it->second, &used);
        if (used != it->second.size()) throw std::invalid_argument(key);
    } catch (const std::exception&) {
        error = "invalid number for " + key + ": '" + it->second + "'";
        return false;
    }
    return true;
}

std::string valueOr(const Args& args, const std::string& key, const std::string& fallback) {
    const auto it = args.values.find(key);
    return it == args.values.end() ? fallback : it->second;
}

bool is3dPreset(const std::string& name) {
    for (const auto& p : rt::presets()) {
        if (p.id == name) return true;
    }
    return false;
}

bool loadOpticsScene(const std::string& spec, optics::Scene& scene, std::string& error) {
    if (optics::makePreset(spec, scene)) return true;
    if (fileExists(spec)) return optics::Scene::loadFromFile(spec, scene, error);
    error = "'" + spec + "' is neither a 2D demo name nor a readable scene file (try: raytracer list)";
    return false;
}

void progress(const char* what, double fraction, double seconds) {
    std::fprintf(stderr, "\r[%s] %5.1f%%  %6.1fs", what, fraction * 100.0, seconds);
    std::fflush(stderr);
}

int fail(const std::string& message) {
    std::fprintf(stderr, "error: %s\n", message.c_str());
    return 1;
}

int commandList() {
    std::printf("2D optics demos (raytracer --scene NAME, raytracer render2d --scene NAME):\n");
    for (const auto& p : optics::presets()) std::printf("  %-12s %s\n", p.id.c_str(), p.description.c_str());
    std::printf("\n3D path tracer scenes (raytracer --3d --scene NAME, raytracer render3d --scene NAME):\n");
    for (const auto& p : rt::presets()) std::printf("  %-12s %s\n", p.id.c_str(), p.description.c_str());
    return 0;
}

int commandRender2d(const Args& args) {
    std::string error;
    optics::Scene scene;
    if (!loadOpticsScene(valueOr(args, "--scene", "dispersion"), scene, error)) return fail(error);

    double width = 1600, height = 0, rays = 2e7, ev = 0, threads = 0;
    if (!numberArg(args, "--width", width, error) || !numberArg(args, "--height", height, error) ||
        !numberArg(args, "--rays", rays, error) || !numberArg(args, "--ev", ev, error) ||
        !numberArg(args, "--threads", threads, error)) {
        return fail(error);
    }
    if (height <= 0) height = std::round(width * scene.height() / scene.width());
    if (width < 1 || height < 1 || width > 16384 || height > 16384) return fail("image size out of range");
    const std::string output = valueOr(args, "--output", timestampedFileName("optics", ".png"));

    ThreadPool pool(static_cast<std::size_t>(threads));
    optics::LightTracer tracer;
    const int w = static_cast<int>(width), h = static_cast<int>(height);
    tracer.configure(w, h, optics::ViewTransform::fit(scene.bounds(), w, h, 1.0f));
    const auto total = static_cast<std::size_t>(rays);
    constexpr std::size_t kBatch = 500'000;
    Stopwatch clock;
    for (std::size_t done = 0; done < total;) {
        const std::size_t n = std::min(kBatch, total - done);
        tracer.trace(scene, pool, n);
        done += n;
        progress("render2d", static_cast<double>(done) / static_cast<double>(total), clock.seconds());
    }
    ImageF hdr;
    tracer.resolve(hdr);
    Image8 image = toneMap(hdr, autoExposure(hdr) * std::exp2(scene.exposure() + static_cast<float>(ev)));
    if (!args.flags.count("--no-overlay")) optics::drawOverlay(image, scene, tracer.view(), {});
    if (!writePng(output, image, &error)) return fail(error);
    std::fprintf(stderr, "\n%s: %dx%d, %s rays in %.1fs (%.2f Mrays/s) -> %s\n", scene.name().c_str(), w, h,
                 ui::formatCount(rays).c_str(), clock.seconds(), rays / clock.seconds() / 1e6, output.c_str());
    return 0;
}

int commandRender3d(const Args& args) {
    std::string error;
    const std::string name = valueOr(args, "--scene", "showcase");
    rt::Scene scene;
    if (!rt::makePreset(name, scene)) return fail("unknown 3D scene '" + name + "' (try: raytracer list)");

    double width = 1280, height = 720, spp = 256, bounces = 8, ev = 0, threads = 0;
    if (!numberArg(args, "--width", width, error) || !numberArg(args, "--height", height, error) ||
        !numberArg(args, "--spp", spp, error) || !numberArg(args, "--bounces", bounces, error) ||
        !numberArg(args, "--ev", ev, error) || !numberArg(args, "--threads", threads, error)) {
        return fail(error);
    }
    if (width < 1 || height < 1 || width > 16384 || height > 16384) return fail("image size out of range");
    if (spp < 1 || bounces < 1) return fail("--spp and --bounces must be positive");
    const std::string output = valueOr(args, "--output", timestampedFileName("render", ".png"));

    ThreadPool pool(static_cast<std::size_t>(threads));
    rt::PathTracer tracer;
    tracer.settings().maxDepth = static_cast<int>(bounces);
    tracer.configure(static_cast<int>(width), static_cast<int>(height), 1.0f);
    Stopwatch clock;
    const int total = static_cast<int>(spp);
    while (tracer.samples() < total) {
        tracer.renderPass(scene, scene.camera, pool, std::min(4, total - tracer.samples()));
        progress("render3d", static_cast<double>(tracer.samples()) / total, clock.seconds());
    }
    ImageF hdr;
    tracer.resolve(hdr);
    if (!writePng(output, toneMap(hdr, std::exp2(scene.exposure + static_cast<float>(ev))), &error)) return fail(error);
    std::fprintf(stderr, "\n%s: %.0fx%.0f, %d spp in %.1fs (%.2f Mpaths/s) -> %s\n", scene.name.c_str(), width, height,
                 total, clock.seconds(), static_cast<double>(tracer.pathsTraced()) / clock.seconds() / 1e6, output.c_str());
    return 0;
}

int commandBench(const Args& args) {
    std::string error;
    double seconds = 2.0, threads = 0;
    if (!numberArg(args, "--seconds", seconds, error) || !numberArg(args, "--threads", threads, error)) return fail(error);
    ThreadPool pool(static_cast<std::size_t>(threads));
    std::printf("Benchmark on %zu worker threads, %.1fs per scene\n\n", pool.size(), seconds);

    std::printf("3D path tracer (320x180):\n");
    for (const auto& p : rt::presets()) {
        rt::Scene scene;
        rt::makePreset(p.id, scene);
        rt::PathTracer tracer;
        tracer.configure(320, 180, 1.0f);
        Stopwatch clock;
        while (clock.seconds() < seconds) tracer.renderPass(scene, scene.camera, pool, 1);
        std::printf("  %-12s %8.2f Mpaths/s  (%d spp)\n", p.id.c_str(),
                    static_cast<double>(tracer.pathsTraced()) / clock.seconds() / 1e6, tracer.samples());
    }
    std::printf("\n2D light tracer (240x150):\n");
    for (const auto& p : optics::presets()) {
        optics::Scene scene;
        optics::makePreset(p.id, scene);
        optics::LightTracer tracer;
        tracer.configure(240, 150, optics::ViewTransform::fit(scene.bounds(), 240, 150, 1.0f));
        Stopwatch clock;
        while (clock.seconds() < seconds) tracer.trace(scene, pool, 100'000);
        std::printf("  %-12s %8.2f Mrays/s\n", p.id.c_str(), static_cast<double>(tracer.raysTraced()) / clock.seconds() / 1e6);
    }
    return 0;
}

int commandInteractive(const Args& args) {
    AppOptions options;
    std::string error;
    double threads = 0;
    if (!numberArg(args, "--threads", threads, error)) return fail(error);
    options.threads = static_cast<std::size_t>(threads);
    options.start3d = args.flags.count("--3d") > 0;
    if (args.flags.count("--ascii")) options.imageStyle = term::ImageStyle::Ascii;
    if (args.flags.count("--256")) options.colorMode = term::ColorMode::Palette256;

    std::string spec = valueOr(args, "--scene", "");
    if (!spec.empty() && is3dPreset(spec)) {
        options.start3d = true;
        options.preset3d = spec;
        spec.clear();
    }
    if (spec.empty()) {
        optics::makePreset("dispersion", options.opticsScene);
    } else {
        if (!loadOpticsScene(spec, options.opticsScene, error)) return fail(error);
        if (fileExists(spec)) options.opticsScenePath = spec;
    }
    App app(std::move(options));
    return app.run();
}

}  // namespace

int runCli(int argc, char** argv) {
    Args args;
    std::string error;
    if (!parseArgs(argc, argv, args, error)) {
        std::fprintf(stderr, "error: %s\n\n%s", error.c_str(), kUsage);
        return 2;
    }
    if (args.flags.count("--help") || args.command == "help") {
        std::printf("%s", kUsage);
        return 0;
    }
    if (args.flags.count("--version")) {
        std::printf("Console Ray Tracer %s\n", CRT_VERSION);
        return 0;
    }
    try {
        if (args.command == "list") return commandList();
        if (args.command == "render2d") return commandRender2d(args);
        if (args.command == "render3d") return commandRender3d(args);
        if (args.command == "bench") return commandBench(args);
        return commandInteractive(args);
    } catch (const std::exception& e) {
        return fail(e.what());
    }
}

}  // namespace crt::app
