#include <atomic>
#include <cstring>

#include "TestFramework.hpp"
#include "core/Image.hpp"
#include "core/Random.hpp"
#include "core/Spectrum.hpp"
#include "core/ThreadPool.hpp"
#include "core/Vec2.hpp"
#include "core/Vec3.hpp"

using namespace crt;

namespace {

/// Minimal inflater for the subset of deflate our encoder produces (stored + fixed-Huffman
/// blocks). Used to verify that encodePng/zlibCompress output decodes to the original bytes.
class Inflater {
public:
    explicit Inflater(const std::vector<unsigned char>& data) : data_(data) {}

    bool zlib(std::vector<unsigned char>& out) {
        if (data_.size() < 6 || data_[0] != 0x78 || ((data_[0] << 8) | data_[1]) % 31 != 0) return false;
        pos_ = 2;
        bool last = false;
        while (!last) {
            last = bit() != 0;
            const unsigned type = bits(2);
            if (type == 0) {
                bitCount_ = 0;
                if (pos_ + 4 > data_.size()) return false;
                const unsigned len = data_[pos_] | (data_[pos_ + 1] << 8);
                pos_ += 4;
                out.insert(out.end(), data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                           data_.begin() + static_cast<std::ptrdiff_t>(pos_ + len));
                pos_ += len;
            } else if (type == 1) {
                if (!fixedBlock(out)) return false;
            } else {
                return false;
            }
        }
        return !failed_;
    }

private:
    unsigned bit() {
        if (bitCount_ == 0) {
            if (pos_ >= data_.size()) {
                failed_ = true;
                return 0;
            }
            current_ = data_[pos_++];
            bitCount_ = 8;
        }
        const unsigned b = current_ & 1u;
        current_ >>= 1;
        --bitCount_;
        return b;
    }
    unsigned bits(int n) {
        unsigned v = 0;
        for (int i = 0; i < n; ++i) v |= bit() << i;
        return v;
    }
    int literal() {
        unsigned code = 0;
        for (int len = 1; len <= 9 && !failed_; ++len) {
            code = (code << 1) | bit();
            if (len == 7 && code <= 0x17) return static_cast<int>(256 + code);
            if (len == 8 && code >= 0x30 && code <= 0xbf) return static_cast<int>(code - 0x30);
            if (len == 8 && code >= 0xc0 && code <= 0xc7) return static_cast<int>(280 + code - 0xc0);
            if (len == 9 && code >= 0x190) return static_cast<int>(144 + code - 0x190);
        }
        failed_ = true;
        return 256;
    }
    bool fixedBlock(std::vector<unsigned char>& out) {
        static const unsigned lengthBase[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                              31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const int lengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const unsigned distBase[] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                            193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const int distExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            const int sym = literal();
            if (failed_) return false;
            if (sym < 256) {
                out.push_back(static_cast<unsigned char>(sym));
                continue;
            }
            if (sym == 256) return true;
            const int li = sym - 257;
            if (li < 0 || li > 28) return false;
            const unsigned length = lengthBase[li] + bits(lengthExtra[li]);
            unsigned dcode = 0;
            for (int i = 0; i < 5; ++i) dcode = (dcode << 1) | bit();
            if (dcode > 29) return false;
            const unsigned dist = distBase[dcode] + bits(distExtra[dcode]);
            if (dist > out.size()) return false;
            for (unsigned i = 0; i < length; ++i) out.push_back(out[out.size() - dist]);
        }
    }

    const std::vector<unsigned char>& data_;
    std::size_t pos_ = 0;
    unsigned current_ = 0;
    int bitCount_ = 0;
    bool failed_ = false;
};

}  // namespace

TEST_CASE("vec2: arithmetic, rotation and reflection") {
    CHECK(Vec2(1, 2) + Vec2(3, 4) == Vec2(4, 6));
    CHECK_NEAR(dot(Vec2(1, 0), Vec2(0, 1)), 0.0, 1e-7);
    CHECK_NEAR(cross(Vec2(1, 0), Vec2(0, 1)), 1.0, 1e-7);
    const Vec2 r = rotate(Vec2(1, 0), kHalfPi);
    CHECK_NEAR(r.x, 0.0, 1e-6);
    CHECK_NEAR(r.y, 1.0, 1e-6);
    const Vec2 refl = reflect(normalize(Vec2(1, 1)), Vec2(0, -1));
    CHECK_NEAR(refl.x, std::sqrt(0.5), 1e-6);
    CHECK_NEAR(refl.y, -std::sqrt(0.5), 1e-6);
    CHECK_NEAR(wrapAngle(3.0f * kPi), kPi, 1e-5);
    CHECK_NEAR(wrapAngle(-kHalfPi), -kHalfPi, 1e-6);
}

