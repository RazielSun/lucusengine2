#include "renderer.hpp"

#include <limits>

#include "render_types.hpp"
#include "command_buffer.hpp"
#include "window.hpp"
#include "window_manager.hpp"

#include "texture_manager.hpp"

#include "dynamic_rhi.hpp"

using namespace lucus;

u32 renderer::g_frameIndex{0};

const validation_status* renderer::getValidationStatus() const
{
    return _dynamicRHI ? &_dynamicRHI->getValidationStatus() : nullptr;
}

void renderer::add_window(window_handle handle)
{
    bool bFirstInit = false;
    if (!_dynamicRHI)
    {
        bFirstInit = true;
        _dynamicRHI = create_dynamic_rhi();
        _dynamicRHI->init(_rhiInitDesc);
    }
    
    if (_dynamicRHI)
    {
        auto window_context = _dynamicRHI->createWindowContext(handle);
        if (!window_context.is_valid())
        {
            std::printf("Error: Failed to create window context for handle %u\n", handle.get());
        }
    }
    else
    {
        std::printf("Error: Renderer not initialized. Call init() before adding windows.\n");
    }

    if (bFirstInit)
    {
        initDefaultResources();
    }
}

void renderer::uploadPendingTextures()
{
    auto& textures = texture_manager::instance();
    if (!textures.hasPendingTextures())
    {
        return;
    }

    for (auto& tex_ptr : textures.getPendingTextures())
    {
        texture_handle tex_handle = _dynamicRHI->createTexture(tex_ptr.get());
        if (!tex_handle.is_valid())
        {
            std::printf("Error: Failed to create GPU texture for '%s'\n", tex_ptr->getFileName().c_str());
            continue;
        }

        _dynamicRHI->loadTextureToGPU(tex_handle, g_frameIndex);
        tex_ptr->setHandle(tex_handle);
        textures.textureLoaded(tex_ptr.get());
    }

    textures.resetPendingTextures();
}

void renderer::tick(float dt)
{
    uploadPendingTextures();

    for (const auto& context : _dynamicRHI->getWindowContexts())
    {
        if (_currentScene)
        {
            gpu_command_buffer cmd;

            prepareScene(_currentScene.get());
            shadowPass(_currentScene.get(), cmd);

            if (r_mode == render_mode::DEFERRED)
            {
                _dynamicRHI->ensureDeferredGBufferTargets(context);
            }

            const pass_context pc = makePassContext(context);

            if (r_mode == render_mode::FORWARD)
            {
                forwardPass(_currentScene.get(), pc, cmd);
            }
            else if (r_mode == render_mode::DEFERRED)
            {
                gbufferPass(_currentScene.get(), pc, cmd);
                lightingPass(_currentScene.get(), pc, cmd);
            }

            _dynamicRHI->execute(context, g_frameIndex, cmd);
        }
    }

    g_frameIndex++;
}

void renderer::cleanup()
{
    _dynamicRHI.reset();
}

texture_handle renderer::baseColorTextureFor(const material* mat) const
{
    if (mat && mat->getTexturesCount() > 0)
    {
        const texture* tex = mat->getTextures()[0].tex.get();
        if (tex && tex->getHandle().is_valid())
        {
            return tex->getHandle();
        }
    }

    return g_defaultWhiteTexture->getHandle();
}

