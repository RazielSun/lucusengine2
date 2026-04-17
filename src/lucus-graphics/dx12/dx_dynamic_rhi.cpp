#include "dx_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "command_buffer.hpp"
#include "material.hpp"
#include "mesh.hpp"

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
    _texFence.Reset();
    if (_texFenceEvent) {
        CloseHandle((HANDLE)_texFenceEvent);
        _texFenceEvent = nullptr;
    }

    for (auto& buffer : _uniformBuffers) {
        buffer.cleanup();
    }
    _uniformBuffers.clear();

    _pipelineStates.clear();

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
    initTextureCmdResources();
}

window_context_handle dx_dynamic_rhi::createWindowContext(const window_handle& handle) 
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to dx_swapchain::init");
	}

    dx_window_context context;
    context.init(_DXGIFactory, _deviceHandle, _commandQueue, window);

    _contexts.push_back(context);
    window_context_handle out_handle(static_cast<uint32_t>(_contexts.size()));
    _contextHandles.push_back(out_handle);

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
    ThrowIfFailed(ctx.commandPool.commandAllocators[currentFrame]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(ctx.commandBuffer->Reset(ctx.commandPool.commandAllocators[currentFrame].Get(), nullptr), "Failed Reset Command Buffer");
    
    if (!ctx.commandBuffer)
        throw std::runtime_error("Command buffer is null");

    if (!ctx.mRTVHeap)
        throw std::runtime_error("RTV heap is null");
    if (!ctx.mRenderTargets[ctx.backBufferIndex])
        throw std::runtime_error("Current render target is null");

    if (!ctx.mDSVHeap)
        throw std::runtime_error("DSV heap is null");
    if (!ctx.mDepthStencils[ctx.backBufferIndex])
        throw std::runtime_error("Depth stencil is null");

    D3D12_RESOURCE_BARRIER barrier_begin{};
    barrier_begin.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_begin.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_begin.Transition.pResource = ctx.mRenderTargets[ctx.backBufferIndex].Get();
    barrier_begin.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier_begin.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_begin.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ctx.commandBuffer->ResourceBarrier(1, &barrier_begin);

    for (u32 i = 0; i < cmd.commandCount; ++i)
    {
        const u8 *data = cmd.data.data() + i * COMMAND_FIXED_SIZE;
        const gpu_command_base* gpu_cmd = reinterpret_cast<const gpu_command_base*>(data);
        switch (gpu_cmd->type)
        {
            case gpu_command_type::RENDER_PASS_BEGIN:
                {
                    const auto* rb_cmd = reinterpret_cast<const gpu_render_pass_begin_command*>(data);

                    // Com<ID3D12GraphicsCommandList4> cmd4;
                    // ctx.commandBuffer.As(&cmd4);

                    D3D12_CPU_DESCRIPTOR_HANDLE rtv = ctx.mRTVHeap->GetCPUDescriptorHandleForHeapStart();
                    rtv.ptr += static_cast<SIZE_T>(ctx.backBufferIndex) * ctx.mRTVDescriptorSize;

                    // D3D12_RENDER_PASS_RENDER_TARGET_DESC colorDesc = {};
                    // colorDesc.cpuDescriptor = rtv;
                    // colorDesc.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                    // colorDesc.BeginningAccess.Clear.ClearValue.Color[0] = 0.f;
                    // colorDesc.BeginningAccess.Clear.ClearValue.Color[1] = 0.f;
                    // colorDesc.BeginningAccess.Clear.ClearValue.Color[2] = 0.f;
                    // colorDesc.BeginningAccess.Clear.ClearValue.Color[3] = 1.f;
                    // colorDesc.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

                    D3D12_CPU_DESCRIPTOR_HANDLE dsv = ctx.mDSVHeap->GetCPUDescriptorHandleForHeapStart();
                    dsv.ptr += static_cast<SIZE_T>(ctx.backBufferIndex) * ctx.mDSVDescriptorSize;

                    // D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthDesc = {};
                    // depthDesc.cpuDescriptor = dsv;
                    // depthDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                    // depthDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = 1.0f;
                    // depthDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

                    // ctx.commandBuffer->BeginRenderPass(1, &colorDesc, &depthDesc,D3D12_RENDER_PASS_FLAG_NONE);

                    const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

                    ctx.commandBuffer->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
                    ctx.commandBuffer->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
                    ctx.commandBuffer->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                }
                break;
            case gpu_command_type::RENDER_PASS_END:
                {
                    // ctx.commandBuffer->EndRenderPass();
                }
                break;
            case gpu_command_type::SET_VIEWPORT:
                {
                    const auto* vp_cmd = reinterpret_cast<const gpu_set_viewport_command*>(data);
                    D3D12_VIEWPORT viewport{};
                    viewport.TopLeftX = float(vp_cmd->x);
                    viewport.TopLeftY = float(vp_cmd->y);
                    viewport.Width = float(vp_cmd->width);
                    viewport.Height = vp_cmd->height);
                    viewport.MinDepth = 0.f;
                    viewport.MaxDepth = 1.f;
                    ctx.commandBuffer->RSSetViewports(1, &viewport);
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
                    ctx.commandBuffer->RSSetScissorRects(1, &scissor);
                }
                break;
            case gpu_command_type::BIND_PIPELINE:
                {
                    const auto* bp_cmd = reinterpret_cast<const gpu_bind_pipeline_command*>(data);
                    auto found = _pipelineStates.find(bp_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    auto& pso = found->second;
                    ctx.commandBuffer->SetGraphicsRootSignature(pso.getRootSignature().Get());
                    ctx.commandBuffer->SetPipelineState(pso.getPipeline().Get());
                }
                break;
            case gpu_command_type::BIND_UNIFORM_BUFFER:
                {
                    const auto* bu_cmd = reinterpret_cast<const gpu_bind_uniform_buffer_command*>(data);
                    // auto found = _pipelineStates.find(bu_cmd->pso_handle.get());
                    // auto& pso = found->second;
                    auto& ub = _uniformBuffers[bu_cmd->ub_handle.as_index()];
                    ctx.commandBuffer->SetGraphicsRootConstantBufferView((u32)bu_cmd->position, ub.get(currentFrame)->GetGPUVirtualAddress());
                }
                break;
            case gpu_command_type::BIND_TEXTURE:
                {
                    const auto* bt_cmd = reinterpret_cast<const gpu_bind_texture_command*>(data);
                    // auto found = _pipelineStates.find(bt_cmd->pso_handle.get());
                    // auto& pso = found->second;
                    auto found_tex = _textures.find(bt_cmd->tex_handle.get());
                    assert(found_tex != _textures.end());
                    auto& tex = found_tex->second;

                    ID3D12DescriptorHeap* heaps[] = { tex.srvHeap.Get(), tex.samplerHeap.Get() };
                    ctx.commandBuffer->SetDescriptorHeaps(2, heaps);

                    const u32 rootIndex = (u32)bt_cmd->position;
                    const u32 tex_slot = rootIndex - (u32)shader_binding::MATERIAL;

                    const UINT srvDescriptorSize = _deviceHandle->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = tex.srvHeap->GetGPUDescriptorHandleForHeapStart();
                    srvHandle.ptr += static_cast<UINT64>(tex_slot) * srvDescriptorSize;
                    
                    const UINT samplerDescriptorSize = _deviceHandle->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                    D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle = tex.samplerHeap->GetGPUDescriptorHandleForHeapStart();
                    samplerHandle.ptr += static_cast<UINT64>(tex_slot) * samplerDescriptorSize;
                    
                    ctx.commandBuffer->SetGraphicsRootDescriptorTable(rootIndex + 0, srvHandle);
                    ctx.commandBuffer->SetGraphicsRootDescriptorTable(rootIndex + 1, samplerHandle);
                }
                break;
            case gpu_command_type::BIND_VERTEX_BUFFER:
                {
                    const auto* bv_cmd = reinterpret_cast<const gpu_bind_vertex_command*>(data);
                    auto found = _meshes.find(bv_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    ctx.commandBuffer->IASetVertexBuffers(0, 1, &gpuMesh.vbView);
                }
                break;
            case gpu_command_type::DRAW_INDEXED:
                {
                    const auto* di_cmd = reinterpret_cast<const gpu_draw_indexed_command*>(data);
                    auto found = _meshes.find(di_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    ctx.commandBuffer->IASetIndexBuffer(&gpuMesh.ibView);
                    ctx.commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    ctx.commandBuffer->DrawIndexedInstanced(di_cmd->indexCount, 1, 0, 0, 0);
                }
                break;
            case gpu_command_type::DRAW_VERTEX:
                {
                    const auto* dv_cmd = reinterpret_cast<const gpu_draw_vertex_command*>(data);
                    ctx.commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    ctx.commandBuffer->DrawInstanced(dv_cmd->vertexCount, 1, 0, 0);
                }
                break;
            default:
                {
                    std::printf("execute cmd doesn't handled %d\n", (u32)gpu_cmd->type);
                }
                break;
        }
    }

    D3D12_RESOURCE_BARRIER barrier_end{};
    barrier_end.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_end.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_end.Transition.pResource = ctx.mRenderTargets[ctx.backBufferIndex].Get();
    barrier_end.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier_end.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier_end.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    ctx.commandBuffer->ResourceBarrier(1, &barrier_end);

    ThrowIfFailed(ctx.commandBuffer->Close(), "Failed Close Command Buffer");

    ID3D12CommandList* command_lists[] = { ctx.commandBuffer.Get() };
    _commandQueue->ExecuteCommandLists(1, command_lists);

    ThrowIfFailed(ctx.swapChain->Present(1, 0), "Failed SwapChain Preset");

    //

    const uint64_t current_fence = ctx.fenceValues[currentFrame];

    ThrowIfFailed(_commandQueue->Signal(ctx.fence.Get(), current_fence), "Failed Queue Signal");

    ctx.backBufferIndex = ctx.swapChain->GetCurrentBackBufferIndex();

    if (ctx.fence->GetCompletedValue() < ctx.fenceValues[currentFrame])
    {
        ThrowIfFailed(ctx.fence->SetEventOnCompletion(ctx.fenceValues[currentFrame], (HANDLE)ctx.fenceEvent), "Failed Set Event OnCompletion");
        WaitForSingleObject((HANDLE)ctx.fenceEvent, INFINITE);
    }

    ctx.fenceValues[currentFrame] = current_fence + 1;
}

pipeline_state_handle dx_dynamic_rhi::createPSO(material* mat)
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

        init_desc.depthFormat = _contexts[0].mDepthFormat;

        if (mat->useFrameUniformBuffer())
        {
            init_desc.layouts.emplace_back().InitAsConstantBufferView(
                (u32)shader_binding::FRAME, // b0
                0, // register space
                D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                D3D12_SHADER_VISIBILITY_VERTEX);
        }

        if (mat->useObjectUniformBuffer())
        {
            init_desc.layouts.emplace_back().InitAsConstantBufferView(
                (u32)shader_binding::OBJECT, // b1
                0, // register space
                D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                D3D12_SHADER_VISIBILITY_VERTEX);
        }

        if (mat->getTexturesCount() > 0)
        {
            CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
            ranges[0].Init(
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                1,
                0, // t0
                0,
                D3D12_DESCRIPTOR_RANGE_FLAG_NONE
            );

            ranges[1].Init(
                D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                1,
                0, // s0
                0,
                D3D12_DESCRIPTOR_RANGE_FLAG_NONE
            );
            // TODO:
            // texture (SRV table)
            init_desc.layouts.emplace_back().InitAsDescriptorTable(
                1,
                &ranges[0],
                D3D12_SHADER_VISIBILITY_PIXEL);

            // sampler table
            init_desc.layouts.emplace_back().InitAsDescriptorTable(
                1,
                &ranges[1],
                D3D12_SHADER_VISIBILITY_PIXEL);
        }

        if (mat->useVertexIndexBuffers())
        {
            init_desc.vertexLayouts.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
            init_desc.vertexLayouts.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertex, texCoords), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
            init_desc.vertexLayouts.push_back({ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
        }
        
        // TODO: depth format should be determined by window context or material properties, not hardcoded
        const std::string& shaderName = mat->getShaderName();
        pso.init(shaderName, init_desc);

        std::printf("PSO %u created successfully\n", pso_handle.get());
    }

    return pso_handle;
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

texture_handle dx_dynamic_rhi::loadTexture(texture* tex)
{
    assert(tex != nullptr);
    u32 texHash = tex->getHash();

    auto it = _textures.find(texHash);
    if (it != _textures.end()) {
        return texture_handle(it->first);
    }

    auto pair = _textures.emplace(texHash, dx_texture());
    assert(pair.second);

    dx_texture& dxtex = pair.first->second;

    dxtex.init(_deviceHandle, tex);

    uploadTextureToGpu(dxtex);
    
    std::printf("Texture %u loaded successfully\n", texHash);

    return texture_handle(texHash);
}

uniform_buffer_handle dx_dynamic_rhi::createUniformBuffer(uniform_buffer_type ub_type, size_t bufferSize)
{
    dx_uniform_buffer& buffer = _uniformBuffers.emplace_back();
    buffer.init(_deviceHandle, bufferSize);

    uniform_buffer_handle ub_handle(static_cast<u32>(_uniformBuffers.size()));
    std::printf("Uniform buffer %d [Type: %d] created successfully\n", ub_handle.get(), (u32)ub_type);

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

void dx_dynamic_rhi::initTextureCmdResources()
{
    // Create synchronization objects.

    ThrowIfFailed(_deviceHandle->CreateFence(_texFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_texFence)), "Failed Create TexFence");
	_texFenceValue++;

	_texFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_texFenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), " Failed Create Tex Fence Event");
	}

    ThrowIfFailed(_deviceHandle->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_texCmdAllocator.ReleaseAndGetAddressOf())), "Failed Create Tex Command Allocator");

    // Create command list for recording graphics commands
	ThrowIfFailed(_deviceHandle->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT, _texCmdAllocator.Get(), nullptr, IID_PPV_ARGS(_texCmdBuffer.ReleaseAndGetAddressOf())),
        "Failed Create Tex Command List"
    );
	ThrowIfFailed(_texCmdBuffer->Close(), "Failed Close Tex Command List");
	_texCmdBuffer->SetName(L"Tex Command List");

    std::printf("Texture Command Buffer Resources created successfully\n");
}

