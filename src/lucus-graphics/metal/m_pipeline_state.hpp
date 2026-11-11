#pragma once

#include "m_pch.hpp"

namespace lucus
{
    struct m_pipeline_state_desc
    {
        MTLVertexDescriptor* vertexDescriptor = nil;
        MTLPixelFormat colorFormat = MTLPixelFormatInvalid;
        MTLPixelFormat depthFormat = MTLPixelFormatInvalid;
    };

    class m_shader_library_cache
    {
        public:
            void init(id<MTLDevice> device);
            void cleanup();

            id<MTLLibrary> get(const std::string& shaderName);

        private:
            id<MTLLibrary> compile(const std::string& shaderName) const;

            id<MTLDevice> _device = nil;
            std::unordered_map<std::string, id<MTLLibrary>> _libraries;
    };

    class m_pipeline_state
    {
        public:
            m_pipeline_state(id<MTLDevice> device);

            void init(id<MTLLibrary> library, const m_pipeline_state_desc& init_desc);
            void cleanup();

            id<MTLRenderPipelineState> getPipeline() { return _pipeline; }
            id<MTLDepthStencilState> getDepthStencilState() { return _depthStencilState; }

        private:
            id<MTLDevice> _device;

            id<MTLRenderPipelineState> _pipeline = nil;
            id<MTLDepthStencilState> _depthStencilState = nil;
    };
}
