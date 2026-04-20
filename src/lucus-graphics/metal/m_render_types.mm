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

    if (!tex->IsInitialized())
    {
        tex->initResource();
    }

    width = tex->getWidth();
    height = tex->getHeight();
    bytesPerPixel = tex->getBytesPerPixel();
    texSize = width * height * bytesPerPixel;
    bytesPerRow = width * bytesPerPixel;

    stgBuffer = [device newBufferWithBytes:tex->getResource()
                        length:texSize
                       options:MTLResourceStorageModeShared];

    // std::memcpy((uint8_t*)stgBuffer.contents, tex->getResource(), width * height * bytesPerPixel);

    tex->freeResource();

    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                                    width:width
                                                    height:height
                                                mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;

    mtexture = [device newTextureWithDescriptor:texDesc];

    // MTLRegion region = {{0, 0, 0},{ width, height, 1 }};

    // [texture replaceRegion:region
    //         mipmapLevel:0
    //             withBytes:tex->getResource()
    //         bytesPerRow:bytesPerRow];
}

void m_texture::free_staging()
{
    stgBuffer = nil;
}

void m_texture::cleanup()
{
    mtexture = nil;
}

void m_sampler::init(id<MTLDevice> device)
{
    MTLSamplerDescriptor* smplDesc = [MTLSamplerDescriptor new];

    smplDesc.minFilter = MTLSamplerMinMagFilterLinear;
    smplDesc.magFilter = MTLSamplerMinMagFilterLinear;

    smplDesc.sAddressMode = MTLSamplerAddressModeRepeat;
    smplDesc.tAddressMode = MTLSamplerAddressModeRepeat;

    sampler = [device newSamplerStateWithDescriptor:smplDesc];
}

void m_sampler::cleanup()
{
    sampler = nil;
}