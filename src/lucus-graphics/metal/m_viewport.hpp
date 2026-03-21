#pragma once

#include "m_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class window;
    
    struct m_viewport
    {
        void init(id<MTLDevice> device, window* window);

        id<CAMetalDrawable> getNewDrawable();

        MTLPixelFormat getPixelFormat() const { return _metalLayer.pixelFormat; }

    protected:
        CAMetalLayer* _metalLayer = nil;
    };
}