void dx_dynamic_rhi::uploadTextureToGpu(dx_texture& tex)
{
    ThrowIfFailed(_texCmdAllocator->Reset(), "Failed Reset Tex Command Allocator");
    ThrowIfFailed(_texCmdBuffer->Reset(_texCmdAllocator.Get(), nullptr), "Failed Reset Tex Command Buffer");
    
    // --- Copy buffer → texture ---
    D3D12_SUBRESOURCE_DATA subresource{};
    subresource.pData = tex.tex_ptr;
    subresource.RowPitch = tex.width * tex.bytesPerPixel;
    subresource.SlicePitch = subresource.RowPitch * tex.height;

    UpdateSubresources(
        _texCmdBuffer.Get(),
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

    _texCmdBuffer->ResourceBarrier(1, &barrier);

    // --- Submit ---
    _texCmdBuffer->Close();

    ID3D12CommandList* lists[] = { _texCmdBuffer.Get() };
    _commandQueue->ExecuteCommandLists(1, lists);

    // --- Fence wait (sync) ---
    _texFenceValue++;
    _commandQueue->Signal(_texFence.Get(), _texFenceValue);

    if (_texFence->GetCompletedValue() < _texFenceValue)
    {
        _texFence->SetEventOnCompletion(_texFenceValue, _texFenceEvent);
        WaitForSingleObject(_texFenceEvent, INFINITE);
    }

    // --- Cleanup ---
    tex.free_staging();
}