#include "m_dynamic_rhi.hpp"

#include "window_manager.hpp"

#include "command_buffer.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "texture.hpp"
#include "image_capture.hpp"

namespace lucus
{
    std::shared_ptr<dynamic_rhi> create_dynamic_rhi()
    {
        return std::make_shared<m_dynamic_rhi>();
    }
}

using namespace lucus;

struct m_dynamic_rhi::frame_readback
{
    id<MTLBuffer> buffer = nil;
    size_t row_pitch{0};
    bool bgra{false};
};

namespace
{
    struct pending_frame
    {
        dispatch_semaphore_t semaphore;
        bool submitted{false};

        ~pending_frame()
        {
            if (!submitted)
            {
                dispatch_semaphore_signal(semaphore);
            }
        }
    };
}

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

    for (auto& rt : _renderTargets)
    {
        rt.cleanup();
    }
    _renderTargets.clear();

    _shaderLibraries.cleanup();

    _deviceHandle = nil;
    _device.reset();
}

void m_dynamic_rhi::init(const rhi_init_desc& desc)
{
    beginValidationSetup(desc);

    createDevice();
    _shaderLibraries.init(_deviceHandle);

    _validationStatus.detail = "Metal validation is not implemented yet";
    finishValidationSetup();
}

window_context_handle m_dynamic_rhi::createWindowContext(const window_handle& handle)
{
    window* window = window_manager::instance().getWindow(handle);
	if (!window) {
		throw std::runtime_error("Invalid window handle provided to m_viewport::init");
	}

    auto& context = _contexts.emplace_back();
    context.init(_deviceHandle, window);

    const CGSize size = context.getDrawableSize();
    const u32 width = static_cast<u32>(size.width);
    const u32 height = static_cast<u32>(size.height);
    context.color_handle = createRenderTarget(width, height, render_target_type::COLOR, 1, true);
    context.depth_handle = createRenderTarget(width, height, render_target_type::DEPTH, g_framesInFlight);

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

render_target_handle m_dynamic_rhi::getWindowContextRenderTarget(const window_context_handle& ctx_handle, render_target_type rt_type) const
{
    if (!ctx_handle.is_valid())
    {
        throw std::runtime_error("getWindowContextRenderTarget:: we have invalid window context!");
    }
    const auto& ctx = _contexts[ctx_handle.as_index()];

    if (rt_type == render_target_type::DEPTH)
    {
        return ctx.depth_handle;
    }
    return ctx.color_handle;
}

id<MTLTexture> m_dynamic_rhi::renderTargetTexture(const render_target_handle& rt_handle, const m_window_context& ctx, u32 frameIndex) const
{
    if (!rt_handle.is_valid() || rt_handle.as_index() >= _renderTargets.size())
    {
        throw std::runtime_error("renderTargetTexture:: invalid render target handle " + std::to_string(rt_handle.get()));
    }

    const m_render_target& rt = _renderTargets[rt_handle.as_index()];
    if (rt.swapchain)
    {
        return ctx.currentDrawable.texture;
    }
    return rt.texture(frameIndex);
}

MTLRenderPassDescriptor* m_dynamic_rhi::makeRenderPassDescriptor(const gpu_render_pass_begin_command& cmd, const m_window_context& ctx, u32 frameIndex) const
{
    assert(cmd.colorTargetCount <= g_maxMrtColorTargets);
    assert(cmd.depthTargetCount <= 1);

    MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

    for (u8 c = 0; c < cmd.colorTargetCount; ++c)
    {
        MTLRenderPassColorAttachmentDescriptor* color = descriptor.colorAttachments[c];
        color.texture = renderTargetTexture(cmd.colorTargets[c], ctx, frameIndex);
        color.loadAction = MTLLoadActionClear;
        color.storeAction = MTLStoreActionStore;
        color.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    }

    if (cmd.depthTargetCount > 0)
    {
        descriptor.depthAttachment.texture = renderTargetTexture(cmd.depthTarget, ctx, frameIndex);
        descriptor.depthAttachment.loadAction = MTLLoadActionClear;
        descriptor.depthAttachment.storeAction = MTLStoreActionStore;
        descriptor.depthAttachment.clearDepth = 1.0;
    }

    return descriptor;
}

void m_dynamic_rhi::execute(const window_context_handle& ctx_handle, u32 frameIndex, const gpu_command_buffer& cmd, image_capture* capture)
{
    if (!ctx_handle.is_valid() || ctx_handle.as_index() >= _contexts.size()) {
        if (capture)
        {
            throw std::runtime_error("Metal screenshot: invalid window context");
        }
        return;
    }

    m_window_context& ctx = _contexts[ctx_handle.as_index()];

    if (capture)
    {
        ctx.metalLayer.framebufferOnly = NO;
    }
    ctx.wait_frame();
    pending_frame frame{ctx.frameSemaphore};

    if (!ctx.currentDrawable)
    {
        if (capture)
        {
            throw std::runtime_error("Metal screenshot: no drawable is available");
        }
        return;
    }

    id<MTLCommandBuffer> currentBuffer = createFrameCommandBuffer(ctx.frameSemaphore);
    recordCommands(ctx, frameIndex % g_framesInFlight, currentBuffer, cmd);

    frame_readback readback;
    if (capture)
    {
        id<MTLTexture> source = ctx.currentDrawable.texture;
        prepareFrameCapture(source, *capture, readback);
        recordFrameCapture(currentBuffer, source, *capture, readback);
    }

    [currentBuffer presentDrawable:ctx.currentDrawable];
    [currentBuffer commit];
    frame.submitted = true;
    ctx.currentDrawable = nil;

    if (capture)
    {
        completeFrameCapture(currentBuffer, readback, *capture);
    }
}

id<MTLCommandBuffer> m_dynamic_rhi::createFrameCommandBuffer(dispatch_semaphore_t frameSemaphore)
{
    id<MTLCommandBuffer> currentBuffer = [_device->getCommandPool() commandBuffer];
    if (!currentBuffer)
    {
        throw std::runtime_error("Failed to create Metal command buffer");
    }

    [currentBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        dispatch_semaphore_signal(frameSemaphore);
    }];

    return currentBuffer;
}

