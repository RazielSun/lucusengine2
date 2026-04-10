#include "m_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "material.hpp"
#include "mesh.hpp"
#include "texture.hpp"

namespace lucus
{
    std::shared_ptr<dynamic_rhi> create_dynamic_rhi()
    {
        return std::make_shared<m_dynamic_rhi>();
    }
}

using namespace lucus;

m_dynamic_rhi::m_dynamic_rhi()
{
}

m_dynamic_rhi::~m_dynamic_rhi()
{
    for (auto& mat : _materials)
    {
        mat.cleanup();
    }
    _materials.clear();

    for (auto& tex : _textures)
    {
        tex.cleanup();
    }
    _textures.clear();

    _deviceHandle = nil;
    _device.reset();
}

void m_dynamic_rhi::init()
{
    createDevice();
    createObjectUniformBuffers();
}

window_context_handle m_dynamic_rhi::createWindowContext(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to m_viewport::init");
	}

    m_window_context context;
    context.init(_deviceHandle, window);

    _contexts.push_back(context);

    window_context_handle out_handle(static_cast<uint32_t>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    return out_handle;
}

const std::vector<window_context_handle>& m_dynamic_rhi::getWindowContexts() const
{
    return _contextHandles;
}

float m_dynamic_rhi::getWindowContextAspectRatio(const window_context_handle& ctx_handle) const
{
    if (!ctx_handle.is_valid()) {
        return 1.0f; // Default aspect ratio for invalid handle
    }

    const m_window_context& context = _contexts[ctx_handle.as_index()];
    CGSize drawableSize = context.getDrawableSize();
    return drawableSize.width / drawableSize.height;
}

void m_dynamic_rhi::beginFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];

    ctx.wait_frame();
}

void m_dynamic_rhi::submit(const window_context_handle& ctx_handle, const command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];

    if (!ctx.currentDrawable)
    {
        dispatch_semaphore_signal(ctx.frameSemaphore);
        return;
    }

    id<MTLCommandBuffer> currentBuffer = [_device->getCommandPool() commandBuffer];

    dispatch_semaphore_t semaphore = ctx.frameSemaphore;
    [currentBuffer addCompletedHandler:^(id<MTLCommandBuffer> _) {
        dispatch_semaphore_signal(semaphore);
    }];

    MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    descriptor.colorAttachments[0].texture = ctx.currentDrawable.texture;
    descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    descriptor.depthAttachment.texture = ctx.depthTextures[ctx.currentBufferIndex];
    descriptor.depthAttachment.loadAction = MTLLoadActionClear;
    descriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
    descriptor.depthAttachment.clearDepth = 1.0;
    // descriptor.stencilAttachment.texture = ctx.depthTexture;
    // descriptor.stencilAttachment.storeAction = MTLStoreActionStore;
    // descriptor.stencilAttachment.loadAction = MTLLoadActionClear;
    // descriptor.stencilAttachment.clearStencil = 0;

    id<MTLRenderCommandEncoder> pass = [currentBuffer renderCommandEncoderWithDescriptor:descriptor];

    size_t frameUniformOffset = ctx.currentBufferIndex * sizeof(frame_uniform_buffer);
    ctx.uniformbuffers.write(&cmd.frame_ubo, sizeof(cmd.frame_ubo), frameUniformOffset);

    for (const auto& renderInstance : cmd.render_list)
    {
        const auto& object_id = renderInstance.object;

        size_t objectSize = sizeof(renderInstance.object_data);
        size_t objectUniformOffset = objectSize * object_id.as_index();
        _objectUniformBuffers[ctx.currentBufferIndex].write(&renderInstance.object_data, objectSize, objectUniformOffset);

        const auto& material_id = renderInstance.material;
        if (material_id.is_valid())
        {
            m_material& mmat = _materials[material_id.as_index()];
            auto psoIt = _pipelineStates.find(mmat.psoHandle);
            if (psoIt != _pipelineStates.end())
            {
                m_pipeline_state& pso = psoIt->second;
                [pass setRenderPipelineState:pso.getPipeline()];
                [pass setDepthStencilState:pso.getDepthStencilState()];
                if (mmat.bUniformBufferUsed)
                {
                    [pass setVertexBuffer:ctx.uniformbuffers.get() offset:frameUniformOffset atIndex:(NSUInteger)shader_binding::frame];
                    [pass setVertexBuffer:_objectUniformBuffers[ctx.currentBufferIndex].get() offset:objectUniformOffset atIndex:(NSUInteger)shader_binding::object];
                }
                if (mmat.bTexturesUsed)
                {
                    for (auto& tex_bind : mmat.texture_binds)
                    {
                        [pass setFragmentTexture:tex_bind.texture atIndex:tex_bind.slot];
                        [pass setFragmentSamplerState:tex_bind.sampler atIndex:tex_bind.slot];
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
                    [pass setVertexBuffer:gpuMesh.vertexBuffer.get() offset:0 atIndex:(NSUInteger)shader_binding::vertex];
                }

                if (gpuMesh.indexCount > 0)
                {
                    [pass drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:gpuMesh.indexCount indexType:MTLIndexTypeUInt32 indexBuffer:gpuMesh.indexBuffer.get() indexBufferOffset:0];
                }
                else if (gpuMesh.vertexCount > 0)
                {
                    [pass drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:gpuMesh.vertexCount instanceCount:1];
                }
            }
        }
    }

    [pass endEncoding];

    [currentBuffer presentDrawable:ctx.currentDrawable];
    [currentBuffer commit];
}

