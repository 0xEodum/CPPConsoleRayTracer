#include "core/Image.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>

namespace crt {

// ---------------------------------------------------------------------------------------------
// Tone mapping
// ---------------------------------------------------------------------------------------------

float toneMapChannel(float v, ToneMapper op) {
    if (!(v > 0.0f)) return 0.0f;  // also catches NaN
    switch (op) {
        case ToneMapper::Linear:
            return saturate(v);
        case ToneMapper::Reinhard:
            return v / (1.0f + v);
        case ToneMapper::Aces: {
            // Narkowicz 2015 fit of the ACES filmic reference rendering transform.
            const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
            return saturate((v * (a * v + b)) / (v * (c * v + d) + e));
        }
    }
    return saturate(v);
}

Rgb8 toneMapPixel(const Rgb& hdr, float scale, ToneMapper op) {
    return {toByte(srgbEncode(toneMapChannel(hdr.r * scale, op))),
            toByte(srgbEncode(toneMapChannel(hdr.g * scale, op))),
            toByte(srgbEncode(toneMapChannel(hdr.b * scale, op)))};
}

Image8 toneMap(const ImageF& hdr, float scale, ToneMapper op) {
    Image8 out(hdr.width(), hdr.height());
    for (std::size_t i = 0; i < hdr.size(); ++i) out.data()[i] = toneMapPixel(hdr.data()[i], scale, op);
    return out;
}

float autoExposure(const ImageF& hdr, float key, float maxHighlight) {
    // Subsample large images: a few tens of thousands of samples pin the statistics well enough.
    const std::size_t stride = std::max<std::size_t>(1, hdr.size() / 40000);
    std::vector<float> lum;
    lum.reserve(hdr.size() / stride + 1);
    double logSum = 0.0;
    for (std::size_t i = 0; i < hdr.size(); i += stride) {
        const float l = hdr.data()[i].luminance();
        if (l > 1e-7f) {
            lum.push_back(l);
            logSum += std::log(static_cast<double>(l));
        }
    }
    if (lum.empty()) return 1.0f;
    const auto logAverage = static_cast<float>(std::exp(logSum / static_cast<double>(lum.size())));
    const auto k = static_cast<std::size_t>(0.99 * static_cast<double>(lum.size() - 1));
    std::nth_element(lum.begin(), lum.begin() + static_cast<std::ptrdiff_t>(k), lum.end());
    return std::min(key / logAverage, maxHighlight / lum[k]);
}

// ---------------------------------------------------------------------------------------------
// Checksums
// ---------------------------------------------------------------------------------------------

std::uint32_t crc32(const unsigned char* data, std::size_t size, std::uint32_t crc) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    return ~crc;
}

namespace {

std::uint32_t adler32(const std::vector<unsigned char>& data) {
    std::uint32_t a = 1, b = 0;
    for (unsigned char byte : data) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

// ---------------------------------------------------------------------------------------------
// Deflate: greedy LZ77 with hash chains + the fixed Huffman code (RFC 1951, section 3.2.6)
// ---------------------------------------------------------------------------------------------

class BitWriter {
public:
    explicit BitWriter(std::vector<unsigned char>& out) : out_(out) {}

    void bits(std::uint32_t value, int count) {  // LSB-first
        buffer_ |= static_cast<std::uint64_t>(value) << bitCount_;
        bitCount_ += count;
        while (bitCount_ >= 8) {
            out_.push_back(static_cast<unsigned char>(buffer_ & 0xffu));
            buffer_ >>= 8;
            bitCount_ -= 8;
        }
    }

    void huffman(std::uint32_t code, int length) {  // Huffman codes are stored MSB-first
        std::uint32_t reversed = 0;
        for (int i = 0; i < length; ++i) reversed |= ((code >> i) & 1u) << (length - 1 - i);
        bits(reversed, length);
    }

    void flush() {
        if (bitCount_ > 0) out_.push_back(static_cast<unsigned char>(buffer_ & 0xffu));
        buffer_ = 0;
        bitCount_ = 0;
    }

private:
    std::vector<unsigned char>& out_;
    std::uint64_t buffer_ = 0;
    int bitCount_ = 0;
};

constexpr std::array<std::uint16_t, 29> kLengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                                       15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                                       67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistBase = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                                     33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                                     1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                     6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void writeLiteral(BitWriter& w, unsigned symbol) {
    if (symbol < 144) w.huffman(0x30 + symbol, 8);
    else if (symbol < 256) w.huffman(0x190 + (symbol - 144), 9);
    else if (symbol < 280) w.huffman(symbol - 256, 7);
    else w.huffman(0xc0 + (symbol - 280), 8);
}

void writeMatch(BitWriter& w, unsigned length, unsigned dist) {
    unsigned li = 28;
    while (kLengthBase[li] > length) --li;
    writeLiteral(w, 257 + li);
    if (kLengthExtra[li] != 0) w.bits(length - kLengthBase[li], kLengthExtra[li]);

    unsigned di = 29;
    while (kDistBase[di] > dist) --di;
    w.huffman(di, 5);
    if (kDistExtra[di] != 0) w.bits(dist - kDistBase[di], kDistExtra[di]);
}

}  // namespace

std::vector<unsigned char> zlibCompress(const std::vector<unsigned char>& data) {
    constexpr std::size_t kWindow = 32768;
    constexpr int kHashBits = 15;
    constexpr std::size_t kMinMatch = 3;
    constexpr std::size_t kMaxMatch = 258;
    constexpr int kMaxChain = 48;

    std::vector<unsigned char> out;
    out.reserve(data.size() / 2 + 64);
    out.push_back(0x78);  // CMF: deflate, 32K window
    out.push_back(0x01);  // FLG: fastest, (0x7801 % 31 == 0)

    BitWriter w(out);
    w.bits(1, 1);  // BFINAL
    w.bits(1, 2);  // BTYPE = 01, fixed Huffman

    std::vector<std::int32_t> head(std::size_t{1} << kHashBits, -1);
    std::vector<std::int32_t> prev(kWindow, -1);
    auto hashAt = [&](std::size_t i) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) | data[i + 2];
        return (v * 2654435761u) >> (32 - kHashBits);
    };
    auto insert = [&](std::size_t i) {
        if (i + kMinMatch > data.size()) return;
        const auto h = hashAt(i);
        prev[i % kWindow] = head[h];
        head[h] = static_cast<std::int32_t>(i);
    };

    std::size_t i = 0;
    while (i < data.size()) {
        std::size_t bestLen = 0;
        std::size_t bestDist = 0;
        if (i + kMinMatch <= data.size()) {
            std::int32_t candidate = head[hashAt(i)];
            const std::size_t maxLen = std::min(kMaxMatch, data.size() - i);
            for (int chain = 0; candidate >= 0 && chain < kMaxChain; ++chain) {
                const auto c = static_cast<std::size_t>(candidate);
                if (i - c > kWindow - 1) break;
                std::size_t len = 0;
                while (len < maxLen && data[c + len] == data[i + len]) ++len;
                if (len > bestLen) {
                    bestLen = len;
                    bestDist = i - c;
                    if (len == maxLen) break;
                }
                const std::int32_t next = prev[c % kWindow];
                if (next >= candidate) break;  // stale entry from an overwritten window slot
                candidate = next;
            }
        }

        if (bestLen >= kMinMatch) {
            writeMatch(w, static_cast<unsigned>(bestLen), static_cast<unsigned>(bestDist));
            for (std::size_t k = 0; k < bestLen; ++k) insert(i + k);
            i += bestLen;
        } else {
            writeLiteral(w, data[i]);
            insert(i);
            ++i;
        }
    }
    writeLiteral(w, 256);  // end of block
    w.flush();

    const std::uint32_t adler = adler32(data);
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<unsigned char>((adler >> shift) & 0xffu));
    return out;
}

