#pragma once

#include "m_pch.hpp"

#include "dynamic_rhi.hpp"
#include "render_types.hpp"
#include "m_device.hpp"

namespace lucus
{
    struct m_viewport;
    struct m_pipeline_state;
    
    class m_dynamic_rhi : public dynamic_rhi
    {
        public:
            m_dynamic_rhi();
            virtual ~m_dynamic_rhi() override;

            virtual void init() override;

            virtual viewport_handle createViewport(const window_handle& handle) override;

            virtual void beginFrame(const viewport_handle& viewport) override;
            virtual void endFrame() override;

            virtual void submit(const command_buffer& cmd) override;

            virtual material_handle createMaterial(material* mat) override;
        
        protected:
            void createDevice();
            void createSyncObjectsStable();

        private:
            std::unique_ptr<m_device> _device;
            id<MTLDevice> _deviceHandle;

            std::vector<m_viewport> _viewports;

            viewport_handle _currentViewport;
            id<CAMetalDrawable> _currentDrawable = nil;
            id<MTLCommandBuffer> _currentBuffer = nil;

            dispatch_semaphore_t _frameSemaphore;

            std::unordered_map<uint32_t, m_pipeline_state> _pipelineStates;
    };
}