void m_dynamic_rhi::endFrame(const window_context_handle& ctx_handle)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];
    
    ctx.currentBufferIndex = (ctx.currentBufferIndex + 1) % g_framesInFlight;
}

material_handle m_dynamic_rhi::createMaterial(material* mat)
{
    assert(mat && "Material pointer cannot be null");
    
    uint64_t shaderHash = mat->getHash();

    uint64_t psoHash = 0;
    auto it = _pipelineStates.find(shaderHash);
    if (it != _pipelineStates.end()) {
        psoHash = it->first;
    }

    if (psoHash == 0)
    {
        _pipelineStates.emplace(shaderHash, _deviceHandle);
        it = _pipelineStates.find(shaderHash);
        m_pipeline_state& pso = it->second;

        // TODO: remove hardcoded pixel format
        pso.init(mat, _contexts[0].getPixelFormat(), _contexts[0].getDepthPixelFormat());

        std::printf("PSO %llu created successfully\n", static_cast<unsigned long long>(shaderHash));

        psoHash = shaderHash;
    }

    _materials.push_back(m_material());

    m_material& mmat = _materials.back();
    mmat.psoHandle = psoHash;
    // mmat.init(_deviceHandle, mat);
    mmat.bUniformBufferUsed = mat->isUseUniformBuffers();
    if (mat->getTexturesCount() > 0)
    {
        mmat.bTexturesUsed = true;
        mmat.texture_binds.reserve(mat->getTexturesCount());
        int i = 0;
        for (const auto& tex : mat->getTextures())
        {
            auto mtexIt = _textures.find(tex->getHash());
            if (mtexIt != _textures.end())
            {
                m_texture& mtex = mtexIt->second;
                mmat.push_back({mtex.texture, mtex.sampler, i});
                i++;
            }
        }
    }

    uint32_t matIndex = static_cast<uint32_t>(_materials.size());
    std::printf("Material %d created successfully\n", matIndex);

    return material_handle(matIndex);
}

mesh_handle m_dynamic_rhi::createMesh(mesh* msh)
{
    assert(msh && "Mesh pointer cannot be null");
    
    uint64_t meshHash = msh->getHash();

    auto it = _meshes.find(meshHash);
    if (it != _meshes.end()) {
        return mesh_handle(it->first);
    }

    _meshes.emplace(meshHash, m_mesh());

    it = _meshes.find(meshHash);
    it->second.init(_deviceHandle, msh);

    std::printf("Mesh %llu created successfully\n", static_cast<unsigned long long>(meshHash));

    return mesh_handle(meshHash);
}

texture_handle m_dynamic_rhi::loadTexture(texture* tex)
{
    assert(tex);
    uint64_t texHash = tex->getHash();

    auto it = _textures.find(texHash);
    if (it != _textures.end()) {
        return texture_handle(it->first);
    }

    _textures.emplace(texHash, m_texture());
    it = _textures.find(texHash);
    m_texture& mtex = it->second;

    mtex.init(_deviceHandle, tex);

    uploadTextureToGpu(mtex);
    // vktex.free_staging();
    
    std::printf("Texture %llu loaded successfully\n", static_cast<unsigned long long>(texHash));

    return texture_handle(texHash);
}

render_object_handle m_dynamic_rhi::createUniformBuffer(render_object* obj)
{
    static uint32_t nextObjectId = 0;
    uint32_t objectId = ++nextObjectId;
    if (objectId >= g_maxObjectBufferCount) {
        throw std::runtime_error("Exceeded maximum object buffer count");
    }
    return render_object_handle(objectId);
}

void m_dynamic_rhi::createDevice()
{
    // engine_info& engine_info = engine_info::instance();

    _device = std::make_unique<m_device>();
    _device->createLogicalDevice();

    _deviceHandle = _device->getDevice();
}

void m_dynamic_rhi::createObjectUniformBuffers()
{
    size_t size = sizeof(object_uniform_buffer) * g_maxObjectBufferCount;
    for (int i = 0; i < g_framesInFlight; ++i)
    {
        _objectUniformBuffers[i].init(_deviceHandle, size);
    }

    std::printf("Frame & Object uniform buffers created successfully\n");
}

void m_dynamic_rhi::uploadTextureToGpu(m_texture& tex)
{
    id<MTLCommandBuffer> currentBuffer = [_device->getCommandPool() commandBuffer];

    id<MTLBlitCommandEncoder> blit = [currentBuffer blitCommandEncoder];

    [blit copyFromBuffer:tex.stgBuffer
        sourceOffset:0
    sourceBytesPerRow:tex.bytesPerRow
    sourceBytesPerImage:tex.texSize
            sourceSize:MTLSizeMake(tex.width, tex.height, 1)
            toTexture:tex.texture
    destinationSlice:0
    destinationLevel:0
    destinationOrigin:MTLOriginMake(0,0,0)];

    [blit endEncoding];

    [currentBuffer commit];
    [currentBuffer waitUntilCompleted];
}