void renderer::prepareScene(const scene* scn)
{
    {
        pipeline_state_handle pso_handle = g_shadowMat->getPSOHandle();
        if (!pso_handle.is_valid()) {
            pso_handle = _dynamicRHI->createPSO(g_shadowMat.get(), render_pass_name::SHADOW_PASS);
            g_shadowMat->setPSOHandle(pso_handle);
        }
    }

    {
        if (r_mode == render_mode::DEFERRED)
        {
            pipeline_state_handle pso_handle = g_deferredLightingMat->getPSOHandle();
            if (!pso_handle.is_valid()) {
                pso_handle = _dynamicRHI->createPSO(g_deferredLightingMat.get(), render_pass_name::DEFERRED_LIGHTING_PASS);
                g_deferredLightingMat->setPSOHandle(pso_handle);
            }
        }
    }

    for (const auto& obj_ptr : scn->objects())
    {
        if (!obj_ptr)
        {
            std::printf("Warning: Null render object found in renderer's render object list.\n");
            continue;
        }

        render_object* obj = obj_ptr.get();
        material* matInst = obj->getMaterial();
        mesh* meshInst = obj->getMesh();

        uniform_buffer_handle obj_handle = obj->getHandle();
        if (!obj_handle.is_valid()) {
            obj_handle = _dynamicRHI->createUniformBuffer(sizeof(object_uniform_buffer));
            obj->setHandle(obj_handle);
        }

        updateObjectUniformBuffer(obj);

        pipeline_state_handle pso_handle = matInst->getPSOHandle();
        if (!pso_handle.is_valid()) {
            pso_handle = _dynamicRHI->createPSO(matInst, (r_mode == render_mode::FORWARD) ? render_pass_name::FORWARD_PASS : render_pass_name::GBUFFER_PASS);
            matInst->setPSOHandle(pso_handle);
        }

        mesh_handle msh_handle = meshInst->getHandle();
        if (!msh_handle.is_valid()) {
            msh_handle = _dynamicRHI->createMesh(meshInst);
            meshInst->setHandle(msh_handle);
        }
    }
}

pass_context renderer::makePassContext(const window_context_handle& ctx_handle)
{
    pass_context pc;
    pc.ctx = ctx_handle;

    _dynamicRHI->getWindowContextSize(ctx_handle, pc.width, pc.height);
    pc.color = _dynamicRHI->getWindowContextRenderTarget(ctx_handle, render_target_type::COLOR);
    pc.depth = _dynamicRHI->getWindowContextRenderTarget(ctx_handle, render_target_type::DEPTH);

    if (pc.width == 0 || pc.height == 0 || !pc.color.is_valid())
    {
        throw std::runtime_error("makePassContext:: window context render target is not valid!");
    }

    pc.view = viewBufferFor(ctx_handle);

    return pc;
}

uniform_buffer_handle renderer::viewBufferFor(const window_context_handle& ctx_handle)
{
    const u32 index = ctx_handle.as_index();
    if (index >= _viewBuffers.size())
    {
        _viewBuffers.resize(index + 1);
    }

    if (!_viewBuffers[index].is_valid())
    {
        _viewBuffers[index] = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer), shader_binding_stage::BOTH);
    }

    return _viewBuffers[index];
}

const window_gbuffer_targets& renderer::requireGBufferTargets(const pass_context& pc, const char* pass_name)
{
    const window_gbuffer_targets& gb = _dynamicRHI->getWindowContextGBufferTargets(pc.ctx);
    if (!gb.valid())
    {
        throw std::runtime_error(std::string(pass_name) + ":: per-window G-buffer targets are not valid!");
    }
    return gb;
}

uniform_buffer_handle renderer::ensureViewBuffer(const camera* cmr, const pass_context& pc)
{
    if (!cmr)
    {
        return g_defaultViewBufferHandle;
    }

    updateFrameUniformBuffer(cmr, pc.view, pc.width, pc.height);
    return pc.view;
}

uniform_buffer_handle renderer::ensureShadowViewBuffer(const directional_light* dir_light, u32 width, u32 height)
{
    if (!dir_light)
    {
        return g_defaultViewBufferHandle;
    }

    uniform_buffer_handle handle = dir_light->getViewProjHandle();
    if (!handle.is_valid())
    {
        handle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer), shader_binding_stage::BOTH);
        const_cast<directional_light*>(dir_light)->setViewProjHandle(handle);
    }

    updateFrameUniformBuffer(dir_light, width, height);
    return handle;
}

uniform_buffer_handle renderer::ensureLightBuffer(const directional_light* dir_light)
{
    if (!dir_light)
    {
        return g_defaultLightBufferHandle;
    }

    uniform_buffer_handle handle = dir_light->getHandle();
    if (!handle.is_valid())
    {
        handle = _dynamicRHI->createUniformBuffer(sizeof(light_uniform_buffer), shader_binding_stage::BOTH);
        const_cast<directional_light*>(dir_light)->setHandle(handle);
    }

    updateLightUniformBuffer(dir_light);
    return handle;
}