void m_dynamic_rhi::recordCommands(const m_window_context& ctx, u32 currentFrame, id<MTLCommandBuffer> currentBuffer, const gpu_command_buffer& cmd)
{
    id<MTLRenderCommandEncoder> pass = nil;

    for (const gpu_command_base& command : cmd)
    {
        const u8* data = reinterpret_cast<const u8*>(&command);
        const gpu_command_base* gpu_cmd = &command;
        switch (gpu_cmd->type)
        {
            case gpu_command_type::RENDER_PASS_BEGIN:
                {
                    const auto* rb_cmd = reinterpret_cast<const gpu_render_pass_begin_command*>(data);
                    MTLRenderPassDescriptor* descriptor = makeRenderPassDescriptor(*rb_cmd, ctx, currentFrame);
                    pass = [currentBuffer renderCommandEncoderWithDescriptor:descriptor];
                }
                break;
            case gpu_command_type::RENDER_PASS_END:
                {
                    [pass endEncoding];
                    pass = nil;
                }
                break;
            case gpu_command_type::IMAGE_BARRIER:
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
                    const u32 offset = buffer.getItemStride() * currentFrame;
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
                    if (found == _textures.end()) {
                        throw std::runtime_error("failed to bind texture! texture " + std::to_string(bt_cmd->tex_handle.get()) + " is not on the GPU!");
                    }
                    auto& tex = found->second;
                    [pass setFragmentTexture:tex.mtexture atIndex:(u32)bt_cmd->position - (u32)shader_binding::TEXTURE];
                }
                break;
            case gpu_command_type::BIND_RENDER_TARGET:
                {
                    const auto* brt_cmd = reinterpret_cast<const gpu_bind_render_target_command*>(data);
                    id<MTLTexture> tex = renderTargetTexture(brt_cmd->rt_handle, ctx, currentFrame);
                    [pass setFragmentTexture:tex atIndex:(u32)brt_cmd->position - (u32)shader_binding::TEXTURE];
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
                }
                break;
            case gpu_command_type::SET_BINDLESS:
                {
                    (void)reinterpret_cast<const gpu_set_bindless_command*>(data);
                }
                break;
            case gpu_command_type::BIND_VERTEX_BUFFER:
                {
                    const auto* bv_cmd = reinterpret_cast<const gpu_bind_vertex_command*>(data);
                    auto found = _meshes.find(bv_cmd->msh_handle.get());
                    assert(found != _meshes.end());
                    auto& gpuMesh = found->second;
                    [pass setVertexBuffer:gpuMesh.vertexBuffer.get() offset:0 atIndex:g_metalVertexBufferIndex];
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
}

void m_dynamic_rhi::prepareFrameCapture(id<MTLTexture> source, image_capture& capture, frame_readback& readback)
{
    switch (source.pixelFormat)
    {
        case MTLPixelFormatRGBA8Unorm:
        case MTLPixelFormatRGBA8Unorm_sRGB:
            break;
        case MTLPixelFormatBGRA8Unorm:
        case MTLPixelFormatBGRA8Unorm_sRGB:
            readback.bgra = true;
            break;
        default:
            throw std::runtime_error("Metal screenshot: unsupported drawable pixel format " +
                std::to_string(source.pixelFormat));
    }
    if (source.width > std::numeric_limits<u32>::max() || source.height > std::numeric_limits<u32>::max())
    {
        throw std::runtime_error("Metal screenshot: drawable dimensions are too large");
    }
    capture = image_capture(static_cast<u32>(source.width), static_cast<u32>(source.height));
    readback.row_pitch = (static_cast<size_t>(capture.width) * 4 + 255) & ~size_t(255);
    if (capture.height > std::numeric_limits<size_t>::max() / readback.row_pitch)
    {
        throw std::runtime_error("Metal screenshot: readback buffer is too large");
    }
    readback.buffer = [_deviceHandle newBufferWithLength:readback.row_pitch * capture.height
        options:MTLResourceStorageModeShared];
    if (!readback.buffer)
    {
        throw std::runtime_error("Metal screenshot: failed to allocate readback buffer");
    }
}

void m_dynamic_rhi::recordFrameCapture(id<MTLCommandBuffer> currentBuffer, id<MTLTexture> source, const image_capture& capture, const frame_readback& readback)
{
    id<MTLBlitCommandEncoder> blit = [currentBuffer blitCommandEncoder];
    if (!blit)
    {
        throw std::runtime_error("Metal screenshot: failed to create blit encoder");
    }
    [blit copyFromTexture:source sourceSlice:0 sourceLevel:0
        sourceOrigin:MTLOriginMake(0, 0, 0)
        sourceSize:MTLSizeMake(capture.width, capture.height, 1)
        toBuffer:readback.buffer destinationOffset:0 destinationBytesPerRow:readback.row_pitch
        destinationBytesPerImage:readback.row_pitch * capture.height];
    [blit endEncoding];
}

void m_dynamic_rhi::completeFrameCapture(id<MTLCommandBuffer> currentBuffer, const frame_readback& readback, image_capture& capture)
{
    [currentBuffer waitUntilCompleted];
    if (currentBuffer.status != MTLCommandBufferStatusCompleted)
    {
        NSString* detail = currentBuffer.error.localizedDescription;
        throw std::runtime_error(std::string("Metal screenshot: GPU execution failed: ") +
            (detail ? detail.UTF8String : "unknown error"));
    }
    capture.copy_pixels(readback.buffer.contents, readback.buffer.length, readback.row_pitch, readback.bgra);
}

pipeline_state_handle m_dynamic_rhi::createPSO(material* mat, render_pass_name passName)
{
    assert(mat && "Material pointer cannot be null");

    if (passName == render_pass_name::GBUFFER_PASS || passName == render_pass_name::DEFERRED_LIGHTING_PASS)
    {
        throw std::runtime_error("Metal backend does not support deferred rendering yet");
    }

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
            NSUInteger bufferIndex = g_metalVertexBufferIndex;
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

        init_desc.colorFormat = passName == render_pass_name::SHADOW_PASS ? MTLPixelFormatInvalid : _contexts[0].getPixelFormat();
        init_desc.depthFormat = g_metalDepthTargetFormat;

        pso.init(_shaderLibraries.get(mat->getShaderName()), init_desc);

        std::printf("PSO %u created successfully\n", pso_handle.get());
    }

    return pso_handle;
}

render_target_handle m_dynamic_rhi::createRenderTarget(u32 width, u32 height, render_target_type rt_type, u32 count, bool skip_resources)
{
    auto& rt = _renderTargets.emplace_back();
    render_target_handle rt_handle(static_cast<u32>(_renderTargets.size()));

    if (skip_resources)
    {
        rt.initSwapchain();
        std::printf("Render Target %u created (skip_resources)\n", rt_handle.get());
        return rt_handle;
    }

    rt.init(_deviceHandle, width, height, rt_type, count);

    std::printf("Render Target %u created successfully\n", rt_handle.get());

    return rt_handle;
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

sampler_handle m_dynamic_rhi::createSampler(const sampler_desc& desc, resource_binding_mode /*binding_mode*/)
{
    auto& smpl = _samplers.emplace_back();
    
    smpl.init(_deviceHandle, desc);

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

void m_dynamic_rhi::loadTextureToGPU(const texture_handle& tex_handle, u32 /*frameIndex*/)
{
    auto it = _textures.find(tex_handle.get());
    if (it == _textures.end()) {
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
    
    buffer.init(_deviceHandle, bufferSize, g_framesInFlight, stage, g_metalConstantBufferOffsetAlignment);

    uniform_buffer_handle ub_handle(static_cast<u32>(_uniformBuffers.size()));

    std::printf("Uniform buffer %d created successfully\n", ub_handle.get());

    return ub_handle;
}

void m_dynamic_rhi::getUniformBufferMemory(const uniform_buffer_handle& ub_handle, u32 frameIndex, void*& memory_ptr)
{
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
