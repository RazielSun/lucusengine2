#include "m_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "command_buffer.hpp"
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
    for (auto& smpl : _samplers)
    {
        smpl.cleanup();
    }
    _samplers.clear();

    for (auto& buffer : _uniformBuffers)
    {
        buffer.cleanup();
    }
    _uniformBuffers.clear();

    for (auto& msh : _meshes)
    {
        msh.second.cleanup();
    }
    _meshes.clear();

    for (auto& pso : _pipelineStates)
    {
        pso.second.cleanup();
    }
    _pipelineStates.clear();

    for (auto& tex : _textures)
    {
        tex.second.cleanup();
    }
    _textures.clear();

    // TODO: Window context cleanup

    _deviceHandle = nil;
    _device.reset();
}

void m_dynamic_rhi::init()
{
    createDevice();
    // createObjectUniformBuffers();
}

window_context_handle m_dynamic_rhi::createWindowContext(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to m_viewport::init");
	}

    auto& context = _contexts.emplace_back();
    context.init(_deviceHandle, window);

    window_context_handle out_handle(static_cast<u32>(_contexts.size()));
    _contextHandles.push_back(out_handle);

    return out_handle;
}

void m_dynamic_rhi::getWindowContextSize(const window_context_handle& ctx_handle, u32& width, u32& height) const
{
    width = 0, height = 0;
    if (!ctx_handle.is_valid())
    {
        // TODO: error
        return;
    }

    const auto& ctx = _contexts[ctx_handle.as_index()];
    CGSize drawableSize = ctx.getDrawableSize();
    width = drawableSize.width;
    height = drawableSize.height;
}