void renderer::shadowPass(const scene* scn, gpu_command_buffer& cmd)
{
    const u32 width = shadow_map_width, height = shadow_map_height;
    if (!shadow_rt_handle.is_valid())
    {
        throw std::runtime_error("shadowPass:: shadow render target is not valid!");
    }

    cmd.emplaceCommand<gpu_image_barrier_command>(shadow_rt_handle,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::SHADOW_PASS, 0, 1, width, height);
    bpc.depthTarget = shadow_rt_handle;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, width, height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, width, height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const uniform_buffer_handle view_handle = ensureShadowViewBuffer(scn->getDirectionalLight(), width, height);

    for (const auto& obj_ptr : scn->objects())
    {
        if (!obj_ptr)
        {
            std::printf("Warning: Null render object found in renderer's render object list.\n");
            continue;
        }

        render_object* obj = obj_ptr.get();
        mesh* meshInst = obj->getMesh();
        if (!obj || !meshInst)
        {
            std::printf("Warning: SHADOW PASS - Render object / Mesh is not valid.\n");
            continue;
        }

        if (!obj->getCastShadow())
        {
            continue;
        }

        if (!meshInst->hasVertices())
        {
            continue;
        }

        uniform_buffer_handle obj_handle = obj->getHandle();
        assert(obj_handle.is_valid());
        
        pipeline_state_handle pso_handle = g_shadowMat->getPSOHandle();
        assert(pso_handle.is_valid());

        cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(view_handle, (u8)shader_binding::VIEW);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(obj_handle, (u8)shader_binding::OBJECT);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultMaterialBufferHandle, (u8)shader_binding::MATERIAL);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultLightBufferHandle, (u8)shader_binding::LIGHT);

        texture_handle tex0 = g_defaultWhiteTexture->getHandle();
        assert(tex0.is_valid());
        cmd.emplaceCommand<gpu_bind_texture_command>(tex0, (u8)shader_binding::TEXTURE);

        texture_handle tex1 = g_defaultBlackTexture->getHandle();
        assert(tex1.is_valid());
        cmd.emplaceCommand<gpu_bind_texture_command>(tex1, (u8)shader_binding::SHADOW_MAP);

        cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::SHADOW_PASS);

        mesh_handle msh_handle = meshInst->getHandle();
        assert(msh_handle.is_valid());

        cmd.emplaceCommand<gpu_bind_vertex_command>(msh_handle, 0);

        const u32 indexCount = meshInst->getIndexCount();
        if (indexCount > 0)
        {
            cmd.emplaceCommand<gpu_draw_indexed_command>(msh_handle, indexCount);
        }
        else
        {
            const u32 vertexCount = meshInst->getVertexCount();
            cmd.emplaceCommand<gpu_draw_vertex_command>(vertexCount);
        }
    }

    cmd.emplaceCommand<gpu_render_pass_end_command>();
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        shadow_rt_handle,
        resource_state::DEPTH_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::DEPTH, 0);
}

