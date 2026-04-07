#include "dx_render_types.hpp"

#include "mesh.hpp"

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

    vertexCount = msh->getDrawCount();

    const auto vtxCount = msh->getVertices().size();
    const auto idxCount = msh->getIndices().size();

    bHasVertexData = vtxCount > 0;
    if (bHasVertexData)
    {
        // TODO: Consider staging buffer for larger meshes
        uint32_t vbSize = sizeof(vertex) * vtxCount;
        vertexBuffer.init(device, vbSize);
        vertexBuffer.write(msh->getVertices().data(), vbSize);

        vbView.BufferLocation = vertexBuffer.get()->GetGPUVirtualAddress();
        vbView.SizeInBytes = vbSize;
        vbView.StrideInBytes = sizeof(vertex);

        if (idxCount > 0) {
            indexCount = idxCount;

            uint32_t ibSize = sizeof(uint32_t) * idxCount;

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