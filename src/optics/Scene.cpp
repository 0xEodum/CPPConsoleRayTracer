#include "optics/Scene.hpp"

#include <algorithm>
#include <fstream>
#include <locale>
#include <sstream>

#include "optics/Material.hpp"

namespace crt::optics {

// ---------------------------------------------------------------------------------------------
// Object management
// ---------------------------------------------------------------------------------------------

Scene::Scene(const Scene& other)
    : name_(other.name_), exposure_(other.exposure_), width_(other.width_), height_(other.height_) {
    shapes_.reserve(other.shapes_.size());
    for (const auto& s : other.shapes_) shapes_.push_back(s->clone());
    lights_.reserve(other.lights_.size());
    for (const auto& l : other.lights_) lights_.push_back(l->clone());
}

Scene& Scene::operator=(const Scene& other) {
    if (this != &other) {
        Scene copy(other);
        *this = std::move(copy);
    }
    return *this;
}

Vec2 Scene::clampToBounds(Vec2 p) const { return {std::clamp(p.x, 0.0f, width_), std::clamp(p.y, 0.0f, height_)}; }

bool Scene::isValid(Selection s) const {
    if (s.isShape()) return s.index < shapes_.size();
    if (s.isLight()) return s.index < lights_.size();
    return false;
}

Shape* Scene::shape(Selection s) { return s.isShape() && s.index < shapes_.size() ? shapes_[s.index].get() : nullptr; }
const Shape* Scene::shape(Selection s) const {
    return s.isShape() && s.index < shapes_.size() ? shapes_[s.index].get() : nullptr;
}
Light* Scene::light(Selection s) { return s.isLight() && s.index < lights_.size() ? lights_[s.index].get() : nullptr; }
const Light* Scene::light(Selection s) const {
    return s.isLight() && s.index < lights_.size() ? lights_[s.index].get() : nullptr;
}

Selection Scene::add(std::unique_ptr<Shape> shape) {
    shapes_.push_back(std::move(shape));
    return Selection::shape(shapes_.size() - 1);
}

Selection Scene::add(std::unique_ptr<Light> light) {
    lights_.push_back(std::move(light));
    return Selection::light(lights_.size() - 1);
}

void Scene::remove(Selection s) {
    if (s.isShape() && s.index < shapes_.size()) shapes_.erase(shapes_.begin() + static_cast<std::ptrdiff_t>(s.index));
    if (s.isLight() && s.index < lights_.size()) lights_.erase(lights_.begin() + static_cast<std::ptrdiff_t>(s.index));
}

Selection Scene::duplicate(Selection s, Vec2 offset) {
    if (const Shape* src = shape(s)) {
        auto copy = src->clone();
        copy->setPosition(clampToBounds(copy->position() + offset));
        return add(std::move(copy));
    }
    if (const Light* src = light(s)) {
        auto copy = src->clone();
        copy->position = clampToBounds(copy->position + offset);
        return add(std::move(copy));
    }
    return Selection::none();
}

void Scene::clear() {
    shapes_.clear();
    lights_.clear();
}

Selection Scene::pick(Vec2 p, float tolerance) const {
    float best = tolerance;
    Selection result;
    for (std::size_t i = 0; i < lights_.size(); ++i) {
        const float d = distance(lights_[i]->position, p);
        if (d <= best) {
            best = d;
            result = Selection::light(i);
        }
    }
    if (result) return result;
    for (std::size_t i = shapes_.size(); i-- > 0;) {
        const Shape& s = *shapes_[i];
        const float d = s.distance(p);
        if (s.isSolid() ? d <= 0.0f : d <= tolerance) return Selection::shape(i);
    }
    // Nothing directly under the cursor: accept solid outlines within tolerance as well.
    for (std::size_t i = shapes_.size(); i-- > 0;) {
        if (shapes_[i]->distance(p) <= tolerance * 0.5f) return Selection::shape(i);
    }
    return result;
}

bool Scene::intersect(const Ray2& ray, float tMin, float tMax, SurfaceHit& hit) const {
    bool found = false;
    float closest = tMax;
    for (const auto& s : shapes_) {
        float t;
        Vec2 n;
        if (s->intersect(ray, tMin, closest, t, n)) {
            closest = t;
            hit.t = t;
            hit.normal = n;
            hit.shape = s.get();
            found = true;
        }
    }
    if (found) hit.point = ray.at(hit.t);
    return found;
}

float Scene::totalPower() const {
    float sum = 0.0f;
    for (const auto& l : lights_) sum += std::max(0.0f, l->power);
    return sum;
}

// ---------------------------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------------------------

namespace {

std::string formatNumber(float v) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss.precision(5);
    ss << v;
    return ss.str();
}

std::string formatVec(Vec2 v) { return formatNumber(v.x) + "," + formatNumber(v.y); }

bool parseNumber(std::string_view text, float& out) {
    std::istringstream ss{std::string(text)};
    ss.imbue(std::locale::classic());
    ss >> out;
    return !ss.fail() && ss.peek() == std::char_traits<char>::eof();
}

bool parseVec(std::string_view text, Vec2& out) {
    const auto comma = text.find(',');
    if (comma == std::string_view::npos) return false;
    return parseNumber(text.substr(0, comma), out.x) && parseNumber(text.substr(comma + 1), out.y);
}

/// Splits a line into whitespace-separated tokens; double quotes group words.
std::vector<std::string> tokenize(std::string_view line) {
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    bool any = false;
    for (char c : line) {
        if (c == '"') {
            quoted = !quoted;
            any = true;
        } else if (!quoted && (c == ' ' || c == '\t' || c == '\r')) {
            if (any) tokens.push_back(std::move(current));
            current.clear();
            any = false;
        } else {
            current += c;
            any = true;
        }
    }
    if (any) tokens.push_back(std::move(current));
    return tokens;
}

}  // namespace

