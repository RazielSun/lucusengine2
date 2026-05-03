#include "dx_pipeline_state.hpp"

#include "filesystem.hpp"

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

void dx_pipeline_state::init(const std::string& shaderName, const dx_pipeline_state_desc& init_desc)
{

    auto vs = filesystem::instance().read_shader(shaderName + ".vert.dxil");
    auto ps = filesystem::instance().read_shader(shaderName + ".frag.dxil");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    
    pso.VS.pShaderBytecode = vs.data();
    pso.VS.BytecodeLength = vs.size();
    pso.PS.pShaderBytecode = ps.data();
    pso.PS.BytecodeLength = ps.size();

    // TODO: Shared?
    createRootSignature(init_desc);
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
    depth_stencil.DepthEnable = TRUE;
    depth_stencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depth_stencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depth_stencil.StencilEnable = FALSE;

    pso.DepthStencilState = depth_stencil;
    pso.DSVFormat = init_desc.depthFormat;

    pso.SampleMask = UINT_MAX;

    if (init_desc.vertexLayouts.empty())
    {
        pso.InputLayout = { nullptr, 0 };
    }
    else
    {
        pso.InputLayout = { init_desc.vertexLayouts.data(), (u32)init_desc.vertexLayouts.size() };
    }

    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count = 1;

    switch (init_desc.pass)
    {
    case render_pass_name::SHADOW_PASS:
        pso.NumRenderTargets = 0;
        pso.DepthStencilState.DepthEnable = TRUE;
        break;
    case render_pass_name::GBUFFER_PASS:
        pso.NumRenderTargets = 3;
        pso.RTVFormats[0] = pso.RTVFormats[1] = pso.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso.DepthStencilState.DepthEnable = TRUE;
        break;
    case render_pass_name::DEFERRED_LIGHTING_PASS:
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
        break;
    default: // FORWARD_PASS
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso.DepthStencilState.DepthEnable = TRUE;
        break;
    }

    ThrowIfFailed(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_pipelineState)), "Failed Create Graphics Pipeline State");

    std::printf("ID3D12PipelineState created successfully\n");
}

void dx_pipeline_state::createRootSignature(const dx_pipeline_state_desc& init_desc)
{
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    desc.Desc_1_1.NumParameters = static_cast<uint32_t>(init_desc.layouts.size());
    desc.Desc_1_1.pParameters = init_desc.layouts.data();

    // Static samplers ?
    desc.Desc_1_1.NumStaticSamplers = 0;
    desc.Desc_1_1.pStaticSamplers = nullptr;
    
    Com<ID3DBlob> signature;
    Com<ID3DBlob> error;

    HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &signature, &error);
    if (FAILED(hr) && error && error->GetBufferPointer())
    {
        std::fprintf(stderr, "D3D12SerializeVersionedRootSignature: %s\n",
            static_cast<const char*>(error->GetBufferPointer()));
    }
    ThrowIfFailed(hr, "Failed Serialize Root Signature");

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
