#pragma once

#include "dx_pch.hpp"

namespace lucus
{
    class material;

    class dx_pipeline_state
    {
        public:
            dx_pipeline_state(Com<ID3D12Device> device);
            ~dx_pipeline_state();

            void init(const material* mat, DXGI_FORMAT depthFormat);

            Com<ID3D12PipelineState> getPipeline() { return _pipelineState; }
            Com<ID3D12RootSignature> getRootSignature() { return _rootSignature; }

            bool isUniformBufferUsed() const { return _useUniformBuffers; }
        
        protected:

            void createRootSignature(uint32_t layoutCount = 0, uint32_t texturesCount = 0);

        private:
            Com<ID3D12Device> _device;
            Com<ID3D12RootSignature> _rootSignature;

            Com<ID3D12PipelineState> _pipelineState;

            bool _useUniformBuffers{false};
    };
}