TEST_CASE("vec3: cross product and orthonormal basis") {
    const Vec3 c = cross(Vec3(1, 0, 0), Vec3(0, 1, 0));
    CHECK_NEAR(c.z, 1.0, 1e-7);
    for (const Vec3& n : {Vec3(0, 0, 1), Vec3(0, 0, -1), normalize(Vec3(1, 2, 3)), normalize(Vec3(-3, 0.1f, -0.2f))}) {
        Vec3 t, b;
        orthonormalBasis(n, t, b);
        CHECK_NEAR(length(t), 1.0, 1e-5);
        CHECK_NEAR(length(b), 1.0, 1e-5);
        CHECK_NEAR(dot(t, n), 0.0, 1e-5);
        CHECK_NEAR(dot(b, n), 0.0, 1e-5);
        CHECK_NEAR(dot(t, b), 0.0, 1e-5);
    }
}

TEST_CASE("pcg32: range, determinism and mean") {
    Pcg32 a(42);
    double sum = 0.0;
    bool inRange = true;
    for (int i = 0; i < 100000; ++i) {
        const float u = a.uniform();
        inRange = inRange && u >= 0.0f && u < 1.0f;
        sum += u;
    }
    CHECK(inRange);
    CHECK_NEAR(sum / 100000.0, 0.5, 0.01);
    Pcg32 c(7), d(7);
    for (int i = 0; i < 100; ++i) CHECK(c.nextU32() == d.nextU32());
    for (int i = 0; i < 1000; ++i) CHECK(c.below(10) < 10u);
}

TEST_CASE("spectrum: equal-energy light is white") {
    Rgb sum;
    const int n = 4000;
    for (int i = 0; i < n; ++i) {
        const float l = spectrum::kMinWavelength + (spectrum::kMaxWavelength - spectrum::kMinWavelength) * (i + 0.5f) / n;
        sum += spectrum::wavelengthToRgb(l);
    }
    CHECK_NEAR(sum.r / n, 1.0, 0.01);
    CHECK_NEAR(sum.g / n, 1.0, 0.01);
    CHECK_NEAR(sum.b / n, 1.0, 0.01);
}

TEST_CASE("spectrum: hues of spectral colours") {
    const Rgb red = spectrum::wavelengthToRgb(650.0f);
    const Rgb green = spectrum::wavelengthToRgb(532.0f);
    const Rgb blue = spectrum::wavelengthToRgb(450.0f);
    CHECK(red.r > red.g && red.r > red.b);
    CHECK(green.g > green.r && green.g > green.b);
    CHECK(blue.b > blue.r && blue.b > blue.g);
    CHECK_NEAR(spectrum::reflectance(Rgb(1.0f), 500.0f), 1.0, 1e-5);
    CHECK(spectrum::reflectance({0.9f, 0.1f, 0.1f}, 650.0f) > 0.7f);
    CHECK(spectrum::reflectance({0.9f, 0.1f, 0.1f}, 450.0f) < 0.3f);
}

TEST_CASE("spectrum: dispersion and emission sampling") {
    CHECK(spectrum::cauchyIor(1.5f, 0.0042f, 400.0f) > spectrum::cauchyIor(1.5f, 0.0042f, 700.0f));
    const auto warm = spectrum::EmissionSpectrum::blackbody(3000.0f);
    const auto mono = spectrum::EmissionSpectrum::monochromatic(589.0f);
    double mean = 0.0;
    for (int i = 0; i < 1000; ++i) {
        const float l = warm.sample((i + 0.5f) / 1000.0f);
        CHECK(l >= spectrum::kMinWavelength && l <= spectrum::kMaxWavelength);
        mean += l;
    }
    CHECK(mean / 1000.0 > 600.0);  // a 3000K body is red-heavy
    CHECK_NEAR(mono.sample(0.3f), 589.0, 1e-4);
    const Rgb8 swatch = warm.swatch();
    CHECK(swatch.r >= swatch.b);
}

TEST_CASE("image: tone mapping is monotonic and bounded") {
    float previous = -1.0f;
    for (float v = 0.0f; v < 50.0f; v += 0.25f) {
        const float t = toneMapChannel(v, ToneMapper::Aces);
        CHECK(t >= previous);
        CHECK(t >= 0.0f && t <= 1.0f);
        previous = t;
    }
    CHECK(toneMapChannel(-1.0f, ToneMapper::Reinhard) == 0.0f);
    CHECK(toneMapChannel(std::nanf(""), ToneMapper::Aces) == 0.0f);
    const Rgb8 white = toneMapPixel(Rgb(1000.0f), 1.0f);
    CHECK(white.r == 255 && white.g == 255 && white.b == 255);
    CHECK(toSrgb8(Rgb(0.5f)).r == 188);  // sRGB transfer curve
}

