#include "image_capture_selftest.hpp"

#include <filesystem>

#include <stb_image.h>

#include "image_write.hpp"

using namespace lucus;

namespace
{
    struct temporary_directory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("lucus-capture-selftest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        temporary_directory()
        {
            if (!std::filesystem::create_directory(path))
            {
                throw std::runtime_error("Failed to create screenshot selftest directory");
            }
        }

        ~temporary_directory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
}

bool lucus::run_image_capture_selftest()
{
    u32 checks = 0;
    u32 failures = 0;
    const auto check = [&](bool success, const char* name)
    {
        ++checks;
        if (!success)
        {
            ++failures;
            std::fprintf(stderr, "[capture-selftest] FAIL %s\n", name);
        }
    };
    const auto rejects = [&](auto action, const char* name)
    {
        try
        {
            action();
            check(false, name);
        }
        catch (const std::exception&)
        {
            check(true, name);
        }
    };

    const std::array<u8, 20> padded{
        0, 0, 255, 0, 0, 255, 0, 17,
        99, 99, 99, 99,
        255, 0, 0, 33, 255, 255, 255, 44
    };
    const std::vector<u8> expected{
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255
    };
    image_capture image(2, 2);
    image.copy_pixels(padded.data(), padded.size(), 12, true);
    check(image.rgba == expected, "BGRA, row padding, top-left origin and opaque alpha");

    image_capture rgba(2, 2);
    rgba.copy_pixels(expected.data(), expected.size(), 8, false);
    check(rgba.rgba == expected, "packed RGBA");

    rejects([] { image_capture invalid(0, 2); }, "zero width rejected");
    rejects([] { image_capture invalid(2, 0); }, "zero height rejected");
    rejects([] { image_capture invalid(std::numeric_limits<u32>::max(), 2); }, "oversized width rejected");
    rejects([] { image_capture invalid(2, std::numeric_limits<u32>::max()); }, "oversized image rejected");
    rejects([&] { image.copy_pixels(nullptr, 20, 12, true); }, "null pixels rejected");
    rejects([&] { image.copy_pixels(padded.data(), 20, 0, true); }, "zero pitch rejected");
    rejects([&] { image.copy_pixels(padded.data(), 20, 7, true); }, "short pitch rejected");
    rejects([&] { image.copy_pixels(padded.data(), 19, 12, true); }, "truncated final row rejected");
    rejects([&] { image.copy_pixels(padded.data(), 20, std::numeric_limits<size_t>::max(), true); },
        "overflowing pitch rejected");
    check(image.rgba == expected, "invalid input preserves pixels");

    temporary_directory directory;
    const std::string path = (directory.path / "frame.png").string();
    write_png(path, image);
    int width = 0, height = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load(path.c_str(), &width, &height, &channels, 4), &stbi_image_free);
    check(decoded && width == 2 && height == 2 && channels == 4, "PNG dimensions and channels");
    check(decoded && width == 2 && height == 2 &&
        std::equal(expected.begin(), expected.end(), decoded.get()), "PNG pixel round trip");
    rejects([&] { write_png("", image); }, "empty path rejected");
    rejects([&] { write_png(directory.path.string(), image); }, "file write failure reported");
    if (std::filesystem::exists("/dev/full"))
    {
        rejects([&] { write_png("/dev/full", image); }, "late file write failure reported");
    }
    image.rgba.pop_back();
    rejects([&] { write_png(path, image); }, "truncated output pixels rejected");

    std::printf("[capture-selftest] %u checks, %u failures\n", checks, failures);
    return failures == 0;
}
