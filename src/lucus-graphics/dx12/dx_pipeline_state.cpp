#include "dx_pipeline_state.hpp"

#include "filesystem.hpp"
#include "material.hpp"

using namespace lucus;

dx_pipeline_state::dx_pipeline_state(Com<ID3D12Device> device)
    : _device(device)
{
    //
}

dx_pipeline_state::~dx_pipeline_state()
{
    _pipelineState.Reset();
    _rootSignature.Reset();
    _device.Reset();
}

void dx_pipeline_state::init(const material* mat, uint32_t layoutCount)
{
    assert(mat);
    const std::string& shaderName = mat->getShaderName();

    _useUniformBuffers = mat->isUseUniformBuffers();

    auto vs = filesystem::instance().read_shader(shaderName + ".vert.dxil");
    auto ps = filesystem::instance().read_shader(shaderName + ".frag.dxil");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    
    pso.VS.pShaderBytecode = vs.data();
    pso.VS.BytecodeLength = vs.size();
    pso.PS.pShaderBytecode = ps.data();
    pso.PS.BytecodeLength = ps.size();

    createRootSignature(layoutCount);
    pso.pRootSignature = _rootSignature.Get();

    D3D12_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    // rasterizer.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE; // Only for Triangle?
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    pso.RasterizerState = rasterizer;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC default_rt_blend =
    {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };

    for (int i = 0; i < 8; ++i)
        blend.RenderTarget[i] = default_rt_blend;
    
    pso.BlendState = blend;

    D3D12_DEPTH_STENCIL_DESC depth_stencil{};
    depth_stencil.DepthEnable = FALSE;
    depth_stencil.StencilEnable = FALSE;

    pso.DepthStencilState = depth_stencil;

    pso.SampleMask = UINT_MAX;
    pso.InputLayout = { nullptr, 0 };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pipelineState)), "Failed Create Graphics Pipeline State");

    std::printf("ID3D12PipelineState created successfully\n");
}

void dx_pipeline_state::createRootSignature(uint32_t layoutCount)
{
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    if (layoutCount > 0)
    {
        CD3DX12_ROOT_PARAMETER1 rootParameters[2];

        rootParameters[0].InitAsConstantBufferView(
            0, // b0
            0, // register space
            D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
            D3D12_SHADER_VISIBILITY_VERTEX);

        rootParameters[1].InitAsConstantBufferView(
            1, // b1
            0, // register space
            D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
            D3D12_SHADER_VISIBILITY_VERTEX);
        
        desc.Desc_1_1.NumParameters = 2;
        desc.Desc_1_1.pParameters = rootParameters;
    }
    else
    {
        desc.Desc_1_1.NumParameters = 0;
        desc.Desc_1_1.pParameters = nullptr;
    }
    
    // TODO
    desc.Desc_1_1.NumStaticSamplers = 0;
    desc.Desc_1_1.pStaticSamplers = nullptr;

    Com<ID3DBlob> signature;
    Com<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeVersionedRootSignature(
            &desc,
            &signature,
            &error
        ),
        "Failed Serialize Root Signature"
    );

    ThrowIfFailed(_device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&_rootSignature)
        ),
        "Failed Create Root Signature"
    );

    std::printf("ID3D12RootSignature created successfully\n");
}