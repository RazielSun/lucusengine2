#include "dx_pipeline_state.hpp"

#include "filesystem.hpp"

using namespace lucus;

dx_pipeline_state::dx_pipeline_state(Com<ID3D12Device> device, Com<ID3D12RootSignature> rootSignature)
    : _device(device), _rootSignature(rootSignature)
{
    //
}

dx_pipeline_state::~dx_pipeline_state()
{
    _pipelineState.Reset();
    _rootSignature.Reset();
    _device.Reset();
}

void dx_pipeline_state::init(const std::string& shaderName)
{
//     UINT compile_flags = 0;
// #ifndef NDEBUG
//     compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
// #endif

//     Com<ID3DBlob> vs;
//     Com<ID3DBlob> ps;
//     Com<ID3DBlob> errors;

//     std::string shader_name = shaderName + ".hlsl";
//     auto shader_code = filesystem::instance().read_shader(shader_name);

//     ThrowIfFailed(D3DCompile(
//             shader_code.data(),
//             shader_code.size(),
//             shader_name.c_str(),
//             nullptr,
//             D3D_COMPILE_STANDARD_FILE_INCLUDE,
//             "VSMain",
//             "vs_5_0",
//             compile_flags,
//             0,
//             &vs,
//             &errors
//         ),
//         "Failed Compile VS Shader"
//     );

//     ThrowIfFailed(D3DCompile(
//             shader_code.data(),
//             shader_code.size(),
//             shader_name.c_str(),
//             nullptr,
//             D3D_COMPILE_STANDARD_FILE_INCLUDE,
//             "PSMain",
//             "ps_5_0",
//             compile_flags,
//             0,
//             &ps,
//             &errors
//         ),
//         "Failed Compile PS Shader"
//     );

    auto vs = filesystem::instance().read_shader(shaderName + ".vert.dxil");
    auto ps = filesystem::instance().read_shader(shaderName + ".frag.dxil");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = _rootSignature.Get();
    // pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    // pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.VS.pShaderBytecode = vs.data();
    pso.VS.BytecodeLength = vs.size();
    pso.PS.pShaderBytecode = ps.data();
    pso.PS.BytecodeLength = ps.size();

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