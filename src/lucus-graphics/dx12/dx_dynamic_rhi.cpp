#include "dx_dynamic_rhi.hpp"

#include "window_manager.hpp"

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
    for (auto& buffer : _objectUniformBuffers) {
        buffer.cleanup();
    }
    _objectUniformBuffers.clear();

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

const std::vector<window_context_handle>& dx_dynamic_rhi::getWindowContexts() const
{
    return _contextHandles;
}

float dx_dynamic_rhi::getWindowContextAspectRatio(const window_context_handle& ctx_handle) const
{
    if (!ctx_handle.is_valid()) {
        return 1.0f;
    }

    const auto& ctx = _contexts[ctx_handle.as_index()];
    return static_cast<float>(ctx.viewport.viewport.Width) / static_cast<float>(ctx.viewport.viewport.Height);
}

void dx_dynamic_rhi::beginFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    // Nothing?
}

void dx_dynamic_rhi::submit(const window_context_handle& ctx_handle, const command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    uint32_t bufferIndex = ctx.backBufferIndex % g_framesInFlight;

    // Here or in beginFrame?
    ThrowIfFailed(ctx.commandPool.commandAllocators[bufferIndex]->Reset(), "Failed Reset Command Allocator");
    ThrowIfFailed(ctx.commandBuffer->Reset(ctx.commandPool.commandAllocators[bufferIndex].Get(), nullptr), "Failed Reset Command Buffer");
    
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

    ctx.commandBuffer->RSSetViewports(1, &ctx.viewport.viewport);
    ctx.commandBuffer->RSSetScissorRects(1, &ctx.viewport.scissor);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = ctx.mRTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(ctx.backBufferIndex) * ctx.mRTVDescriptorSize;

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = ctx.mDSVHeap->GetCPUDescriptorHandleForHeapStart();
    dsv.ptr += static_cast<SIZE_T>(ctx.backBufferIndex) * ctx.mDSVDescriptorSize;

    const float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    ctx.commandBuffer->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    ctx.commandBuffer->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
    ctx.commandBuffer->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ctx.uniformbuffers.write(bufferIndex, &cmd.frame_ubo, sizeof(cmd.frame_ubo));

    for (const auto& renderInstance : cmd.render_list)
    {
        const auto& object_id = renderInstance.object;

        dx_uniform_buffer& buffer = _objectUniformBuffers[object_id.as_index()];
        buffer.write(bufferIndex, &renderInstance.object_data, sizeof(renderInstance.object_data));

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            dx_material& dxmat = _materials[material_id.as_index()];
            auto psoIt = _pipelineStates.find(dxmat.psoHandle);
            if (psoIt != _pipelineStates.end())
            {
                dx_pipeline_state& pso = psoIt->second;
                ctx.commandBuffer->SetGraphicsRootSignature(pso.getRootSignature().Get());
                ctx.commandBuffer->SetPipelineState(pso.getPipeline().Get());
                if (dxmat.bUniformBufferUsed)
                {
                    ctx.commandBuffer->SetGraphicsRootConstantBufferView((uint32_t)shader_binding::frame, ctx.uniformbuffers.get(bufferIndex)->GetGPUVirtualAddress());
                    ctx.commandBuffer->SetGraphicsRootConstantBufferView((uint32_t)shader_binding::object, _objectUniformBuffers[object_id.as_index()].get(bufferIndex)->GetGPUVirtualAddress());
                }
                if (dxmat.bTexturesUsed)
                {
                    const UINT srvDescriptorSize = _deviceHandle->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    const UINT samplerDescriptorSize = _deviceHandle->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

                    for (auto& tex_bind : dxmat.texture_binds)
                    {
                        ID3D12DescriptorHeap* heaps[] = { tex_bind.srvHeap.Get(), tex_bind.samplerHeap.Get() };
                        ctx.commandBuffer->SetDescriptorHeaps(2, heaps);
                        
                        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = tex_bind.srvHeap->GetGPUDescriptorHandleForHeapStart();
                        srvHandle.ptr += static_cast<UINT64>(tex_bind.slot) * srvDescriptorSize;

                        D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle = tex_bind.samplerHeap->GetGPUDescriptorHandleForHeapStart();
                        samplerHandle.ptr += static_cast<UINT64>(tex_bind.slot) * samplerDescriptorSize;

                        const uint32_t rootIndex = static_cast<uint32_t>(shader_binding::material) + tex_bind.slot * 2;
                        ctx.commandBuffer->SetGraphicsRootDescriptorTable(rootIndex + 0, srvHandle);
                        ctx.commandBuffer->SetGraphicsRootDescriptorTable(rootIndex + 1, samplerHandle);
                    }
                }
            }
            else
            {
                std::cerr << "Invalid material handle: " << material_id.get() << std::endl;
            }
        }
        
        const auto& mesh_id = renderInstance.mesh;
        if (mesh_id.is_valid())
        {
            auto meshIt = _meshes.find(mesh_id.get());
            if (meshIt != _meshes.end())
            {
                auto& gpuMesh = meshIt->second;
                if (gpuMesh.bHasVertexData)
                {
                    ctx.commandBuffer->IASetVertexBuffers(0, 1, &gpuMesh.vbView);
                }

                ctx.commandBuffer->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                if (gpuMesh.indexCount > 0)
                {
                    ctx.commandBuffer->IASetIndexBuffer(&gpuMesh.ibView);
                    ctx.commandBuffer->DrawIndexedInstanced(gpuMesh.indexCount, 1, 0, 0, 0);
                }
                else if (gpuMesh.vertexCount > 0)
                {
                    ctx.commandBuffer->DrawInstanced(gpuMesh.vertexCount, 1, 0, 0);
                }
            }
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
}

void dx_dynamic_rhi::endFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    auto& ctx = _contexts[ctx_handle.as_index()];

    uint32_t fenceIndex = ctx.backBufferIndex % g_framesInFlight;
    const uint64_t current_fence = ctx.fenceValues[fenceIndex];

    ThrowIfFailed(_commandQueue->Signal(ctx.fence.Get(), current_fence), "Failed Queue Signal");

    ctx.backBufferIndex = ctx.swapChain->GetCurrentBackBufferIndex();

    if (ctx.fence->GetCompletedValue() < ctx.fenceValues[fenceIndex])
    {
        ThrowIfFailed(ctx.fence->SetEventOnCompletion(ctx.fenceValues[fenceIndex], (HANDLE)ctx.fenceEvent), "Failed Set Event OnCompletion");
        WaitForSingleObject((HANDLE)ctx.fenceEvent, INFINITE);
    }

    ctx.fenceValues[fenceIndex] = current_fence + 1;
}

