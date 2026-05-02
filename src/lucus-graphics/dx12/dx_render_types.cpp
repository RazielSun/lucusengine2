#include "dx_render_types.hpp"

#include "mesh.hpp"
#include "texture.hpp"
#include "material.hpp"

#include "gltf_utils.hpp"

using namespace lucus;

void dx_commandbuffer_pool::init(Com<ID3D12Device> device)
{
    // [Command Pool] Create command allocators for each back buffer
	for (auto& commandAllocator : commandAllocators)
	{
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf())), "Failed Create Command Allocator");
	}
}

void dx_commandbuffer_pool::cleanup()
{
    for (auto& commandAllocator : commandAllocators)
    {
        commandAllocator.Reset();
    }
}

void dx_mesh::init(Com<ID3D12Device> device, mesh* msh)
{
    assert(msh != nullptr);

    vertexCount = msh->getVertexCount();
    const auto idxCount = msh->getIndices().size();

    if (msh->hasVertices())
    {
        // TODO: Consider staging buffer for larger meshes
        u32 vbSize = sizeof(vertex) * vertexCount;
        vertexBuffer.init(device, vbSize);
        vertexBuffer.write(msh->getVertices().data(), vbSize);

        vbView.BufferLocation = vertexBuffer.get()->GetGPUVirtualAddress();
        vbView.SizeInBytes = vbSize;
        vbView.StrideInBytes = sizeof(vertex);

        if (idxCount > 0) {
            indexCount = idxCount;

            u32 ibSize = sizeof(u32) * idxCount;

            indexBuffer.init(device, ibSize);
            indexBuffer.write(msh->getIndices().data(), ibSize);

            ibView.BufferLocation = indexBuffer.get()->GetGPUVirtualAddress();
            ibView.SizeInBytes = ibSize;
            ibView.Format = DXGI_FORMAT_R32_UINT;
        }
    }
}

void dx_mesh::cleanup()
{
    vertexBuffer.cleanup();
    indexBuffer.cleanup();
}

void dx_texture::init(Com<ID3D12Device> device, Com<ID3D12DescriptorHeap> srvHeap, u32 in_index, texture* tex)
{
    assert(tex);
    texLink.reset(tex);
    index = in_index;

    if (!tex->IsInitialized())
    {
        tex->initResource();
    }

    width = tex->getWidth();
    height = tex->getHeight();
    bytesPerPixel = tex->getBytesPerPixel();

    texSize = width * height * bytesPerPixel;

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    CD3DX12_HEAP_PROPERTIES texHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &texHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texResource)
    ), "CreateCommittedResource Texture failed");

    UINT64 uploadSize = GetRequiredIntermediateSize(texResource.Get(), 0, 1);

    CD3DX12_HEAP_PROPERTIES stgHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC stgSizeDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &stgHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &stgSizeDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&stgBuffer)
    ), "CreateCommittedResource Stage Buffer failed");

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        const UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        srvCPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            srvHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(index),
            srvDescriptorSize);
        device->CreateShaderResourceView(texResource.Get(), &srvDesc, srvCPUHandle);

        if (srvHeap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        {
            srvGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvHeap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), srvDescriptorSize);
        }
        else
        {
            srvGPUHandle = {};
        }
    }
}

void dx_texture::free_staging()
{
    texLink.reset();
    stgBuffer.Reset();
}

void dx_texture::cleanup()
{
    //
}

void dx_sampler::init(Com<ID3D12Device> device, Com<ID3D12DescriptorHeap> samplerHeap, u32 in_index)
{
    index = in_index;
    {
        const UINT samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        samplerCPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            samplerHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(index),
            samplerDescriptorSize);

        D3D12_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

        device->CreateSampler(&samplerDesc, samplerCPUHandle);

        if (samplerHeap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        {
            samplerGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(samplerHeap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), samplerDescriptorSize);
        }
        else
        {
            samplerGPUHandle = {};
        }
    }
}

void dx_sampler::cleanup()
{
    //
}

void dx_heap_descriptor::init(Com<ID3D12Device> device, u32 in_bindless_stable, u32 in_ring_per_frame, u32 in_dynamic_resource_count, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    bindlessStableCount = in_bindless_stable;
    ringPerFrameCapacity = in_ring_per_frame;
    resourceCapacity = in_bindless_stable + in_dynamic_resource_count;
    resourceIndex = in_bindless_stable;
    heapItemSize = device->GetDescriptorHandleIncrementSize(type);
    _device = device;
    heapType = type;

    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = resourceCapacity;
        heapDesc.Type = type;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&resourceHeap)), "CreateDescriptorHeap failed");
    }

    const u32 shaderVisibleCount = bindlessStableCount + ringPerFrameCapacity * g_framesInFlight;
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.NumDescriptors = shaderVisibleCount;
        heapDesc.Type = type;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&shaderHeap)), "CreateDescriptorHeap failed");

        for (u32 i = 0; i < g_framesInFlight; i++)
        {
            const u32 base = bindlessStableCount + i * ringPerFrameCapacity;
            allocators[i].init(base, ringPerFrameCapacity);
        }
    }
}