void renderer::forwardPass(const scene* scn, const pass_context& pc, gpu_command_buffer& cmd)
{
    assert(scn);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        pc.color,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        pc.depth,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::FORWARD_PASS, 1, 1, pc.width, pc.height);
    bpc.colorTargets[0] = pc.color;
    bpc.depthTarget = pc.depth;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, pc.width, pc.height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, pc.width, pc.height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const uniform_buffer_handle view_handle = ensureViewBuffer(scn->getCamera(), pc);
    const uniform_buffer_handle light_handle = ensureLightBuffer(scn->getDirectionalLight());

    for (const auto& obj_ptr : scn->objects())
    {
        if (!obj_ptr)
        {
            std::printf("Warning: Null render object found in renderer's render object list.\n");
            continue;
        }

        render_object* obj = obj_ptr.get();
        material* matInst = obj->getMaterial();
        mesh* meshInst = obj->getMesh();
        if (!obj || !matInst || !meshInst)
        {
            std::printf("Warning: Render object / Mesh / Material is not valid.\n");
            continue;
        }

        // TODO: Implement logic to skip rendering certain objects in shadow pass based on material properties or object flags

        uniform_buffer_handle obj_handle = obj->getHandle();
        assert(obj_handle.is_valid());
        
        pipeline_state_handle pso_handle = matInst->getPSOHandle();
        assert(pso_handle.is_valid());
        
        cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(view_handle, (u8)shader_binding::VIEW);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(matInst->useObjectUniformBuffer() ? obj_handle : g_defaultObjectBufferHandle, (u8)shader_binding::OBJECT);
        updateMaterialUniformBuffer(matInst);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultMaterialBufferHandle, (u8)shader_binding::MATERIAL);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(light_handle, (u8)shader_binding::LIGHT);

        cmd.emplaceCommand<gpu_bind_texture_command>(baseColorTextureFor(matInst), (u8)shader_binding::TEXTURE);

        cmd.emplaceCommand<gpu_bind_render_target_command>(shadow_rt_handle, render_target_type::DEPTH, (u8)shader_binding::SHADOW_MAP, 0);

        cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::FORWARD_PASS);

        mesh_handle msh_handle = meshInst->getHandle();
        assert(msh_handle.is_valid());

        if (matInst->useVertexIndexBuffers() && !meshInst->hasVertices())
        {
            std::printf("Warning: Material '%s' expects vertex/index buffers, but mesh '%s' has no vertices.\n",
                matInst->getShaderName().c_str(),
                meshInst->getName().c_str());
            continue;
        }

        if (meshInst->hasVertices())
        {
            cmd.emplaceCommand<gpu_bind_vertex_command>(msh_handle, 0);
        }

        const u32 indexCount = meshInst->getIndexCount();
        if (indexCount > 0)
        {
            cmd.emplaceCommand<gpu_draw_indexed_command>(msh_handle, indexCount);
        }
        else
        {
            const u32 vertexCount = meshInst->getVertexCount();
            cmd.emplaceCommand<gpu_draw_vertex_command>(vertexCount);
        }
    }

    cmd.emplaceCommand<gpu_render_pass_end_command>();

    cmd.emplaceCommand<gpu_image_barrier_command>(
        pc.color,
        resource_state::COLOR_WRITE,
        resource_state::PRESENT,
        image_barrier_aspect::COLOR, 0);
}