material_handle dx_dynamic_rhi::createMaterial(material* mat)
{
    assert(mat != nullptr);

    uint64_t shaderHash = mat->getHash();

    uint64_t psoHandle = 0;
    auto it = _pipelineStates.find(shaderHash);
    if (it != _pipelineStates.end()) {
        psoHandle = it->first;
    }

    if (psoHandle == 0)
    {
        _pipelineStates.emplace(shaderHash, dx_pipeline_state(_deviceHandle));

        it = _pipelineStates.find(shaderHash);
        dx_pipeline_state& pso = it->second;
        
        // TODO: depth format should be determined by window context or material properties, not hardcoded
        pso.init(mat, _contexts[0].mDepthFormat);

        std::printf("PSO %llu created successfully\n", static_cast<unsigned long long>(shaderHash));

        psoHandle = shaderHash;
    }

    _materials.push_back(dx_material());

    dx_material& dxmat = _materials.back();
    dxmat.psoHandle = psoHandle;
    dxmat.bUniformBufferUsed = mat->isUseUniformBuffers();
    if (mat->getTexturesCount() > 0)
    {
        dxmat.bTexturesUsed = true;

        uint32_t i = 0;
        for (const auto& tex : mat->getTextures())
        {
            auto dxtexIt = _textures.find(tex->getHash());
            if (dxtexIt != _textures.end())
            {
                dx_texture& dxtex = dxtexIt->second;
                dxmat.texture_binds.push_back({dxtex.srvHeap, dxtex.samplerHeap, i});
            }
            i++;
        }
    }

    uint32_t matIndex = static_cast<uint32_t>(_materials.size());
    std::printf("Material %d created successfully\n", matIndex);

    return material_handle(matIndex);
}

mesh_handle dx_dynamic_rhi::createMesh(mesh* msh)
{
    assert(msh != nullptr);
    uint64_t meshHash = msh->getHash();

    auto it = _meshes.find(meshHash);
    if (it != _meshes.end()) {
        return mesh_handle(it->first);
    }

    _meshes.emplace(meshHash, dx_mesh());

    it = _meshes.find(meshHash);

    it->second.init(_deviceHandle, msh);

    std::printf("Mesh %llu created successfully\n", static_cast<unsigned long long>(meshHash));

    return mesh_handle(meshHash);
}

texture_handle dx_dynamic_rhi::loadTexture(texture* tex)
{
    assert(tex != nullptr);
    uint64_t texHash = tex->getHash();

    auto it = _textures.find(texHash);
    if (it != _textures.end()) {
        return texture_handle(it->first);
    }

    _textures.emplace(texHash, dx_texture());

    it = _textures.find(texHash);
    dx_texture& dxtex = it->second;

    dxtex.init(_deviceHandle, tex);

    uploadTextureToGpu(dxtex);
    
    std::printf("Texture %llu loaded successfully\n", static_cast<unsigned long long>(texHash));

    return texture_handle(texHash);
}

render_object_handle dx_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    assert(obj != nullptr);

    _objectUniformBuffers.emplace_back();
    dx_uniform_buffer& buffer = _objectUniformBuffers.back();
    buffer.init(_deviceHandle, sizeof(object_uniform_buffer));

    std::printf("Uniform buffer for object %p created successfully\n", obj);

    return render_object_handle(static_cast<uint32_t>(_objectUniformBuffers.size()));
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

    UINT64 requiredSize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[1];
    UINT numRows[1];
    UINT64 rowSizes[1];

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
        requiredSize,
        layouts,
        numRows,
        rowSizes,
        &subresource
    );

    // --- Barrier: COPY → SHADER READ ---
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            tex.texResource,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );

    _texCmdBuffer->ResourceBarrier(1, &barrier);

    // --- Submit ---
    _texCmdBuffer->Close();

    ID3D12CommandList* lists[] = { _texCmdBuffer };
    _commandQueue->ExecuteCommandLists(1, lists);

    // --- Fence wait (sync) ---
    _texFenceValue++;
    _commandQueue->Signal(_texFence, _texFenceValue);

    if (_texFence->GetCompletedValue() < _texFenceValue)
    {
        _texFence->SetEventOnCompletion(_texFenceValue, _texFenceEvent);
        WaitForSingleObject(_texFenceEvent, INFINITE);
    }

    // --- Cleanup ---
    tex.free_staging();
}