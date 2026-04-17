#pragma once

#include "dx_pch.hpp"

namespace lucus
{
    struct dx_pipeline_state_desc
    {
        DXGI_FORMAT depthFormat;

        std::vector<CD3DX12_ROOT_PARAMETER1> layouts;
        std::vector<D3D12_INPUT_ELEMENT_DESC> vertexLayouts;
    };

    class dx_pipeline_state
    {
    public:
        dx_pipeline_state(Com<ID3D12Device> device);
        ~dx_pipeline_state();

        void init(const std::string& shaderName, const dx_pipeline_state_desc& init_desc);

        Com<ID3D12PipelineState> getPipeline() { return _pipelineState; }
        Com<ID3D12RootSignature> getRootSignature() { return _rootSignature; }
    
    protected:

        void createRootSignature(const dx_pipeline_state_desc& init_desc);

    private:
        Com<ID3D12Device> _device;
        Com<ID3D12RootSignature> _rootSignature;

        Com<ID3D12PipelineState> _pipelineState;
    };
}