void renderer::gbufferPass(const scene* scn, const pass_context& pc, gpu_command_buffer& cmd)
{

    assert(scn);

    const window_gbuffer_targets& gb = requireGBufferTargets(pc, "gbufferPass");

    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.a,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.b,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.c,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.depth,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::GBUFFER_PASS, 3, 1, pc.width, pc.height);
    bpc.colorTargets[0] = gb.a;
    bpc.colorTargets[1] = gb.b;
    bpc.colorTargets[2] = gb.c;
    bpc.depthTarget = gb.depth;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, pc.width, pc.height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, pc.width, pc.height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const uniform_buffer_handle view_handle = ensureViewBuffer(scn->getCamera(), pc);

    for (const auto& obj_ptr : scn->objects())
    {
        if (!obj_ptr)
        {
            std::printf("Warning: Null render object found in renderer's render object list.\n");
            continue;
        }

        render_object* obj = obj_ptr.get();
        material* matInst = obj->getMaterial();
        mesh* meshInst = obj->getMesh();
        if (!obj || !matInst || !meshInst)
        {
            std::printf("Warning: Render object / Mesh / Material is not valid.\n");
            continue;
        }

        uniform_buffer_handle obj_handle = obj->getHandle();
        assert(obj_handle.is_valid());
        
        pipeline_state_handle pso_handle = matInst->getPSOHandle();
        assert(pso_handle.is_valid());
        
        cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(view_handle, (u8)shader_binding::VIEW);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(matInst->useObjectUniformBuffer() ? obj_handle : g_defaultObjectBufferHandle, (u8)shader_binding::OBJECT);
        updateMaterialUniformBuffer(matInst);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultMaterialBufferHandle, (u8)shader_binding::MATERIAL);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultLightBufferHandle, (u8)shader_binding::LIGHT);

        cmd.emplaceCommand<gpu_bind_texture_command>(baseColorTextureFor(matInst), (u8)shader_binding::TEXTURE);

        cmd.emplaceCommand<gpu_bind_render_target_command>(shadow_rt_handle, render_target_type::DEPTH, (u8)shader_binding::SHADOW_MAP, 0);

        cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::GBUFFER_PASS);

        mesh_handle msh_handle = meshInst->getHandle();
        assert(msh_handle.is_valid());

        if (matInst->useVertexIndexBuffers() && !meshInst->hasVertices())
        {
            std::printf("Warning: Material '%s' expects vertex/index buffers, but mesh '%s' has no vertices.\n",
                matInst->getShaderName().c_str(),
                meshInst->getName().c_str());
            continue;
        }

        if (meshInst->hasVertices())
        {
            cmd.emplaceCommand<gpu_bind_vertex_command>(msh_handle, 0);
        }

        const u32 indexCount = meshInst->getIndexCount();
        if (indexCount > 0)
        {
            cmd.emplaceCommand<gpu_draw_indexed_command>(msh_handle, indexCount);
        }
        else
        {
            const u32 vertexCount = meshInst->getVertexCount();
            cmd.emplaceCommand<gpu_draw_vertex_command>(vertexCount);
        }
    }

    cmd.emplaceCommand<gpu_render_pass_end_command>();

    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.a,
        resource_state::COLOR_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.b,
        resource_state::COLOR_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.c,
        resource_state::COLOR_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        gb.depth,
        resource_state::DEPTH_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::DEPTH, 0);
}

void renderer::lightingPass(const scene* scn, const pass_context& pc, gpu_command_buffer& cmd)
{

    assert(scn);

    const window_gbuffer_targets& gb = requireGBufferTargets(pc, "lightingPass");

    const uniform_buffer_handle view_handle = scn->getCamera() ? pc.view : g_defaultViewBufferHandle;

    cmd.emplaceCommand<gpu_image_barrier_command>(
        pc.color,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        pc.depth,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::DEFERRED_LIGHTING_PASS, 1, 1, pc.width, pc.height);
    bpc.colorTargets[0] = pc.color;
    bpc.depthTarget = pc.depth;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, pc.width, pc.height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, pc.width, pc.height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const uniform_buffer_handle light_handle = ensureLightBuffer(scn->getDirectionalLight());

    pipeline_state_handle pso_handle = g_deferredLightingMat->getPSOHandle();
    assert(pso_handle.is_valid());

    cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(view_handle, (u8)shader_binding::VIEW);
    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultObjectBufferHandle, (u8)shader_binding::OBJECT);
    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultMaterialBufferHandle, (u8)shader_binding::MATERIAL);
    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(light_handle, (u8)shader_binding::LIGHT);

    cmd.emplaceCommand<gpu_bind_texture_command>(g_defaultWhiteTexture->getHandle(), (u8)shader_binding::TEXTURE);

    cmd.emplaceCommand<gpu_bind_render_target_command>(shadow_rt_handle, render_target_type::DEPTH, (u8)shader_binding::SHADOW_MAP, 0);

    cmd.emplaceCommand<gpu_bind_render_target_command>(gb.a, render_target_type::COLOR, (u8)shader_binding::GBUFFER_A, 0);
    cmd.emplaceCommand<gpu_bind_render_target_command>(gb.b, render_target_type::COLOR, (u8)shader_binding::GBUFFER_B, 0);
    cmd.emplaceCommand<gpu_bind_render_target_command>(gb.c, render_target_type::COLOR, (u8)shader_binding::GBUFFER_C, 0);
    cmd.emplaceCommand<gpu_bind_render_target_command>(gb.depth, render_target_type::DEPTH, (u8)shader_binding::GBUFFER_DEPTH, 0);

    cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
    cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);
    cmd.emplaceCommand<gpu_bind_sampler_command>(g_gbufferSamplerHandle, (u8)shader_binding::GBUFFER_SAMPLER);

    cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::DEFERRED_LIGHTING_PASS);

    cmd.emplaceCommand<gpu_draw_vertex_command>(3);

    cmd.emplaceCommand<gpu_render_pass_end_command>();

    cmd.emplaceCommand<gpu_image_barrier_command>(
        pc.color,
        resource_state::COLOR_WRITE,
        resource_state::PRESENT,
        image_barrier_aspect::COLOR, 0);
}

