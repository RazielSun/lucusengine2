#include "m_render_types.hpp"

#include "mesh.hpp"
#include "texture.hpp"
#include "material.hpp"

#include "gltf_utils.hpp"

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
            indexCount = idxCount;
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

void m_texture::init(id<MTLDevice> device, texture* tex)
{
    using namespace lucus::utils;

    assert(tex);

    int texWidth, texHeight, texChannels;
    void* tex_ptr = load_texture(tex->getFileName(), texWidth, texHeight, texChannels);

    width = texWidth;
    height = texHeight;
    bytesPerPixel = 4; // TODO
    texSize = width * height * bytesPerPixel;
    bytesPerRow = width * bytesPerPixel;

    stgBuffer = [device newBufferWithBytes:tex_ptr
                        length:texSize
                       options:MTLResourceStorageModeShared];

    // std::memcpy((uint8_t*)stgBuffer.contents, tex_ptr, width * height * bytesPerPixel);

    free_texture(tex_ptr);

    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                                    width:width
                                                    height:height
                                                mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;

    texture = [device newTextureWithDescriptor:texDesc];

    // MTLRegion region = {{0, 0, 0},{ width, height, 1 }};

    // [texture replaceRegion:region
    //         mipmapLevel:0
    //             withBytes:tex_ptr
    //         bytesPerRow:bytesPerRow];

    MTLSamplerDescriptor* smplDesc = [MTLSamplerDescriptor new];

    smplDesc.minFilter = MTLSamplerMinMagFilterLinear;
    smplDesc.magFilter = MTLSamplerMinMagFilterLinear;

    smplDesc.sAddressMode = MTLSamplerAddressModeRepeat;
    smplDesc.tAddressMode = MTLSamplerAddressModeRepeat;

    sampler = [device newSamplerStateWithDescriptor:smplDesc];
}

void m_texture::cleanup()
{
    // TODO?
}

void m_material::init(id<MTLDevice> device, material* mat)
{
    //
}

void m_material::cleanup()
{
    //
}