u32 dx_heap_descriptor::allocateResource(u32 count)
{
    assert(resourceIndex + count <= resourceCapacity);
    u32 current = resourceIndex;
    resourceIndex += count;
    return current;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE dx_heap_descriptor::getResourceHandle(u32 index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        resourceHeap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(index),
        heapItemSize);
}

void dx_heap_descriptor::copy(u32 index, u32 frameIndex)
{
    u32 currentFrame = frameIndex % g_framesInFlight;
    u32 new_index = allocators[currentFrame].allocate();
    CD3DX12_CPU_DESCRIPTOR_HANDLE dstCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        shaderHeap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(new_index),
        heapItemSize);
    _device->CopyDescriptorsSimple(1, dstCpuHandle, getResourceHandle(index), heapType);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_heap_descriptor::getShaderHeadHandle(u32 frameIndex, u32 offset) const
{
    u32 currentFrame = frameIndex % g_framesInFlight;
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        shaderHeap->GetGPUDescriptorHandleForHeapStart(),
        static_cast<INT>(allocators[currentFrame].head + offset),
        heapItemSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE dx_heap_descriptor::getBindlessTableGpuStart() const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        shaderHeap->GetGPUDescriptorHandleForHeapStart(),
        0,
        heapItemSize);
}

void dx_heap_descriptor::copyBindlessSrvFromResourceToSlot(u32 srcResourceIndex, u32 bindlessSlot)
{
    assert(bindlessSlot < bindlessStableCount);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dst(
        shaderHeap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(bindlessSlot),
        heapItemSize);
    _device->CopyDescriptorsSimple(1, dst, getResourceHandle(srcResourceIndex), heapType);
}

void dx_heap_descriptor::copyBindlessSamplerFromResourceToStable(u32 srcResourceIndex)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE src = getResourceHandle(srcResourceIndex);
    for (u32 s = 0; s < bindlessStableCount; ++s)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE dst(
            shaderHeap->GetCPUDescriptorHandleForHeapStart(),
            static_cast<INT>(s),
            heapItemSize);
        _device->CopyDescriptorsSimple(1, dst, src, heapType);
    }
}

void dx_heap_descriptor::reset_allocator(u32 frameIndex)
{
    u32 currentFrame = frameIndex % g_framesInFlight;
    allocators[currentFrame].reset();
}

void dx_heap_descriptor::head_to_current(u32 frameIndex)
{
    u32 currentFrame = frameIndex % g_framesInFlight;
    allocators[currentFrame].head_to_current();
}

void dx_heap_descriptor::cleanup()
{
    _device.Reset();
    resourceHeap.Reset();
    shaderHeap.Reset();
}

