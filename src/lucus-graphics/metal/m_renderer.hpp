#pragma once

#include "m_pch.hpp"

#include "renderer.hpp"
#include "m_device.hpp"

namespace lucus
{
    class window;
    
    class m_renderer : public renderer
    {
        public:
            m_renderer();
            ~m_renderer() override = default;

            virtual bool init() override;
            virtual bool prepare(std::shared_ptr<window> window) override;
            virtual void tick() override;
            virtual void cleanup() override;
        
        protected:
            void initDevices();

            void createMetalLayerAndView(std::shared_ptr<window> window);

            void createSynchObjects();
            void createPipeline();

            id<MTLLibrary> loadLibrary(const std::string& filename);
            id<MTLRenderPipelineState> createPipeline(id<MTLLibrary> library, MTLPixelFormat colorFormat);

            void prepareFrame();
            void buildCommandBuffer();
            void submitFrame();

        private:
            std::unique_ptr<m_device> _device;

            id<MTLDevice> deviceMTL;

            CAMetalLayer* _metalLayer = nil;

            id<MTLRenderPipelineState> _pipeline = nil;

            id<CAMetalDrawable> _currentDrawable = nil;
            id<MTLCommandBuffer> _currentBuffer = nil;

            dispatch_semaphore_t _frameSemaphore;
    };
}