TEST_CASE("image: auto exposure targets the log-average") {
    ImageF img(10, 10, Rgb(0.0f));
    for (int x = 0; x < 10; ++x) img.at(x, 5) = Rgb(2.0f);
    const float e = autoExposure(img, 0.5f, 100.0f);
    CHECK_NEAR(e, 0.25, 1e-4);
    ImageF black(4, 4, Rgb(0.0f));
    CHECK(autoExposure(black) == 1.0f);
}

TEST_CASE("image: crc32 reference value") {
    const char* text = "123456789";
    CHECK(crc32(reinterpret_cast<const unsigned char*>(text), std::strlen(text)) == 0xCBF43926u);
}

TEST_CASE("image: deflate round trip") {
    std::vector<unsigned char> data;
    Pcg32 rng(3);
    for (int i = 0; i < 70000; ++i) data.push_back(static_cast<unsigned char>(i % 7 == 0 ? rng.below(256) : (i / 13) % 251));
    const auto compressed = zlibCompress(data);
    CHECK(compressed.size() < data.size());
    std::vector<unsigned char> decoded;
    Inflater inflater(compressed);
    REQUIRE(inflater.zlib(decoded));
    CHECK(decoded == data);

    std::vector<unsigned char> empty;
    std::vector<unsigned char> decodedEmpty;
    const auto compressedEmpty = zlibCompress(empty);
    Inflater emptyInflater(compressedEmpty);
    CHECK(emptyInflater.zlib(decodedEmpty));
    CHECK(decodedEmpty.empty());
}

TEST_CASE("image: png structure") {
    Image8 img(7, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 7; ++x) img.at(x, y) = {static_cast<std::uint8_t>(x * 30), static_cast<std::uint8_t>(y * 50), 7};
    const auto png = encodePng(img);
    REQUIRE(png.size() > 57);
    CHECK(png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    CHECK(std::memcmp(png.data() + 12, "IHDR", 4) == 0);
    CHECK(png[19] == 7 && png[23] == 5);  // width / height (big endian)
    CHECK(std::memcmp(png.data() + png.size() - 8, "IEND", 4) == 0);

    // Decode the IDAT payload and undo the scanline filters.
    const std::size_t idatLength = (png[33] << 24) | (png[34] << 16) | (png[35] << 8) | png[36];
    REQUIRE(std::memcmp(png.data() + 37, "IDAT", 4) == 0);
    const std::vector<unsigned char> idat(png.begin() + 41, png.begin() + 41 + static_cast<std::ptrdiff_t>(idatLength));
    std::vector<unsigned char> raw;
    Inflater inflater(idat);
    REQUIRE(inflater.zlib(raw));
    REQUIRE(raw.size() == 5u * (1 + 7 * 3));
    std::vector<unsigned char> prior(21, 0);
    for (int y = 0; y < 5; ++y) {
        const unsigned char filter = raw[static_cast<std::size_t>(y) * 22];
        std::vector<unsigned char> row(raw.begin() + y * 22 + 1, raw.begin() + y * 22 + 22);
        for (std::size_t i = 0; i < 21; ++i) {
            const int a = i >= 3 ? row[i - 3] : 0, b = prior[i], c = i >= 3 ? prior[i - 3] : 0;
            int pred = 0;
            if (filter == 1) pred = a;
            if (filter == 2) pred = b;
            if (filter == 3) pred = (a + b) / 2;
            if (filter == 4) {
                const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
            }
            row[i] = static_cast<unsigned char>(row[i] + pred);
        }
        for (int x = 0; x < 7; ++x) {
            CHECK(row[static_cast<std::size_t>(x) * 3] == img.at(x, y).r);
            CHECK(row[static_cast<std::size_t>(x) * 3 + 1] == img.at(x, y).g);
        }
        prior = row;
    }
}

TEST_CASE("thread pool: every job runs exactly once") {
    ThreadPool pool(4);
    CHECK(pool.size() == 4);
    for (int round = 0; round < 20; ++round) {
        std::vector<std::atomic<int>> hits(1000);
        std::atomic<bool> badWorker{false};
        pool.parallelFor(hits.size(), [&](std::size_t i, std::size_t worker) {
            hits[i].fetch_add(1);
            if (worker >= pool.size()) badWorker = true;
        });
        bool once = true;
        for (auto& h : hits) once = once && h.load() == 1;
        CHECK(once);
        CHECK(!badWorker);
    }
    int calls = 0;
    pool.parallelFor(0, [&](std::size_t, std::size_t) { ++calls; });
    CHECK(calls == 0);
}