void renderer::updateFrameUniformBuffer(const uniform_buffer_handle &handle, u32 width, u32 height)
{
    frame_uniform_buffer buffer;
    buffer.other = glm::vec4((float)width, (float)height, 0.0f, 0.0f);

    void* buffer_memory;
    _dynamicRHI->getUniformBufferMemory(handle, g_frameIndex, buffer_memory);
    std::memcpy(static_cast<u8*>(buffer_memory), &buffer, sizeof(buffer));
}

void renderer::updateFrameUniformBuffer(const camera *cmr, const uniform_buffer_handle& handle, u32 width, u32 height)
{
    if (cmr)
    {
        frame_uniform_buffer buffer;
        buffer.view = cmr->getViewMatrix();
        buffer.proj = cmr->getProjectionMatrix((float)width / (float)height);
        buffer.invViewProj = glm::inverse(buffer.proj * buffer.view);
        buffer.position = glm::vec4(cmr->getPosition(), 1.0f);
        buffer.other = glm::vec4((float)width, (float)height, 0.0f, 0.0f);

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(handle, g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &buffer, sizeof(buffer));
    }
}

void renderer::updateFrameUniformBuffer(const directional_light* dir_light, u32 width, u32 height)
{
    if (dir_light)
    {
        frame_uniform_buffer buffer;
        buffer.view = dir_light->getViewMatrix();
        buffer.proj = dir_light->getProjectionMatrix();
        buffer.position = glm::vec4(dir_light->getPosition(), 1.0f);
        buffer.other = glm::vec4((float)width, (float)height, 0.0f, 0.0f);

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(dir_light->getViewProjHandle(), g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &buffer, sizeof(buffer));
    }
}

void renderer::updateLightUniformBuffer(const directional_light* dir_light)
{
    if (dir_light)
    {
        light_uniform_buffer buffer;
        buffer.position = glm::vec4(dir_light->getPosition(), 1.0f);
        buffer.direction = glm::vec4(dir_light->getDirection(), 0.0f);
        buffer.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
        buffer.ambient = glm::vec4(1.f, 1.f, 1.f, 0.1f);
        buffer.viewProj = dir_light->getProjectionMatrix() * dir_light->getViewMatrix();

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(dir_light->getHandle(), g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &buffer, sizeof(buffer));
    }
}

void renderer::updateObjectUniformBuffer(const render_object* obj)
{
    if (obj)
    {
        object_uniform_buffer oub{};
        oub.model = math::transform_to_mat4(obj->getTransform());

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(obj->getHandle(), g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &oub, sizeof(oub));
    }
}

