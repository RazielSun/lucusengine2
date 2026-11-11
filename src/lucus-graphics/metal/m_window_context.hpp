#pragma once

#include "m_pch.hpp"

#include "render_types.hpp"
#include "m_buffer.hpp"

namespace lucus
{
    class window;
    
    struct m_window_context
    {
        CAMetalLayer* metalLayer = nil;

        render_target_handle color_handle;
        render_target_handle depth_handle;

        CGSize cachedDrawableSize{};

        id<CAMetalDrawable> currentDrawable = nil;

        dispatch_semaphore_t frameSemaphore;

        void init(id<MTLDevice> device, window* window);

        void wait_frame();

        id<CAMetalDrawable> getNewDrawable();
        CGSize getDrawableSize() const;

        MTLPixelFormat getPixelFormat() const { return metalLayer.pixelFormat; }
    };
}