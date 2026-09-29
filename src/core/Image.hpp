#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Color.hpp"

namespace crt {

/// Simple row-major 2D pixel container.
template <class Pixel>
class Image {
public:
    Image() = default;
    Image(int width, int height, Pixel fill = {}) { resize(width, height, fill); }

    void resize(int width, int height, Pixel fill = {}) {
        width_ = width > 0 ? width : 0;
        height_ = height > 0 ? height : 0;
        pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), fill);
    }

    void fill(Pixel value) { std::fill(pixels_.begin(), pixels_.end(), value); }

    int width() const { return width_; }
    int height() const { return height_; }
    bool empty() const { return pixels_.empty(); }
    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

    Pixel& at(int x, int y) {
        assert(contains(x, y));
        return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
    }
    const Pixel& at(int x, int y) const {
        assert(contains(x, y));
        return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
    }

    Pixel* data() { return pixels_.data(); }
    const Pixel* data() const { return pixels_.data(); }
    std::size_t size() const { return pixels_.size(); }

    Pixel* row(int y) { return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_); }
    const Pixel* row(int y) const {
        return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_);
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Pixel> pixels_;
};

using ImageF = Image<Rgb>;
using Image8 = Image<Rgb8>;

enum class ToneMapper { Linear, Reinhard, Aces };

/// Maps a single HDR value to [0,1] display-linear range.
float toneMapChannel(float v, ToneMapper op);

/// Applies `scale` (exposure), a tone curve and the sRGB transfer function.
Image8 toneMap(const ImageF& hdr, float scale, ToneMapper op = ToneMapper::Aces);
Rgb8 toneMapPixel(const Rgb& hdr, float scale, ToneMapper op = ToneMapper::Aces);

/// Automatic exposure: maps the log-average luminance of the lit pixels to `key` (Reinhard's
/// photographic "key value"), but never lets the 99th percentile exceed `maxHighlight`, so thin
/// bright features such as laser beams do not blow out. Returns 1 for a black image.
float autoExposure(const ImageF& hdr, float key = 0.3f, float maxHighlight = 6.0f);

/// Writes an 8-bit RGB PNG (deflate-compressed, adaptive scanline filters). Returns false on I/O error.
bool writePng(const std::string& path, const Image8& image, std::string* error = nullptr);

/// Encodes an 8-bit RGB PNG into memory.
std::vector<unsigned char> encodePng(const Image8& image);

/// zlib-wrapped deflate stream (LZ77 + fixed Huffman). Exposed for testing.
std::vector<unsigned char> zlibCompress(const std::vector<unsigned char>& data);

std::uint32_t crc32(const unsigned char* data, std::size_t size, std::uint32_t crc = 0);

}  // namespace crt
