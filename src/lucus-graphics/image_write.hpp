#pragma once

#include "image_capture.hpp"

namespace lucus
{
    void write_png(const std::string& file_path, const image_capture& image);
}
