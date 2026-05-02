#include "dx_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "command_buffer.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "render_types.hpp"

#include "dx_utils.hpp"

namespace lucus
{
    std::shared_ptr<dynamic_rhi> create_dynamic_rhi()
    {
        return std::make_shared<dx_dynamic_rhi>();
    }
}

using namespace lucus;

dx_dynamic_rhi::dx_dynamic_rhi()
{
}

dx_dynamic_rhi::~dx_dynamic_rhi()
{
    fence.Reset();
    if (fenceEvent) {
        CloseHandle((HANDLE)fenceEvent);
        fenceEvent = nullptr;
    }

    commandBuffer.Reset();
    commandPool.cleanup();

    for (auto& buffer : _uniformBuffers) {
        buffer.cleanup();
    }
    _uniformBuffers.clear();

    _pipelineStates.clear();
    _textures.clear();
    _samplers.clear();

    srvHeapDesc.cleanup();
    samplerHeapDesc.cleanup();

    for (auto& context : _contexts) {
        context.cleanup();
    }
    _contexts.clear();

    _commandQueue.Reset();
    _deviceHandle.Reset();
    _device.reset();
    _DXGIFactory.Reset();
}

void dx_dynamic_rhi::init()
{
    createInstance();
    createDevice();
    createSyncObjects();
    createCommandBufferPool();
    createDescriptorHeaps();
}

window_context_handle dx_dynamic_rhi::createWindowContext(const window_handle& handle) 
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to dx_swapchain::init");
	}

    auto& context = _contexts.emplace_back();
    context.init(_DXGIFactory, _deviceHandle, _commandQueue, mColorFormat, window);

    window_context_handle out_handle(static_cast<uint32_t>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    context.color_handle = createRenderTarget(context.width, context.height, render_target_type::COLOR, context.imageCount, true);
    context.init_images(_renderTargets[context.color_handle.as_index()]);

    context.depth_handle = createRenderTarget(context.width, context.height, render_target_type::DEPTH, context.imageCount);

    return out_handle;
}

void dx_dynamic_rhi::getWindowContextSize(const window_context_handle& ctx_handle, u32& width, u32& height) const
{
    width = 0, height = 0;
    if (!ctx_handle.is_valid())
    {
        return;
    }

    const auto& ctx = _contexts[ctx_handle.as_index()];
    width = ctx.width;
    height = ctx.height;
}

render_target_handle lucus::dx_dynamic_rhi::getWindowContextRenderTarget(const window_context_handle& handle, render_target_type rt_type) const
{
    if (!handle.is_valid())
        return render_target_handle();
    const auto& ctx = _contexts[handle.as_index()];
    return (rt_type == render_target_type::DEPTH) ? ctx.depth_handle : ctx.color_handle;
}
            
