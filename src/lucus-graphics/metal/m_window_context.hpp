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

        std::array<id<MTLTexture>, g_framesInFlight> depthTextures;
        MTLPixelFormat depthFormat = MTLPixelFormatDepth32Float;

        CGSize cachedDrawableSize{};

        id<CAMetalDrawable> currentDrawable = nil;
        // uint32_t currentBufferIndex{ 0 };

        dispatch_semaphore_t frameSemaphore;

        // m_buffer uniformbuffers;

        void init(id<MTLDevice> device, window* window);

        void wait_frame();

        id<CAMetalDrawable> getNewDrawable();
        CGSize getDrawableSize() const;

        MTLPixelFormat getPixelFormat() const { return metalLayer.pixelFormat; }
        MTLPixelFormat getDepthPixelFormat() const { return depthFormat; }
    };
}