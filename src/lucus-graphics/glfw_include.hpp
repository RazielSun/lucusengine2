#pragma once

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
    #define GLFW_INCLUDE_VULKAN
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>