void dx_dynamic_rhi::execute(const window_context_handle& ctx_handle, u32 frameIndex, const gpu_command_buffer& cmd)
{
    if (!ctx_handle.is_valid())
    {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;
    // uint32_t bufferIndex = ctx.backBufferIndex % g_framesInFlight;

    // Here or in beginFrame?
    ThrowIfFailed(commandPool.commandAllocators[currentFrame]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(commandBuffer->Reset(commandPool.commandAllocators[currentFrame].Get(), nullptr), "Failed Reset Command Buffer");

    srvHeapDesc.reset_allocator(frameIndex);
    samplerHeapDesc.reset_allocator(frameIndex);
    
    if (!commandBuffer)
        throw std::runtime_error("Command buffer is null");

    for (u32 i = 0; i < cmd.commandCount; ++i)
    {
        const u8 *data = cmd.data.data() + i * COMMAND_FIXED_SIZE;
        const gpu_command_base* gpu_cmd = reinterpret_cast<const gpu_command_base*>(data);
        switch (gpu_cmd->type)
        {
            case gpu_command_type::RENDER_PASS_BEGIN:
                {
                    const auto* rb_cmd = reinterpret_cast<const gpu_render_pass_begin_command*>(data);

                    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[g_maxMrtColorTargets]{};
                    for (u32 i = 0; i < rb_cmd->colorTargetCount; ++i)
                    {
                        assert(rb_cmd->colorTargets[i].is_valid());
                        auto& rt = _renderTargets[rb_cmd->colorTargets[i].as_index()];
                        const u32 idx = rt.bSwapChain ? ctx.backBufferIndex : currentFrame;
                        rtvs[i] = rt.color.getResourceHandle(idx);
                    }

                    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
                    D3D12_CPU_DESCRIPTOR_HANDLE* dsv = nullptr;
                    if (rb_cmd->depthTargetCount > 0)
                    {
                        assert(rb_cmd->depthTarget.is_valid());
                        auto& rt = _renderTargets[rb_cmd->depthTarget.as_index()];
                        const u32 idx = rt.bSwapChain ? ctx.backBufferIndex : currentFrame;
                        dsvHandle = rt.depth.getResourceHandle(idx);
                        dsv = &dsvHandle;
                    }

                    commandBuffer->OMSetRenderTargets(rb_cmd->colorTargetCount,
                        rb_cmd->colorTargetCount > 0 ? rtvs : nullptr, FALSE, dsv);

                    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
                    for (u32 i = 0; i < rb_cmd->colorTargetCount; ++i)
                        commandBuffer->ClearRenderTargetView(rtvs[i], clearColor, 0, nullptr);

                    if (dsv)
                        commandBuffer->ClearDepthStencilView(*dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                }
                break;
            case gpu_command_type::RENDER_PASS_END:
                {
                    // commandBuffer->EndRenderPass();
                }
                break;
            case gpu_command_type::SET_VIEWPORT:
                {
                    const auto* vp_cmd = reinterpret_cast<const gpu_set_viewport_command*>(data);
                    D3D12_VIEWPORT viewport{};
                    viewport.TopLeftX = float(vp_cmd->x);
                    viewport.TopLeftY = float(vp_cmd->y);
                    viewport.Width = float(vp_cmd->width);
                    viewport.Height = float(vp_cmd->height);
                    viewport.MinDepth = 0.f;
                    viewport.MaxDepth = 1.f;
                    commandBuffer->RSSetViewports(1, &viewport);
                }
                break;
            case gpu_command_type::SET_SCISSOR:
                {
                    const auto* sc_cmd = reinterpret_cast<const gpu_set_scissor_command*>(data);
                    D3D12_RECT scissor{};
                    scissor.left = sc_cmd->offset_x;
                    scissor.top = sc_cmd->offset_y;
                    scissor.right = sc_cmd->extent_x;
                    scissor.bottom = sc_cmd->extent_y;
                    commandBuffer->RSSetScissorRects(1, &scissor);
                }
                break;
            case gpu_command_type::IMAGE_BARRIER:
                {
                    const auto* ib_cmd = reinterpret_cast<const gpu_image_barrier_command*>(data);
                    auto rt_handle = ib_cmd->rt_handle;
                    assert(rt_handle.is_valid());

                    auto& rt = _renderTargets[rt_handle.as_index()];
                    const u32 index = rt.bSwapChain ? ctx.backBufferIndex : currentFrame;

                    dx_images* images = nullptr;
                    if (ib_cmd->aspect == image_barrier_aspect::COLOR)
                    {
                        assert(rt.bColor);
                        images = &rt.color;
                    }
                    else if (ib_cmd->aspect == image_barrier_aspect::DEPTH)
                    {
                        assert(rt.bDepth);
                        images = &rt.depth;
                    }

                    assert(images);
                    assert(index < images->images.size());
                    assert(index < images->states.size());

                    D3D12_RESOURCE_STATES before = images->states[index];
                    if (ib_cmd->src != resource_state::UNDEFINED)
                    {
                        before = utils::to_dx12_resource_state(ib_cmd->src);
                    }

                    const D3D12_RESOURCE_STATES after = utils::to_dx12_resource_state(ib_cmd->dst);
                    if (before != after)
                    {
                        D3D12_RESOURCE_BARRIER barrier{};
                        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                        barrier.Transition.pResource = images->images[index].Get();
                        barrier.Transition.StateBefore = before;
                        barrier.Transition.StateAfter = after;
                        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                        commandBuffer->ResourceBarrier(1, &barrier);
                    }

                    images->states[index] = after;
                }
                break;
            case gpu_command_type::BIND_PIPELINE:
                {
                    const auto* bp_cmd = reinterpret_cast<const gpu_bind_pipeline_command*>(data);
                    auto found = _pipelineStates.find(bp_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    commandBuffer->SetGraphicsRootSignature(pso.getRootSignature().Get());
                    commandBuffer->SetPipelineState(pso.getPipeline().Get());
                }
                break;
            case gpu_command_type::BIND_UNIFORM_BUFFER:
                {
                    const auto* bu_cmd = reinterpret_cast<const gpu_bind_uniform_buffer_command*>(data);
                    auto& ub = _uniformBuffers[bu_cmd->ub_handle.as_index()];
                    commandBuffer->SetGraphicsRootConstantBufferView((u32)bu_cmd->position, ub.get(currentFrame)->GetGPUVirtualAddress());
                }
                break;
            case gpu_command_type::BIND_TEXTURE:
                {
                    const auto* bt_cmd = reinterpret_cast<const gpu_bind_texture_command*>(data);
                    auto found_tex = _textures.find(bt_cmd->tex_handle.get());
                    assert(found_tex != _textures.end());
                    auto& tex = found_tex->second;
                    srvHeapDesc.copy(tex.index, frameIndex);
                }
                break;
            case gpu_command_type::BIND_SAMPLER:
                {
                    const auto* bs_cmd = reinterpret_cast<const gpu_bind_sampler_command*>(data);
                    auto& smpl = _samplers[bs_cmd->smpl_handle.as_index()];
                    samplerHeapDesc.copy(smpl.index, frameIndex);
                }
                break;
            case gpu_command_type::BIND_RENDER_TARGET:
                {
                    const auto* br_cmd = reinterpret_cast<const gpu_bind_render_target_command*>(data);
                    auto rt_handle = br_cmd->rt_handle;
                    assert(rt_handle.is_valid());

                    auto& rt = _renderTargets[rt_handle.as_index()];
                    const u32 index = rt.bSwapChain ? ctx.backBufferIndex : currentFrame;

                    const dx_images* images = nullptr;
                    if (br_cmd->type == render_target_type::COLOR)
                    {
                        assert(rt.bColor);
                        images = &rt.color;
                    }
                    else if (br_cmd->type == render_target_type::DEPTH)
                    {
                        assert(rt.bDepth);
                        images = &rt.depth;
                    }

                    assert(images);
                    assert(images->bShaderRead);
                    assert(index < images->images.size());
                    srvHeapDesc.copy(images->getShaderHandle(index), frameIndex);
                }
                break;
            case gpu_command_type::BIND_DESCRIPTION_TABLE:
                {
                    const auto* bd_cmd = reinterpret_cast<const gpu_bind_description_table_command*>(data);
                    ID3D12DescriptorHeap* heaps[] = { srvHeapDesc.shaderHeap.Get(), samplerHeapDesc.shaderHeap.Get() };
                    commandBuffer->SetDescriptorHeaps(2, heaps);

                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::TEXTURE,            srvHeapDesc.getShaderHeadHandle(frameIndex, 0));
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::SHADOW_MAP,         srvHeapDesc.getShaderHeadHandle(frameIndex, 1));
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::SAMPLER,            samplerHeapDesc.getShaderHeadHandle(frameIndex, 0));
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::SHADOW_MAP_SAMPLER, samplerHeapDesc.getShaderHeadHandle(frameIndex, 1));
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::BINDLESS,           srvHeapDesc.getShaderHeadHandle(frameIndex, 0));
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::BINDLESS_SAMPLER,   samplerHeapDesc.getShaderHeadHandle(frameIndex, 0));
                    if (bd_cmd->pass == render_pass_name::DEFERRED_LIGHTING_PASS)
                    {
                        commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::GBUFFER_A,          srvHeapDesc.getShaderHeadHandle(frameIndex, 2));
                        commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::GBUFFER_B,          srvHeapDesc.getShaderHeadHandle(frameIndex, 3));
                        commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::GBUFFER_C,          srvHeapDesc.getShaderHeadHandle(frameIndex, 4));
                        commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::GBUFFER_DEPTH,      srvHeapDesc.getShaderHeadHandle(frameIndex, 5));
                        commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::GBUFFER_SAMPLER,    samplerHeapDesc.getShaderHeadHandle(frameIndex, 2));
                    }
                    srvHeapDesc.head_to_current(frameIndex);
                    samplerHeapDesc.head_to_current(frameIndex);
                }
                break;
            case gpu_command_type::SET_BINDLESS:
                {
                    (void)reinterpret_cast<const gpu_set_bindless_command*>(data);
                    ID3D12DescriptorHeap* heaps[] = { srvHeapDesc.shaderHeap.Get(), samplerHeapDesc.shaderHeap.Get() };
                    commandBuffer->SetDescriptorHeaps(2, heaps);
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::BINDLESS,         srvHeapDesc.getShaderHeadHandle(frameIndex, 0));
                    commandBuffer->SetGraphicsRootDescriptorTable((u32)shader_binding::BINDLESS_SAMPLER, samplerHeapDesc.getShaderHeadHandle(frameIndex, 0));
                }
                break;
            case gpu_command_type::BIND_VERTEX_BUFFER:
                {
                    const auto* bv_cmd = reinterpret_cast<const gpu_bind_vertex_command*>(data);
                    auto found = _meshes.find(bv_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    commandBuffer->IASetVertexBuffers(0, 1, &gpuMesh.vbView);
                }
                break;
            case gpu_command_type::DRAW_INDEXED:
                {
                    const auto* di_cmd = reinterpret_cast<const gpu_draw_indexed_command*>(data);
                    auto found = _meshes.find(di_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    commandBuffer->IASetIndexBuffer(&gpuMesh.ibView);
                    commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    commandBuffer->DrawIndexedInstanced(di_cmd->indexCount, 1, 0, 0, 0);
                }
                break;
            case gpu_command_type::DRAW_VERTEX:
                {
                    const auto* dv_cmd = reinterpret_cast<const gpu_draw_vertex_command*>(data);
                    commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    commandBuffer->DrawInstanced(dv_cmd->vertexCount, 1, 0, 0);
                }
                break;
            default:
                {
                    std::printf("execute cmd doesn't handled %d\n", (u32)gpu_cmd->type);
                }
                break;
        }
    }

    ThrowIfFailed(commandBuffer->Close(), "Failed Close Command Buffer");

    ID3D12CommandList* command_lists[] = { commandBuffer.Get() };
    _commandQueue->ExecuteCommandLists(1, command_lists);

    ThrowIfFailed(ctx.swapChain->Present(1, 0), "Failed SwapChain Preset");

    const uint64_t current_fence = fenceValues[currentFrame];

    ThrowIfFailed(_commandQueue->Signal(fence.Get(), current_fence), "Failed Queue Signal");

    ctx.backBufferIndex = ctx.swapChain->GetCurrentBackBufferIndex();

    if (fence->GetCompletedValue() < fenceValues[currentFrame])
    {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[currentFrame], (HANDLE)fenceEvent), "Failed Set Event OnCompletion");
        WaitForSingleObject((HANDLE)fenceEvent, INFINITE);
    }

    fenceValues[currentFrame] = current_fence + 1;
}

