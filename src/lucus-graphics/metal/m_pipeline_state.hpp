#pragma once

#include "m_pch.hpp"

namespace lucus
{
    class material;
    
    class m_pipeline_state
    {
        public:
            m_pipeline_state(id<MTLDevice> device);
            ~m_pipeline_state();

            void init(material* mat, MTLPixelFormat colorFormat, MTLPixelFormat depthFormat);

            id<MTLRenderPipelineState> getPipeline() { return _pipeline; }
            id<MTLDepthStencilState> getDepthStencilState() { return _depthStencilState; }

            bool isUniformBufferUsed() const { return _useUniformBuffers; }
        
        protected:
            id<MTLLibrary> loadLibrary(const std::string& filename) const;

        private:
            id<MTLDevice> _device;

            id<MTLRenderPipelineState> _pipeline = nil;
            id<MTLDepthStencilState> _depthStencilState = nil;

            bool _useUniformBuffers{false};
    };
}