void renderer::updateMaterialUniformBuffer(const material* mat)
{
    material_uniform_buffer ub{};
    ub.base_color = glm::vec4(1.0f);
    ub.material_params = glm::vec4(0.0f);

    u32 flags = 0;
    u32 albedoSlot = 0;
    u32 normalSlot = 0;
    u32 rmeSlot = 0;

    u32 bindlessInvalidFallbackSlot = g_bindlessFallbackBlackTexture->getBindlessTextureSlot();
    if (bindlessInvalidFallbackSlot == std::numeric_limits<u32>::max())
    {
        bindlessInvalidFallbackSlot = 0;
    }

    if (mat)
    {
        for (const texture_slot& ts : mat->getTextures())
        {
            const texture* tex = ts.tex.get();
            if (!tex)
            {
                continue;
            }

            const bool bindless = (tex->getResourceBindingMode() == resource_binding_mode::BINDLESS);
            u32 idx = tex->getBindlessTextureSlot();
            if (idx == std::numeric_limits<u32>::max())
            {
                idx = bindlessInvalidFallbackSlot;
            }

            switch (ts.slot_index)
            {
            case 0:
                flags |= material_tex_flag_has_albedo;
                if (bindless)
                {
                    flags |= material_tex_flag_bindless_albedo;
                }
                albedoSlot = idx;
                break;
            case 1:
                flags |= material_tex_flag_has_normal;
                if (bindless)
                {
                    flags |= material_tex_flag_bindless_normal;
                }
                normalSlot = idx;
                break;
            case 2:
                flags |= material_tex_flag_has_rme;
                if (bindless)
                {
                    flags |= material_tex_flag_bindless_rme;
                }
                rmeSlot = idx;
                break;
            default:
                break;
            }
        }
    }

    ub.texture_info = glm::uvec4(flags, albedoSlot, normalSlot, rmeSlot);

    void* buffer_memory = nullptr;
    _dynamicRHI->getUniformBufferMemory(g_defaultMaterialBufferHandle, g_frameIndex, buffer_memory);
    std::memcpy(static_cast<u8*>(buffer_memory), &ub, sizeof(ub));
}

void renderer::initDefaultResources()
{
    g_defaultViewBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer), shader_binding_stage::BOTH);
    g_defaultObjectBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(object_uniform_buffer));
    g_defaultMaterialBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(material_uniform_buffer), shader_binding_stage::BOTH);
    g_defaultLightBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(light_uniform_buffer), shader_binding_stage::BOTH);

    material_uniform_buffer default_mat{};
    for (u32 f = 0; f < g_framesInFlight; ++f)
    {
        void* mat_mem = nullptr;
        _dynamicRHI->getUniformBufferMemory(g_defaultMaterialBufferHandle, f, mat_mem);
        std::memcpy(static_cast<u8*>(mat_mem), &default_mat, sizeof(default_mat));
    }

    g_defaultSamplerHandle = _dynamicRHI->createSampler({}, resource_binding_mode::BINDFULL);
    g_defaultBindlessSamplerHandle = _dynamicRHI->createSampler({}, resource_binding_mode::BINDLESS);

    sampler_desc shadow_sampler_desc;
    shadow_sampler_desc.addressMode = sampler_address_mode::CLAMP_TO_BORDER;
    shadow_sampler_desc.borderColor = sampler_border_color::OPAQUE_WHITE;
    g_shadowMapSamplerHandle = _dynamicRHI->createSampler(shadow_sampler_desc, resource_binding_mode::BINDFULL);

    g_bindlessFallbackBlackTexture.reset(texture::create_one_factory(0, 0, 0, 255));
    g_bindlessFallbackBlackTexture->setResourceBindingMode(resource_binding_mode::BINDLESS);
    g_bindlessFallbackWhiteTexture.reset(texture::create_one_factory(255, 255, 255, 255));
    g_bindlessFallbackWhiteTexture->setResourceBindingMode(resource_binding_mode::BINDLESS);

    g_defaultWhiteTexture.reset(texture::create_one_factory(255, 255, 255, 255));
    g_defaultBlackTexture.reset(texture::create_one_factory(0, 0, 0, 255));

    g_shadowMat.reset(material::create_factory("shadow_map"));
    g_shadowMat->setUseVertexIndexBuffers(true);
    g_shadowMat->setDeferredMode(false);

    shadow_rt_handle = _dynamicRHI->createRenderTarget(shadow_map_width, shadow_map_height, render_target_type::DEPTH, g_framesInFlight);

    if (r_mode == render_mode::DEFERRED)
    {
        sampler_desc gbuffer_sampler_desc;
        gbuffer_sampler_desc.addressMode = sampler_address_mode::CLAMP_TO_EDGE;
        g_gbufferSamplerHandle = _dynamicRHI->createSampler(gbuffer_sampler_desc, resource_binding_mode::BINDFULL);

        g_deferredLightingMat.reset(material::create_factory("deferred_lighting"));
        g_deferredLightingMat->setUseVertexIndexBuffers(false);
        g_deferredLightingMat->setDeferredMode(false);
    }

}