pipeline_state_handle dx_dynamic_rhi::createPSO(material* mat, render_pass_name passName)
{
    assert(mat);

    pipeline_state_handle pso_handle = pipeline_state_handle(mat->getHash());
    auto it = _pipelineStates.find(pso_handle.get());
    if (it != _pipelineStates.end()) {
        return pso_handle;
    }

    {
        auto pair = _pipelineStates.emplace(pso_handle.get(), _deviceHandle);
        assert(pair.second);

        dx_pipeline_state& pso = pair.first->second;

        dx_pipeline_state_desc init_desc;

        init_desc.depthFormat = mDepthFormat;
        init_desc.pass = passName;

        // deferred_lighting reads gFrame (b0) in the pixel shader only; other passes use it in VS.
        const D3D12_SHADER_VISIBILITY viewCbvVisibility =
            (passName == render_pass_name::DEFERRED_LIGHTING_PASS)
                ? D3D12_SHADER_VISIBILITY_ALL
                : D3D12_SHADER_VISIBILITY_VERTEX;
        init_desc.layouts.emplace_back().InitAsConstantBufferView(
            (u32)shader_binding::VIEW, // b0
            0,
            D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
            viewCbvVisibility);

        init_desc.layouts.emplace_back().InitAsConstantBufferView(
            (u32)shader_binding::OBJECT, // b1
            0,
            D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
            D3D12_SHADER_VISIBILITY_VERTEX);

        init_desc.layouts.emplace_back().InitAsConstantBufferView(
            (u32)shader_binding::MATERIAL, // b2
            0,
            D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
            D3D12_SHADER_VISIBILITY_ALL);

        init_desc.layouts.emplace_back().InitAsConstantBufferView(
            (u32)shader_binding::LIGHT, // b3
            0,
            D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
            D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_DESCRIPTOR_RANGE1 ranges[11];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // t0 TEXTURE
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // t1 SHADOW_MAP
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // s0 SAMPLER
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // s1 SHADOW_MAP_SAMPLER
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // t3 GBUFFER_A
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     1, 4, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // t4 GBUFFER_B
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     1, 5, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // t5 GBUFFER_C
        ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,     1, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // t6 GBUFFER_DEPTH
        ranges[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE); // s2 GBUFFER_SAMPLER
        ranges[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(g_maxTexturesCount), 7, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // t7 BINDLESS
        ranges[10].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, static_cast<UINT>(g_maxSamplersCount), 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // s3 BINDLESS_SAMPLER

        init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL); // TEXTURE
        init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL); // SHADOW_MAP
        init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL); // SAMPLER
        init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL); // SHADOW_MAP_SAMPLER
        init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[9], D3D12_SHADER_VISIBILITY_PIXEL); // BINDLESS
        init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[10], D3D12_SHADER_VISIBILITY_PIXEL); // BINDLESS_SAMPLER
        if (passName == render_pass_name::DEFERRED_LIGHTING_PASS)
        {
            init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL); // GBUFFER_A
            init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL); // GBUFFER_B
            init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[6], D3D12_SHADER_VISIBILITY_PIXEL); // GBUFFER_C
            init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[7], D3D12_SHADER_VISIBILITY_PIXEL); // GBUFFER_DEPTH
            init_desc.layouts.emplace_back().InitAsDescriptorTable(1, &ranges[8], D3D12_SHADER_VISIBILITY_PIXEL); // GBUFFER_SAMPLER
        }

        if (mat->useVertexIndexBuffers())
        {
            init_desc.vertexLayouts.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
            init_desc.vertexLayouts.push_back({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
            init_desc.vertexLayouts.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertex, texCoords), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
            init_desc.vertexLayouts.push_back({ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
        }
        const std::string& shaderName = mat->getShaderName();
        pso.init(shaderName, init_desc);

        std::printf("PSO %u created successfully\n", pso_handle.get());
    }

    return pso_handle;
}

render_target_handle dx_dynamic_rhi::createRenderTarget(u32 width, u32 height, render_target_type rt_type, u32 count, bool skip_resources)
{
    auto& rt = _renderTargets.emplace_back();
    render_target_handle rt_handle(static_cast<u32>(_renderTargets.size()));

    if (skip_resources)
    {
        rt.bSwapChain = true;
        rt.bColor = true;
        std::printf("Render Target %u created (skip_resources)\n", rt_handle.get());
        return rt_handle;
    }

    const bool bUseColor = (rt_type == render_target_type::COLOR);
    const bool bUseDepth = (rt_type == render_target_type::DEPTH);
    const bool bShaderRead = true;

    u32 srvColorIndex = 0, srvDepthIndex = 0;
    if (bUseColor)
        srvColorIndex = srvHeapDesc.allocateResource(count);
    if (bUseDepth)
        srvDepthIndex = srvHeapDesc.allocateResource(count);

    dx_render_target_desc init_desc {
        .count = count,
        .width = width,
        .height = height,
        .bSwapChain = false,
        .bUseColor = bUseColor,
        .colorFormat = mColorFormat,
        .bColorShaderRead = bShaderRead,
        .ColorSRVHeadIndex = srvColorIndex,
        .bUseDepth = bUseDepth,
        .depthFormat = mDepthFormat,
        .bDepthShaderRead = bShaderRead,
        .DepthSRVHeadIndex = srvDepthIndex,
        .resourceHeap = srvHeapDesc.resourceHeap,
    };
    rt.init(_deviceHandle, init_desc);

    std::printf("Render Target %u created successfully\n", rt_handle.get());

    return rt_handle;
}

mesh_handle dx_dynamic_rhi::createMesh(mesh* msh)
{
    assert(msh != nullptr);
    u32 meshHash = msh->getHash();

    auto it = _meshes.find(meshHash);
    if (it != _meshes.end()) {
        return mesh_handle(it->first);
    }

    auto pair = _meshes.emplace(meshHash, dx_mesh());
    assert(pair.second);

    dx_mesh& dxmsh = pair.first->second;

    dxmsh.init(_deviceHandle, msh);

    std::printf("Mesh %u created successfully\n", meshHash);

    return mesh_handle(meshHash);
}

sampler_handle dx_dynamic_rhi::createSampler(resource_binding_mode /*binding_mode*/)
{
    auto& smpl = _samplers.emplace_back();

    smpl.init(_deviceHandle, samplerHeapDesc.resourceHeap, samplerHeapDesc.allocateResource());

    sampler_handle smpl_handle(static_cast<u32>(_samplers.size()));
    std::printf("Sampler %d created successfully\n", smpl_handle.get());

    return smpl_handle;
}
            
texture_handle dx_dynamic_rhi::createTexture(texture* tex)
{
    assert(tex);
    u32 texHash = tex->getHash();

    auto it = _textures.find(texHash);
    if (it != _textures.end()) {
        return texture_handle(it->first);
    }

    auto pair = _textures.emplace(texHash, dx_texture());
    assert(pair.second);

    dx_texture& dxtex = pair.first->second;
    dxtex.init(_deviceHandle, srvHeapDesc.resourceHeap, srvHeapDesc.allocateResource(), tex);
    std::printf("Texture %u created successfully\n", texHash);

    return texture_handle(texHash);
}

void dx_dynamic_rhi::loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex)
{
    auto it = _textures.find(tex_handle.get());
    if (it == _textures.end()) {
        // error
        return;
    }

    dx_texture& dxtex = it->second;
    uploadTextureToGpu(dxtex, frameIndex);
    std::printf("Texture %u loaded successfully\n", tex_handle.get());
}