void m_dynamic_rhi::execute(const window_context_handle& ctx_handle, u32 frameIndex, const gpu_command_buffer& cmd)
{
    if (!ctx_handle.is_valid()) {
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;

    ctx.wait_frame();

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

    id<MTLRenderCommandEncoder> pass = nil; // current pass

    for (u32 i = 0; i < cmd.commandCount; ++i)
    {
        const u8 *data = cmd.data.data() + i * COMMAND_FIXED_SIZE;
        const gpu_command_base* gpu_cmd = reinterpret_cast<const gpu_command_base*>(data);
        switch (gpu_cmd->type)
        {
            case gpu_command_type::RENDER_PASS_BEGIN:
                {
                    const auto* rb_cmd = reinterpret_cast<const gpu_render_pass_begin_command*>(data);

                    MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                    descriptor.colorAttachments[0].texture = ctx.currentDrawable.texture;
                    descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                    descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                    descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
                    descriptor.depthAttachment.texture = ctx.depthTextures[currentFrame];
                    descriptor.depthAttachment.loadAction = MTLLoadActionClear;
                    descriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
                    descriptor.depthAttachment.clearDepth = 1.0;
                    // descriptor.stencilAttachment.texture = ctx.depthTexture;
                    // descriptor.stencilAttachment.storeAction = MTLStoreActionStore;
                    // descriptor.stencilAttachment.loadAction = MTLLoadActionClear;
                    // descriptor.stencilAttachment.clearStencil = 0;

                    pass = [currentBuffer renderCommandEncoderWithDescriptor:descriptor];
                }
                break;
            case gpu_command_type::RENDER_PASS_END:
                {
                    [pass endEncoding];
                }
                break;
            case gpu_command_type::SET_VIEWPORT:
                {
                    const auto* vp_cmd = reinterpret_cast<const gpu_set_viewport_command*>(data);
                    MTLViewport viewport{double(vp_cmd->x),double(vp_cmd->y),double(vp_cmd->width),double(vp_cmd->height), 0., 1.};
                    [pass setViewport:viewport];
                }
                break;
            case gpu_command_type::SET_SCISSOR:
                {
                    const auto* sc_cmd = reinterpret_cast<const gpu_set_scissor_command*>(data);
                    MTLScissorRect scissor{NSUInteger(sc_cmd->offset_x), NSUInteger(sc_cmd->offset_y),
                        NSUInteger(sc_cmd->extent_x), NSUInteger(sc_cmd->extent_y)};
                    [pass setScissorRect:scissor];
                }
                break;
            case gpu_command_type::BIND_PIPELINE:
                {
                    const auto* bp_cmd = reinterpret_cast<const gpu_bind_pipeline_command*>(data);
                    auto found = _pipelineStates.find(bp_cmd->pso_handle.get());
                    assert(found != _pipelineStates.end());
                    m_pipeline_state& pso = found->second;
                    [pass setRenderPipelineState:pso.getPipeline()];
                    [pass setDepthStencilState:pso.getDepthStencilState()];
                }
                break;
            case gpu_command_type::BIND_UNIFORM_BUFFER:
                {
                    const auto* bu_cmd = reinterpret_cast<const gpu_bind_uniform_buffer_command*>(data);
                    auto& buffer = _uniformBuffers[bu_cmd->ub_handle.as_index()];
                    const u32 offset = buffer.getItemSize() * currentFrame;
                    const auto& stage = buffer.getStage();
                    if (stage == shader_binding_stage::VERTEX || stage == shader_binding_stage::BOTH)
                    {
                        [pass setVertexBuffer:buffer.get() offset:offset atIndex:(u32)bu_cmd->position];
                    }
                    if (stage == shader_binding_stage::FRAGMENT || stage == shader_binding_stage::BOTH)
                    {
                        [pass setFragmentBuffer:buffer.get() offset:offset atIndex:(u32)bu_cmd->position];
                    }
                }
                break;
            case gpu_command_type::BIND_TEXTURE:
                {
                    const auto* bt_cmd = reinterpret_cast<const gpu_bind_texture_command*>(data);
                    auto found = _textures.find(bt_cmd->tex_handle.get());
                    assert(found != _textures.end());
                    auto& tex = found->second;
                    [pass setFragmentTexture:tex.mtexture atIndex:(u32)bt_cmd->position - (u32)shader_binding::TEXTURE];
                }
                break;
            case gpu_command_type::BIND_SAMPLER:
                {
                    const auto* bs_cmd = reinterpret_cast<const gpu_bind_sampler_command*>(data);
                    auto& smpl = _samplers[bs_cmd->smpl_handle.as_index()];
                    [pass setFragmentSamplerState:smpl.sampler atIndex:(u32)bs_cmd->position - (u32)shader_binding::SAMPLER];
                }
                break;
            case gpu_command_type::BIND_DESCRIPTION_TABLE:
                {
                    // Nothing to do
                }
                break;
            case gpu_command_type::BIND_VERTEX_BUFFER:
                {
                    const auto* bv_cmd = reinterpret_cast<const gpu_bind_vertex_command*>(data);
                    auto found = _meshes.find(bv_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    [pass setVertexBuffer:gpuMesh.vertexBuffer.get() offset:0 atIndex:(u32)bv_cmd->position];
                }
                break;
            case gpu_command_type::DRAW_INDEXED:
                {
                    const auto* di_cmd = reinterpret_cast<const gpu_draw_indexed_command*>(data);
                    auto found = _meshes.find(di_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    [pass drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:di_cmd->indexCount indexType:MTLIndexTypeUInt32 indexBuffer:gpuMesh.indexBuffer.get() indexBufferOffset:0];
                }
                break;
            case gpu_command_type::DRAW_VERTEX:
                {
                    const auto* dv_cmd = reinterpret_cast<const gpu_draw_vertex_command*>(data);
                    [pass drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:dv_cmd->vertexCount instanceCount:1];
                }
                break;
            default:
                {
                    std::printf("execute cmd doesn't handled %d\n", (u32)gpu_cmd->type);
                }
                break;
        }
    }

    [currentBuffer presentDrawable:ctx.currentDrawable];
    [currentBuffer commit];
}

pipeline_state_handle m_dynamic_rhi::createPSO(material* mat)
{
    assert(mat && "Material pointer cannot be null");
    
    pipeline_state_handle pso_handle = pipeline_state_handle(mat->getHash());

    auto it = _pipelineStates.find(pso_handle.get());
    if (it != _pipelineStates.end()) {
        return pso_handle;
    }

    {
        auto pair = _pipelineStates.emplace(pso_handle.get(), _deviceHandle);
        assert(pair.second);

        m_pipeline_state& pso = pair.first->second;

        m_pipeline_state_desc init_desc;

        if (mat->useVertexIndexBuffers())
        {
            NSUInteger bufferIndex = (NSUInteger)shader_binding::VERTEX;
            MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];

            vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
            vertexDescriptor.attributes[0].offset = offsetof(vertex, position);
            vertexDescriptor.attributes[0].bufferIndex = bufferIndex;

            vertexDescriptor.attributes[1].format = MTLVertexFormatFloat3;
            vertexDescriptor.attributes[1].offset = offsetof(vertex, normal);
            vertexDescriptor.attributes[1].bufferIndex = bufferIndex;

            vertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
            vertexDescriptor.attributes[2].offset = offsetof(vertex, texCoords);
            vertexDescriptor.attributes[2].bufferIndex = bufferIndex;

            vertexDescriptor.attributes[3].format = MTLVertexFormatFloat3;
            vertexDescriptor.attributes[3].offset = offsetof(vertex, color);
            vertexDescriptor.attributes[3].bufferIndex = bufferIndex;

            vertexDescriptor.layouts[bufferIndex].stride = sizeof(vertex);
            init_desc.vertexDescriptor = vertexDescriptor;
        }

        init_desc.colorFormat = _contexts[0].getPixelFormat();
        init_desc.depthFormat = _contexts[0].getDepthPixelFormat();

        // TODO: remove hardcoded pixel format
        const std::string& shaderName = mat->getShaderName();
        pso.init(shaderName, init_desc);

        std::printf("PSO %u created successfully\n", pso_handle.get());
    }

    return pso_handle;
}

mesh_handle m_dynamic_rhi::createMesh(mesh* msh)
{
    assert(msh && "Mesh pointer cannot be null");
    
    u32 meshHash = msh->getHash();

    auto it = _meshes.find(meshHash);
    if (it != _meshes.end()) {
        return mesh_handle(meshHash);
    }

    auto pair = _meshes.emplace(meshHash, m_mesh());
    assert(pair.second);

    auto& mmsh = pair.first->second;
    mmsh.init(_deviceHandle, msh);

    std::printf("Mesh %u created successfully\n", meshHash);

    return mesh_handle(meshHash);
}

sampler_handle m_dynamic_rhi::createSampler()
{
    auto& smpl = _samplers.emplace_back();
    
    smpl.init(_deviceHandle);

    sampler_handle smpl_handle(static_cast<u32>(_samplers.size()));

    std::printf("Sampler %d created successfully\n", smpl_handle.get());

    return smpl_handle;
}

texture_handle m_dynamic_rhi::createTexture(texture* tex)
{
    assert(tex);
    u32 texHash = tex->getHash();

    auto it = _textures.find(texHash);
    if (it != _textures.end()) {
        return texture_handle(it->first);
    }

    auto pair = _textures.emplace(texHash, m_texture());
    assert(pair.second);

    auto& mtex = pair.first->second;
    mtex.init(_deviceHandle, tex);
    
    std::printf("Texture %u created successfully\n", texHash);

    return texture_handle(texHash);
}

void m_dynamic_rhi::loadTextureToGPU(const texture_handle& tex_handle, u32 frameIndex)
{
    auto it = _textures.find(tex_handle.get());
    if (it == _textures.end()) {
        // error
        return;
    }

    auto& mtex = it->second;
    uploadTextureToGpu(mtex);
    mtex.free_staging();
    
    std::printf("Texture %u loaded successfully\n", tex_handle.get());
}

uniform_buffer_handle m_dynamic_rhi::createUniformBuffer(size_t bufferSize, shader_binding_stage stage)
{
    auto& buffer = _uniformBuffers.emplace_back();
    
    buffer.init(_deviceHandle, bufferSize, g_framesInFlight, stage);

    uniform_buffer_handle ub_handle(static_cast<u32>(_uniformBuffers.size()));

    std::printf("Uniform buffer %d created successfully\n", ub_handle.get());

    return ub_handle;
}

void m_dynamic_rhi::getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr)
{
    // std::printf("getUniformBufferMemory %d [Ctx: %d]\n", ub_handle.get(), ctx_handle.get());
    if (!ub_handle.is_valid())
    {
        throw std::runtime_error("failed to get uniform buffer memory! ub is null!");
    }

    auto& buffer = _uniformBuffers[ub_handle.as_index()];

    u32 currentFrame = frameIndex % g_framesInFlight;
    memory_ptr = buffer.getMappedData(currentFrame);
}

void m_dynamic_rhi::createDevice()
{
    // engine_info& engine_info = engine_info::instance();

    _device = std::make_unique<m_device>();
    _device->createLogicalDevice();

    _deviceHandle = _device->getDevice();
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
            toTexture:tex.mtexture
    destinationSlice:0
    destinationLevel:0
    destinationOrigin:MTLOriginMake(0,0,0)];

    [blit endEncoding];

    [currentBuffer commit];
    [currentBuffer waitUntilCompleted];

    std::printf("Texture uploaded successfully\n");
}