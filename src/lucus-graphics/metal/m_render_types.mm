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

    tex->freeResource();

    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                                                    width:width
                                                    height:height
                                                mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;

    mtexture = [device newTextureWithDescriptor:texDesc];

}

void m_texture::free_staging()
{
    stgBuffer = nil;
}

void m_texture::cleanup()
{
    mtexture = nil;
}

namespace
{
    MTLSamplerMinMagFilter to_mtl_filter(sampler_filter f)
    {
        return f == sampler_filter::NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
    }

    MTLSamplerMipFilter to_mtl_mip_filter(sampler_filter f)
    {
        return f == sampler_filter::NEAREST ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear;
    }

    MTLSamplerAddressMode to_mtl_address_mode(sampler_address_mode m)
    {
        switch (m)
        {
        case sampler_address_mode::CLAMP_TO_EDGE:   return MTLSamplerAddressModeClampToEdge;
        case sampler_address_mode::CLAMP_TO_BORDER: return MTLSamplerAddressModeClampToBorderColor;
        case sampler_address_mode::REPEAT:          break;
        }
        return MTLSamplerAddressModeRepeat;
    }
}

void m_sampler::init(id<MTLDevice> device, const sampler_desc& desc)
{
    MTLSamplerDescriptor* smplDesc = [MTLSamplerDescriptor new];

    const MTLSamplerAddressMode addressMode = to_mtl_address_mode(desc.addressMode);

    smplDesc.minFilter = to_mtl_filter(desc.filter);
    smplDesc.magFilter = to_mtl_filter(desc.filter);
    smplDesc.mipFilter = to_mtl_mip_filter(desc.mipFilter);

    smplDesc.sAddressMode = addressMode;
    smplDesc.tAddressMode = addressMode;
    smplDesc.rAddressMode = addressMode;
    smplDesc.borderColor = desc.borderColor == sampler_border_color::OPAQUE_WHITE
        ? MTLSamplerBorderColorOpaqueWhite
        : MTLSamplerBorderColorOpaqueBlack;

    sampler = [device newSamplerStateWithDescriptor:smplDesc];
}

void m_sampler::cleanup()
{
    sampler = nil;
}