uniform_buffer_handle dx_dynamic_rhi::createUniformBuffer(size_t bufferSize, shader_binding_stage stage)
{
    dx_uniform_buffer& buffer = _uniformBuffers.emplace_back();
    buffer.init(_deviceHandle, bufferSize);

    uniform_buffer_handle ub_handle(static_cast<u32>(_uniformBuffers.size()));
    std::printf("Uniform buffer %d created successfully\n", ub_handle.get());

    return ub_handle;
}

void dx_dynamic_rhi::getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr)
{
    // std::printf("getUniformBufferMemory %d [Ctx: %d]\n", ub_handle.get(), ctx_handle.get());
    if (!ub_handle.is_valid())
    {
        throw std::runtime_error("failed to get uniform buffer memory! ub is null!");
    }

    auto& buffer = _uniformBuffers[ub_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;
    memory_ptr = buffer.getMappedData(currentFrame);
}

void dx_dynamic_rhi::createInstance()
{
    UINT createFactoryFlags = 0;
#ifndef NDEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&_DXGIFactory)), "Failed Create Factory");

    std::printf("IDXGIFactory4 created successfully\n");
}

void dx_dynamic_rhi::createDevice()
{
    #ifndef NDEBUG
	{
		// Enable debug layer interface before create dx device
		Com<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)), "Failed Get Debug Interface");
		debugController->EnableDebugLayer();

        Com<ID3D12Debug1> debugController1;
        ThrowIfFailed(debugController.As(&debugController1), "Failed As Debug Interface");
        debugController1->SetEnableGPUBasedValidation(TRUE);
	}
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    Com<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != _DXGIFactory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), featureLevel, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

    _device = std::make_unique<dx_device>(adapter.Get());
    _device->createLogicalDevice(featureLevel);

    _deviceHandle = _device->getDevice();

    // Here was an allocator of cmd pool

    // Enable debug messages in debug mode.
