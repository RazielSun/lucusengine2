#pragma once

#include "pch.hpp"
#include "core_types.hpp"

namespace lucus
{
    struct image_capture
    {
        u32 width{0};
        u32 height{0};
        std::vector<u8> rgba;

        image_capture() = default;
        image_capture(u32 width, u32 height);

        size_t byte_size() const;
        void copy_pixels(const void* pixels, size_t data_size, size_t row_pitch, bool bgra);
    };
}
