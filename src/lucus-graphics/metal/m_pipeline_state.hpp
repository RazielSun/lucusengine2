#pragma once

#include "m_pch.hpp"

namespace lucus
{
    struct m_pipeline_state_desc
    {
        MTLVertexDescriptor* vertexDescriptor = nullptr;
        MTLPixelFormat colorFormat;
        MTLPixelFormat depthFormat;
    };

    class m_pipeline_state
    {
        public:
            m_pipeline_state(id<MTLDevice> device);

            void init(const std::string& shaderName, const m_pipeline_state_desc& init_desc);
            void cleanup();

            id<MTLRenderPipelineState> getPipeline() { return _pipeline; }
            id<MTLDepthStencilState> getDepthStencilState() { return _depthStencilState; }
        
        protected:
            id<MTLLibrary> loadLibrary(const std::string& filename) const;

        private:
            id<MTLDevice> _device;

            id<MTLRenderPipelineState> _pipeline = nil;
            id<MTLDepthStencilState> _depthStencilState = nil;
    };
}