#ifndef NDEBUG
    Com<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(_deviceHandle.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};
 
        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };
 
        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };
 
        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;
 
        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter), "Failed Push Debug Info");
    }
#endif

    // Create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(_deviceHandle->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_commandQueue.ReleaseAndGetAddressOf())), "Failed Create Command Queue");
	_commandQueue->SetName(L"Command Queue");

    // _swapchain->setContext(mDXGIFactory, _commandQueue);

    std::printf("ID3D12CommandQueue created successfully\n");
}

// void dx_dynamic_rhi::wait_idle()
// {
//     ThrowIfFailed(
//         _commandQueue->Signal(_fence.Get(), _fenceValues[_currentFrame]),
//         "wait_for_gpu signal failed"
//     );

//     ThrowIfFailed(
//         _fence->SetEventOnCompletion(_fenceValues[_currentFrame], (HANDLE)_fenceEvent),
//         "wait_for_gpu SetEventOnCompletion failed"
//     );

//     WaitForSingleObject((HANDLE)_fenceEvent, INFINITE);
//     ++_fenceValues[_currentFrame];
// }

void dx_dynamic_rhi::createSyncObjects()
{
    // Create synchronization objects.
	ThrowIfFailed(_deviceHandle->CreateFence(fenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "Failed Create Fence");
	fenceValues[0]++;

	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), " Failed Create Event");
	}

    std::printf("ID3D12Fence created successfully\n");
}

