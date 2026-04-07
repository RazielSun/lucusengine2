#include "m_render_types.hpp"

#include "mesh.hpp"

using namespace lucus;

void m_mesh::init(id<MTLDevice> device, mesh* msh)
{
    assert(device);
    assert(msh);

    vertexCount = msh->getDrawCount();

    const auto vtxCount = msh->getVertices().size();
    const auto idxCount = msh->getIndices().size();

    bHasVertexData = vtxCount > 0;
    if (bHasVertexData)
    {
        // TODO: Consider using a staging buffer for better performance, especially for large meshes
        vertexBuffer.init(device, msh->getVertices().size() * sizeof(vertex));
        vertexBuffer.write(msh->getVertices().data(), msh->getVertices().size() * sizeof(vertex));

        if (idxCount > 0)
        {
            indexBuffer.init(device, idxCount * sizeof(uint32_t));
            indexBuffer.write(msh->getIndices().data(), idxCount * sizeof(uint32_t));
        }
    }
}

void m_mesh::cleanup()
{
    vertexBuffer.cleanup();
    indexBuffer.cleanup();
}