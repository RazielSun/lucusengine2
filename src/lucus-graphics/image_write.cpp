#include "image_write.hpp"

#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace lucus;

void lucus::write_png(const std::string& file_path, const image_capture& image)
{
    if (file_path.empty() || image.rgba.size() != image.byte_size())
    {
        throw std::runtime_error("Invalid screenshot path or pixel data");
    }

    const std::string error = "Failed to write screenshot PNG: " + file_path;
    std::ofstream output(file_path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error(error);
    }
    const int encoded = stbi_write_png_to_func([](void* context, void* data, int size)
        {
            static_cast<std::ofstream*>(context)->write(static_cast<const char*>(data), size);
        }, &output, static_cast<int>(image.width), static_cast<int>(image.height),
        4, image.rgba.data(), static_cast<int>(image.width * 4));
    output.close();
    if (!encoded || !output)
    {
        throw std::runtime_error(error);
    }
}