void dx_dynamic_rhi::createCommandBufferPool()
{
    commandPool.init(_deviceHandle);

    // Create command list for recording graphics commands
	ThrowIfFailed(_deviceHandle->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT, commandPool.commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(commandBuffer.ReleaseAndGetAddressOf())),
        "Failed Create Command List"
    );
	ThrowIfFailed(commandBuffer->Close(), "Failed Close Command List");
	commandBuffer->SetName(L"Command List");

    std::printf("ID3D12GraphicsCommandList created successfully\n");
}

void dx_dynamic_rhi::createDescriptorHeaps()
{
    srvHeapDesc.init(_deviceHandle, g_maxTexturesCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    samplerHeapDesc.init(_deviceHandle, g_maxSamplersCount, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

void dx_dynamic_rhi::uploadTextureToGpu(dx_texture& tex, u32 frameIndex)
{
    u32 currentFrame = frameIndex % g_framesInFlight;

    // TODO: Upload several textures during the cmd

    ThrowIfFailed(commandPool.commandAllocators[currentFrame]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(commandBuffer->Reset(commandPool.commandAllocators[currentFrame].Get(), nullptr), "Failed Reset Command Buffer");
    
    // --- Copy buffer → texture ---
    D3D12_SUBRESOURCE_DATA subresource{};
    subresource.pData = tex.texLink->getResource();
    subresource.RowPitch = tex.width * tex.bytesPerPixel;
    subresource.SlicePitch = subresource.RowPitch * tex.height;

    UpdateSubresources(
        commandBuffer.Get(),
        tex.texResource.Get(),
        tex.stgBuffer.Get(),
        0,
        0,
        1,
        &subresource
    );

    // --- Barrier: COPY → SHADER READ ---
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            tex.texResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

    commandBuffer->ResourceBarrier(1, &barrier);

    // --- Submit ---
    commandBuffer->Close();

    ID3D12CommandList* lists[] = { commandBuffer.Get() };
    _commandQueue->ExecuteCommandLists(1, lists);

    // --- Fence wait (sync) ---
    fenceValues[currentFrame]++;
    _commandQueue->Signal(fence.Get(), fenceValues[currentFrame]);

    if (fence->GetCompletedValue() < fenceValues[currentFrame])
    {
        fence->SetEventOnCompletion(fenceValues[currentFrame], fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // --- Cleanup ---
    tex.free_staging();
}
