#pragma once

#include "m_pch.hpp"

namespace lucus
{
    class m_pipeline_state
    {
        public:
            m_pipeline_state(MTLDevice device);
            ~m_pipeline_state();

            void init(const std::string& shaderName, MTLPixelFormat colorFormat);

            id<MTLRenderPipelineState> getPipeline() { return _pipeline; }
        
        protected:
            id<MTLLibrary> loadLibrary(const std::string& filename) const;

        private:
            id<MTLDevice> _device;

            id<MTLRenderPipelineState> _pipeline = nil;
    };
}