// ---------------------------------------------------------------------------------------------
// PNG
// ---------------------------------------------------------------------------------------------

namespace {

void putU32(std::vector<unsigned char>& out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<unsigned char>((v >> shift) & 0xffu));
}

void putChunk(std::vector<unsigned char>& out, const char* type, const std::vector<unsigned char>& payload) {
    putU32(out, static_cast<std::uint32_t>(payload.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), payload.begin(), payload.end());
    putU32(out, crc32(out.data() + start, out.size() - start));
}

unsigned char paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<unsigned char>(a);
    return static_cast<unsigned char>(pb <= pc ? b : c);
}

}  // namespace

std::vector<unsigned char> encodePng(const Image8& image) {
    const int w = image.width();
    const int h = image.height();
    const std::size_t stride = static_cast<std::size_t>(w) * 3;

    // Apply the per-row filter with the smallest sum of absolute residuals (libpng's heuristic).
    std::vector<unsigned char> raw;
    raw.reserve((stride + 1) * static_cast<std::size_t>(h));
    std::vector<unsigned char> prior(stride, 0), current(stride), candidate(stride), best(stride);
    for (int y = 0; y < h; ++y) {
        const Rgb8* src = image.row(y);
        for (int x = 0; x < w; ++x) {
            current[static_cast<std::size_t>(x) * 3 + 0] = src[x].r;
            current[static_cast<std::size_t>(x) * 3 + 1] = src[x].g;
            current[static_cast<std::size_t>(x) * 3 + 2] = src[x].b;
        }
        long bestScore = -1;
        unsigned char bestFilter = 0;
        for (unsigned char filter = 0; filter < 5; ++filter) {
            long score = 0;
            for (std::size_t i = 0; i < stride; ++i) {
                const int a = i >= 3 ? current[i - 3] : 0;
                const int b = prior[i];
                const int c = i >= 3 ? prior[i - 3] : 0;
                int predictor = 0;
                switch (filter) {
                    case 1: predictor = a; break;
                    case 2: predictor = b; break;
                    case 3: predictor = (a + b) / 2; break;
                    case 4: predictor = paeth(a, b, c); break;
                    default: break;
                }
                candidate[i] = static_cast<unsigned char>(current[i] - predictor);
                score += std::abs(static_cast<signed char>(candidate[i]));
            }
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                bestFilter = filter;
                best.swap(candidate);
            }
        }
        raw.push_back(bestFilter);
        raw.insert(raw.end(), best.begin(), best.end());
        prior.swap(current);
    }

    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    std::vector<unsigned char> header;
    putU32(header, static_cast<std::uint32_t>(w));
    putU32(header, static_cast<std::uint32_t>(h));
    header.insert(header.end(), {8, 2, 0, 0, 0});  // 8-bit, truecolour, deflate, adaptive filter, no interlace
    putChunk(png, "IHDR", header);
    putChunk(png, "IDAT", zlibCompress(raw));
    putChunk(png, "IEND", {});
    return png;
}

bool writePng(const std::string& path, const Image8& image, std::string* error) {
    const auto bytes = encodePng(image);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "cannot open '" + path + "' for writing";
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        if (error) *error = "failed to write '" + path + "'";
        return false;
    }
    return true;
}

}  // namespace crt
