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

    vertexCount = msh->getVertexCount();
    const auto idxCount = msh->getIndices().size();

    if (msh->hasVertices())
    {
        // TODO: Consider using a staging buffer for better performance, especially for large meshes
        vertexBuffer.init(device, sizeof(vertex), vertexCount);
        vertexBuffer.write(msh->getVertices().data(), sizeof(vertex) * vertexCount);

        if (idxCount > 0)
        {
            indexCount = idxCount;
            indexBuffer.init(device, sizeof(u32), idxCount);
            indexBuffer.write(msh->getIndices().data(), sizeof(u32) * idxCount);
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

    mtexture = [device newTextureWithDescriptor:texDesc];

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

void m_texture::free_staging()
{
    stgBuffer = nil;
}

void m_texture::cleanup()
{
    // TODO?
}