void lucus::dx_images::init(Com<ID3D12Device> device, const dx_images_desc &init_desc)
{
    _device = device;
    bPreinitialized = init_desc.bPreinitialized;
    bShaderRead = init_desc.bShaderRead;
    ShaderHeadIndex = init_desc.ShaderHeadIndex;
    initialState = init_desc.initialState;
    descriptorSize = device->GetDescriptorHandleIncrementSize(init_desc.heapType);

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = init_desc.count;
    heap_desc.Type = init_desc.heapType;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap)), "Failed Create Descriptor Heap");

    std::printf("dx_images Heap created successfully\n");

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RESOURCE_DESC image_desc{};
    image_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    image_desc.Alignment = 0;
    image_desc.Width = static_cast<UINT64>(init_desc.width);
    image_desc.Height = static_cast<UINT>(init_desc.height);
    image_desc.DepthOrArraySize = 1;
    image_desc.MipLevels = 1;
    image_desc.Format = init_desc.format;
    image_desc.SampleDesc.Count = 1;
    image_desc.SampleDesc.Quality = 0;
    image_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    image_desc.Flags = init_desc.resourceFlags;

    if (images.size() < init_desc.count)
    {
        images.resize(init_desc.count);
    }
    states.assign(init_desc.count, init_desc.initialState);
    for (size_t i = 0; i < images.size(); ++i)
    {
        if (!bPreinitialized)
        {
            ThrowIfFailed(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &image_desc,
                init_desc.initialState,
                &init_desc.clearValue,
                IID_PPV_ARGS(&images[i])
            ), "Failed Create Committed Resource for Image");
        }

        if (init_desc.bIsColor)
        {
            if (bPreinitialized)
            {
                device->CreateRenderTargetView(images[i].Get(), nullptr, handle);
            }
            else
            {
                D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
                rtv_desc.Format = init_desc.format;
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                device->CreateRenderTargetView(images[i].Get(), &rtv_desc, handle);
            }
        }
        else
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_depth_desc{};
            DXGI_FORMAT dsvFormat = init_desc.format;
            if (dsvFormat == DXGI_FORMAT_R32_TYPELESS) dsvFormat = DXGI_FORMAT_D32_FLOAT;
            dsv_depth_desc.Format = dsvFormat;
            dsv_depth_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsv_depth_desc.Flags = D3D12_DSV_FLAG_NONE;

            device->CreateDepthStencilView(images[i].Get(), &dsv_depth_desc, handle);
        }
        
        if (bShaderRead)
        {
            assert(init_desc.resourceHeap);

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format = init_desc.shaderReadFormat;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Texture2D.MipLevels = 1;

            UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
                init_desc.resourceHeap->GetCPUDescriptorHandleForHeapStart(),
                static_cast<INT>(init_desc.ShaderHeadIndex + (u32)i),
                srvDescriptorSize);

            device->CreateShaderResourceView(images[i].Get(), &srv_desc, srvHandle);
        }

        handle.ptr += descriptorSize;
    }

    std::printf("dx_images created successfully\n");
}

void lucus::dx_images::cleanup()
{
    for (int i = 0; i < images.size(); ++i)
    {
        images[i].Reset();
    }
    states.clear();
    heap.Reset();
    _device.Reset();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE lucus::dx_images::getResourceHandle(u32 index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        heap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<INT>(index),
        descriptorSize);
}

u32 lucus::dx_images::getShaderHandle(u32 index) const
{
    return ShaderHeadIndex + index;
}

void lucus::dx_render_target::init(Com<ID3D12Device> device, const dx_render_target_desc &init_desc)
{
    bSwapChain = init_desc.bSwapChain;
    bColor = init_desc.bUseColor;
    if (bColor)
    {
        D3D12_CLEAR_VALUE color_clear{};
        color_clear.Format = init_desc.colorFormat;
        color_clear.Color[0] = 0.0f;
        color_clear.Color[1] = 0.0f;
        color_clear.Color[2] = 0.0f;
        color_clear.Color[3] = 1.0f;

        dx_images_desc color_desc {
            .count = init_desc.count,
            .width = init_desc.width,
            .height = init_desc.height,
            .format = init_desc.colorFormat,
            .shaderReadFormat = init_desc.colorFormat,
            .heapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            .initialState = bSwapChain ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET,
            .clearValue = color_clear,
            .bIsColor = true,
            .bPreinitialized = bSwapChain,
            .bShaderRead = init_desc.bColorShaderRead,
            .resourceHeap = init_desc.resourceHeap,
            .ShaderHeadIndex = init_desc.ColorSRVHeadIndex,
        };
        color.init(device, color_desc);
    }

    bDepth = init_desc.bUseDepth;
    if (bDepth)
    {
        D3D12_CLEAR_VALUE depth_clear{};
        depth_clear.Format = init_desc.depthFormat;
        depth_clear.DepthStencil.Depth = 1.f;
        depth_clear.DepthStencil.Stencil = 0.f;

        // R32_TYPELESS required to allow both DSV and SRV on same resource
        DXGI_FORMAT depthResourceFormat = init_desc.bDepthShaderRead ? DXGI_FORMAT_R32_TYPELESS : init_desc.depthFormat;

        dx_images_desc depth_desc {
            .count = init_desc.count,
            .width = init_desc.width,
            .height = init_desc.height,
            .format = depthResourceFormat,
            .shaderReadFormat = DXGI_FORMAT_R32_FLOAT,
            .heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            .initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE,
            .clearValue = depth_clear,
            .bIsColor = false,
            .bPreinitialized = false,
            .bShaderRead = init_desc.bDepthShaderRead,
            .resourceHeap = init_desc.resourceHeap,
            .ShaderHeadIndex = init_desc.DepthSRVHeadIndex,
        };
        depth.init(device, depth_desc);
    }
}

void lucus::dx_render_target::cleanup()
{
    color.cleanup();
    depth.cleanup();
}
