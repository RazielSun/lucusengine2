#include "image_capture.hpp"

using namespace lucus;

image_capture::image_capture(u32 width, u32 height)
    : width(width), height(height), rgba(byte_size())
{
}

size_t image_capture::byte_size() const
{
    const size_t limit = static_cast<size_t>(std::numeric_limits<int>::max());
    if (width == 0 || height == 0 || width > limit / 4 ||
        height > limit / (static_cast<size_t>(width) * 4))
    {
        throw std::runtime_error("Invalid screenshot dimensions");
    }
    return static_cast<size_t>(width) * height * 4;
}

void image_capture::copy_pixels(const void* pixels, size_t data_size, size_t row_pitch, bool bgra)
{
    const size_t size = byte_size();
    const size_t row_bytes = static_cast<size_t>(width) * 4;
    if (!pixels || row_pitch < row_bytes || data_size < row_bytes ||
        height - 1 > (data_size - row_bytes) / row_pitch)
    {
        throw std::runtime_error("Invalid screenshot pixel buffer or row pitch");
    }

    rgba.resize(size);
    const auto* source = static_cast<const u8*>(pixels);
    for (u32 y = 0; y < height; ++y)
    {
        u8* row = rgba.data() + y * row_bytes;
        std::memcpy(row, source + y * row_pitch, row_bytes);
        for (size_t x = 0; x < row_bytes; x += 4)
        {
            if (bgra)
            {
                std::swap(row[x], row[x + 2]);
            }
            row[x + 3] = 255;
        }
    }
}
