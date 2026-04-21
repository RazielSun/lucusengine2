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

        srvGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvHeap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), srvDescriptorSize);
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

        samplerGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(samplerHeap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), samplerDescriptorSize);
    }
}

void dx_sampler::cleanup()
{
    //
}