std::string Scene::serialize() const {
    std::ostringstream out;
    out << "# Console Ray Tracer - 2D optics scene\n";
    out << "scene name=\"" << name_ << "\" width=" << formatNumber(width_) << " height=" << formatNumber(height_)
        << " exposure=" << formatNumber(exposure_) << "\n\n";
    for (const auto& s : shapes_) {
        out << "shape " << s->type() << " pos=" << formatVec(s->position())
            << " angle=" << formatNumber(degrees(s->angle())) << " material=" << material(s->material()).id;
        for (const Param& p : s->params()) out << ' ' << p.name << '=' << formatNumber(p.value);
        out << '\n';
    }
    for (const auto& l : lights_) {
        out << "light " << l->type() << " pos=" << formatVec(l->position) << " angle=" << formatNumber(degrees(l->angle))
            << " power=" << formatNumber(l->power) << " color=" << lightColor(l->color).id;
        for (const Param& p : l->params()) out << ' ' << p.name << '=' << formatNumber(p.value);
        out << '\n';
    }
    return out.str();
}

bool Scene::parse(std::string_view text, Scene& out, std::string& error) {
    Scene scene;
    int lineNumber = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view raw = text.substr(start, end - start);
        start = end + 1;
        ++lineNumber;

        const auto tokens = tokenize(raw.substr(0, raw.find('#')));
        if (tokens.empty()) continue;

        auto fail = [&](const std::string& message) {
            error = "line " + std::to_string(lineNumber) + ": " + message;
            return false;
        };

        const std::string& keyword = tokens[0];
        std::unique_ptr<Shape> shape;
        std::unique_ptr<Light> light;
        std::size_t first = 1;
        if (keyword == "shape" || keyword == "light") {
            if (tokens.size() < 2) return fail("missing object type");
            if (keyword == "shape") shape = makeShape(tokens[1]);
            else light = makeLight(tokens[1]);
            if (!shape && !light) return fail("unknown " + keyword + " type '" + tokens[1] + "'");
            first = 2;
        } else if (keyword != "scene") {
            return fail("unknown keyword '" + keyword + "'");
        }

        for (std::size_t i = first; i < tokens.size(); ++i) {
            const auto eq = tokens[i].find('=');
            if (eq == std::string::npos) return fail("expected key=value, got '" + tokens[i] + "'");
            const std::string key = tokens[i].substr(0, eq);
            const std::string value = tokens[i].substr(eq + 1);
            float number = 0.0f;
            Vec2 vec;
            const bool isNumber = parseNumber(value, number);

            if (keyword == "scene") {
                if (key == "name") scene.name_ = value;
                else if (key == "width" && isNumber && number > 1.0f) scene.width_ = number;
                else if (key == "height" && isNumber && number > 1.0f) scene.height_ = number;
                else if (key == "exposure" && isNumber) scene.exposure_ = std::clamp(number, -20.0f, 20.0f);
                else return fail("invalid scene property '" + tokens[i] + "'");
                continue;
            }

            if (key == "pos") {
                if (!parseVec(value, vec)) return fail("invalid position '" + value + "'");
                if (shape) shape->setPosition(vec);
                else light->position = vec;
            } else if (key == "angle" && isNumber) {
                if (shape) shape->setAngle(radians(number));
                else light->angle = radians(number);
            } else if (shape && key == "material") {
                const int m = findMaterial(value);
                if (m < 0) return fail("unknown material '" + value + "'");
                shape->setMaterial(m);
            } else if (light && key == "color") {
                const int c = findLightColor(value);
                if (c < 0) return fail("unknown light color '" + value + "'");
                light->color = c;
            } else if (light && key == "power" && isNumber) {
                light->power = std::max(0.0f, number);
            } else if (isNumber && (shape ? shape->setParam(key, number) : light->setParam(key, number))) {
                // dimension parameter applied
            } else {
                return fail("invalid property '" + tokens[i] + "'");
            }
        }

        if (shape) scene.shapes_.push_back(std::move(shape));
        if (light) scene.lights_.push_back(std::move(light));
    }
    out = std::move(scene);
    return true;
}

bool Scene::saveToFile(const std::string& path, std::string& error) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open '" + path + "' for writing";
        return false;
    }
    file << serialize();
    if (!file) {
        error = "failed to write '" + path + "'";
        return false;
    }
    return true;
}

bool Scene::loadFromFile(const std::string& path, Scene& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open '" + path + "'";
        return false;
    }
    std::ostringstream content;
    content << file.rdbuf();
    if (!parse(content.str(), out, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

}  // namespace crt::optics
