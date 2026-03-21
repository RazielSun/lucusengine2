#pragma once

#include "dx_pch.hpp"

namespace lucus
{
    class dx_pipeline_state
    {
        public:
            dx_pipeline_state(Com<ID3D12Device> device, Com<ID3D12RootSignature> rootSignature);
            ~dx_pipeline_state();

            void init(const std::string& shaderName);

            Com<ID3D12PipelineState> getPipeline() { return _pipelineState; }
        
        protected:

        private:
            Com<ID3D12Device> _device;
            Com<ID3D12RootSignature> _rootSignature;

            Com<ID3D12PipelineState> _pipelineState;
    };
}