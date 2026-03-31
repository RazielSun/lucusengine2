#pragma once

#include "m_pch.hpp"

#include "dynamic_rhi.hpp"
#include "render_types.hpp"
#include "m_device.hpp"
#include "m_window_context.hpp"
#include "m_pipeline_state.hpp"

namespace lucus
{
    class m_dynamic_rhi : public dynamic_rhi
    {
        public:
            m_dynamic_rhi();
            virtual ~m_dynamic_rhi() override;

            virtual void init() override;

            virtual window_context_handle createWindowContext(const window_handle& handle) override;
            virtual const std::vector<window_context_handle>& getWindowContexts() const override;
            virtual float getWindowContextAspectRatio(const window_context_handle& handle) const override;

            virtual void beginFrame(const window_context_handle& handle) override;
            virtual void submit(const window_context_handle& handle, const command_buffer& cmd) override;
            virtual void endFrame(const window_context_handle& handle) override;

            virtual material_handle createMaterial(material* mat) override;

            virtual render_object_handle createUniformBuffer(render_object* obj) override;
        
        protected:
            void createDevice();

            void createObjectUniformBuffers();

        private:
            std::unique_ptr<m_device> _device;
            id<MTLDevice> _deviceHandle;

            std::vector<m_window_context> _contexts;
            std::vector<window_context_handle> _contextHandles;

            std::unordered_map<uint32_t, m_pipeline_state> _pipelineStates;

            std::array<id<MTLBuffer>, g_framesInFlight> _objectUniformBuffers;
    };
}