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

void renderer::add_window(window_handle handle)
{
    bool bFirstInit = false;
    if (!_dynamicRHI)
    {
        bFirstInit = true;
        _dynamicRHI = create_dynamic_rhi();
        _dynamicRHI->init(); // Device
    }
    
    if (_dynamicRHI)
    {
        auto window_context = _dynamicRHI->createWindowContext(handle);
        if (!window_context.is_valid())
        {
            // Handle error: Failed to create window context
            std::printf("Error: Failed to create window context for handle %u\n", handle.get());
        }
    }
    else
    {
        // Handle error: Renderer not initialized
        std::printf("Error: Renderer not initialized. Call init() before adding windows.\n");
    }

    if (bFirstInit)
    {
        initDefaultResources();
    }
}

void renderer::tick(float dt)
{
    const bool bNeedLoadTextures = texture_manager::instance().hasPendingTextures();
    if (bNeedLoadTextures)
    {
        for (auto& tex_ptr : texture_manager::instance().getPendingTextures())
        {
            texture_handle tex_handle = _dynamicRHI->createTexture(tex_ptr.get());
            if (tex_handle.is_valid())
            {
                _dynamicRHI->loadTextureToGPU(tex_handle, g_frameIndex);
                tex_ptr->setHandle(tex_handle);
                texture_manager::instance().textureLoaded(tex_ptr.get());
            }
        }

        texture_manager::instance().resetPendingTextures();

        g_frameIndex++;
        return;
    }

    for (const auto& context : _dynamicRHI->getWindowContexts())
    {
        if (_currentScene)
        {
            gpu_command_buffer cmd;

            prepareScene(_currentScene.get());
            shadowPass(_currentScene.get(), context, cmd);

            if (r_mode == render_mode::FORWARD)
            {
                forwardPass(_currentScene.get(), context, cmd);
            }
            else if (r_mode == render_mode::DEFERRED)
            {
                gbufferPass(_currentScene.get(), context, cmd);
                lightingPass(_currentScene.get(), context, cmd);
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

void renderer::shadowPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd)
{
    // Steps:
    // 1. Create custom framebuffer and other - for shadow map
    const u32 width = shadow_map_width, height = shadow_map_height;
    if (!shadow_rt_handle.is_valid())
    {
        throw std::runtime_error("shadowPass:: shadow render target is not valid!");
    }

    cmd.emplaceCommand<gpu_image_barrier_command>(shadow_rt_handle,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    // 2. ShadowPass Begin
    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::SHADOW_PASS, 0, 1, width, height);
    bpc.depthTarget = shadow_rt_handle;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, width, height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, width, height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const directional_light* dir_light = scn->getDirectionalLight();
    uniform_buffer_handle view_handle = g_defaultViewBufferHandle;
    if (dir_light)
    {
        view_handle = dir_light->getViewProjHandle();
        if (!view_handle.is_valid())
        {
            view_handle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer), shader_binding_stage::BOTH);
            const_cast<directional_light*>(dir_light)->setViewProjHandle(view_handle);
        }

        updateFrameUniformBuffer(dir_light, width, height);
    }

    // 3. Bind Commands
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

        cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::SHADOW_PASS); // DX12: ring descriptor tables + head

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

    // 4. ShadowPass End
    cmd.emplaceCommand<gpu_render_pass_end_command>();
    
    // 5. Transit to MainPass
    cmd.emplaceCommand<gpu_image_barrier_command>(
        shadow_rt_handle,
        resource_state::DEPTH_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::DEPTH, 0);
}

void renderer::forwardPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd)
{
    assert(scn);

    u32 v_width, v_height;
    _dynamicRHI->getWindowContextSize(ctx_handle, v_width, v_height);

    render_target_handle fb_color_handle = _dynamicRHI->getWindowContextRenderTarget(ctx_handle, render_target_type::COLOR);
    render_target_handle fb_depth_handle = _dynamicRHI->getWindowContextRenderTarget(ctx_handle, render_target_type::DEPTH);

    if (v_width == 0 || v_height == 0 || !fb_color_handle.is_valid())
    {
        throw std::runtime_error("forwardPass:: forward render target is not valid!");
    }

    cmd.emplaceCommand<gpu_image_barrier_command>(
        fb_color_handle,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        fb_depth_handle,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::FORWARD_PASS, 1, 1, v_width, v_height);
    bpc.colorTargets[0] = fb_color_handle;
    bpc.depthTarget = fb_depth_handle;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const camera* cam = scn->getCamera();
    uniform_buffer_handle view_handle = g_defaultViewBufferHandle;
    if (cam)
    {
        view_handle = cam->getHandle();
        if (!view_handle.is_valid())
        {
            view_handle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer), shader_binding_stage::BOTH);
            const_cast<camera*>(cam)->setHandle(view_handle);
        }

        updateFrameUniformBuffer(cam, v_width, v_height);
    }

    const directional_light* dir_light = scn->getDirectionalLight();
    uniform_buffer_handle light_handle = g_defaultLightBufferHandle;
    if (dir_light)
    {
        light_handle = dir_light->getHandle();
        if (!light_handle.is_valid())
        {
            light_handle = _dynamicRHI->createUniformBuffer(sizeof(light_uniform_buffer), shader_binding_stage::BOTH);
            const_cast<directional_light*>(dir_light)->setHandle(light_handle);
        }

        updateLightUniformBuffer(dir_light);
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
        if (!obj || !matInst || !meshInst)
        {
            std::printf("Warning: Render object / Mesh / Material is not valid.\n");
            continue;
        }

        // TODO: Implement logic to skip rendering certain objects in shadow pass based on material properties or object flags
        // if (meshInst->getName().find("Plane") == std::string::npos)
        // {
        //     continue;
        // }

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

        texture_handle tex0{};
        if (matInst->getTexturesCount() > 0)
        {
            const auto& tex_slot = matInst->getTextures()[0];
            tex0 = tex_slot.texture->getHandle();
        }
        else
        {
            tex0 = g_defaultWhiteTexture->getHandle();
        }
        cmd.emplaceCommand<gpu_bind_texture_command>(tex0, (u8)shader_binding::TEXTURE);

        cmd.emplaceCommand<gpu_bind_render_target_command>(shadow_rt_handle, render_target_type::DEPTH, (u8)shader_binding::SHADOW_MAP, 0);

        cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::FORWARD_PASS); // DX12: ring descriptor tables + head

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
        fb_color_handle,
        resource_state::COLOR_WRITE,
        resource_state::PRESENT,
        image_barrier_aspect::COLOR, 0);
}

void renderer::gbufferPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd)
{
    // Mental map for deferred rendering implementation:
    // Barrier: Transition main framebuffer to be writable
    // RenderPass Begin: GBuffer Pass - bind gbuffer render targets
    // Set viewport/scissor
    // For each render object:
    //   - Update object uniform buffer if needed
    //   - Bind PSO, descriptor sets, vertex/index buffers
    //   - Issue draw call
    // RenderPass End
    // Barrier: Transition gbuffer render targets to be readable for lighting pass

    assert(scn);

    u32 v_width, v_height;
    _dynamicRHI->getWindowContextSize(ctx_handle, v_width, v_height);

    if (v_width == 0 || v_height == 0)
    {
        throw std::runtime_error("gbufferPass:: gBuffer render target is not valid!");
    }

    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferA,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferB,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferC,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferDepth,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::GBUFFER_PASS, 3, 1, v_width, v_height);
    bpc.colorTargets[0] = g_gbufferA;
    bpc.colorTargets[1] = g_gbufferB;
    bpc.colorTargets[2] = g_gbufferC;
    bpc.depthTarget = g_gbufferDepth;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const camera* cam = scn->getCamera();
    uniform_buffer_handle view_handle = g_defaultViewBufferHandle;
    if (cam)
    {
        view_handle = cam->getHandle();
        if (!view_handle.is_valid())
        {
            view_handle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer), shader_binding_stage::BOTH);
            const_cast<camera*>(cam)->setHandle(view_handle);
        }

        updateFrameUniformBuffer(cam, v_width, v_height);
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

        texture_handle tex0{};
        if (matInst->getTexturesCount() > 0)
        {
            const auto& tex_slot = matInst->getTextures()[0];
            tex0 = tex_slot.texture->getHandle();
        }
        else
        {
            tex0 = g_defaultWhiteTexture->getHandle();
        }
        cmd.emplaceCommand<gpu_bind_texture_command>(tex0, (u8)shader_binding::TEXTURE);

        // we don't need it
        cmd.emplaceCommand<gpu_bind_render_target_command>(shadow_rt_handle, render_target_type::DEPTH, (u8)shader_binding::SHADOW_MAP, 0);

        cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::GBUFFER_PASS); // DX12: ring descriptor tables + head

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
        g_gbufferA,
        resource_state::COLOR_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferB,
        resource_state::COLOR_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::COLOR, 0);
    
    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferC,
        resource_state::COLOR_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        g_gbufferDepth,
        resource_state::DEPTH_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::DEPTH, 0);
}

void renderer::lightingPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd)
{
    // Metntal map for deferred rendering implementation:
    // Barrier: Transition main framebuffer to be writable, gbuffer render targets to be readable
    // RenderPass Begin: Lighting Pass - bind main framebuffer render target
    // Set viewport/scissor
    // Bind fullscreen quad pipeline state and descriptor sets (gbuffer textures, shadow map, etc.)
    // Issue draw call for fullscreen quad
    // RenderPass End

    assert(scn);

    u32 v_width, v_height;
    _dynamicRHI->getWindowContextSize(ctx_handle, v_width, v_height);

    render_target_handle fb_color_handle = _dynamicRHI->getWindowContextRenderTarget(ctx_handle, render_target_type::COLOR);
    render_target_handle fb_depth_handle = _dynamicRHI->getWindowContextRenderTarget(ctx_handle, render_target_type::DEPTH);

    if (v_width == 0 || v_height == 0 || !fb_color_handle.is_valid())
    {
        throw std::runtime_error("lightingPass:: render target is not valid!");
    }

    const camera* cam = scn->getCamera();
    uniform_buffer_handle view_handle = g_defaultViewBufferHandle;
    if (cam)
    {
        view_handle = cam->getHandle();
        // We updated it in gbuffer pass
    }

    cmd.emplaceCommand<gpu_image_barrier_command>(
        fb_color_handle,
        resource_state::UNDEFINED,
        resource_state::COLOR_WRITE,
        image_barrier_aspect::COLOR, 0);

    cmd.emplaceCommand<gpu_image_barrier_command>(
        fb_depth_handle,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH, 0);

    auto& bpc = cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::DEFERRED_LIGHTING_PASS, 1, 1, v_width, v_height);
    bpc.colorTargets[0] = fb_color_handle;
    bpc.depthTarget = fb_depth_handle;

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_bindless_command>();

    const directional_light* dir_light = scn->getDirectionalLight();
    uniform_buffer_handle light_handle = g_defaultLightBufferHandle;
    if (dir_light)
    {
        light_handle = dir_light->getHandle();
        if (!light_handle.is_valid())
        {
            light_handle = _dynamicRHI->createUniformBuffer(sizeof(light_uniform_buffer), shader_binding_stage::BOTH);
            const_cast<directional_light*>(dir_light)->setHandle(light_handle);
        }

        updateLightUniformBuffer(dir_light);
    }

    pipeline_state_handle pso_handle = g_deferredLightingMat->getPSOHandle();
    assert(pso_handle.is_valid());

    cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(view_handle, (u8)shader_binding::VIEW);
    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultObjectBufferHandle, (u8)shader_binding::OBJECT);
    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(g_defaultMaterialBufferHandle, (u8)shader_binding::MATERIAL);
    cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(light_handle, (u8)shader_binding::LIGHT);

    cmd.emplaceCommand<gpu_bind_texture_command>(g_defaultWhiteTexture->getHandle(), (u8)shader_binding::TEXTURE);

    cmd.emplaceCommand<gpu_bind_render_target_command>(shadow_rt_handle, render_target_type::DEPTH, (u8)shader_binding::SHADOW_MAP, 0);

    cmd.emplaceCommand<gpu_bind_render_target_command>(g_gbufferA, render_target_type::COLOR, (u8)shader_binding::GBUFFER_A, 0);
    cmd.emplaceCommand<gpu_bind_render_target_command>(g_gbufferB, render_target_type::COLOR, (u8)shader_binding::GBUFFER_B, 0);
    cmd.emplaceCommand<gpu_bind_render_target_command>(g_gbufferC, render_target_type::COLOR, (u8)shader_binding::GBUFFER_C, 0);
    cmd.emplaceCommand<gpu_bind_render_target_command>(g_gbufferDepth, render_target_type::DEPTH, (u8)shader_binding::GBUFFER_DEPTH, 0);

    cmd.emplaceCommand<gpu_bind_sampler_command>(g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
    cmd.emplaceCommand<gpu_bind_sampler_command>(g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);
    cmd.emplaceCommand<gpu_bind_sampler_command>(g_gbufferSamplerHandle, (u8)shader_binding::GBUFFER_SAMPLER);

    cmd.emplaceCommand<gpu_bind_description_table_command>(render_pass_name::DEFERRED_LIGHTING_PASS); // DX12: ring descriptor tables + head

    // FULLSCREEN TRIANGLE - we can use vertex shader to generate vertices, so no need to bind vertex/index buffers
    cmd.emplaceCommand<gpu_draw_vertex_command>(3);

    cmd.emplaceCommand<gpu_render_pass_end_command>();

    cmd.emplaceCommand<gpu_image_barrier_command>(
        fb_color_handle,
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

void renderer::updateFrameUniformBuffer(const camera *cmr, u32 width, u32 height)
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
        _dynamicRHI->getUniformBufferMemory(cmr->getHandle(), g_frameIndex, buffer_memory);
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
            const texture* tex = ts.texture.get();
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

    g_defaultSamplerHandle = _dynamicRHI->createSampler(resource_binding_mode::BINDFULL);
    g_defaultBindlessSamplerHandle = _dynamicRHI->createSampler(resource_binding_mode::BINDLESS);
    // TODO: create new sampler clamp, not repeat
    g_shadowMapSamplerHandle = _dynamicRHI->createSampler(resource_binding_mode::BINDFULL);

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
        u32 v_width, v_height;
        // TODO: 0 index from window contexts is not good, need to find better way to get main window size
        _dynamicRHI->getWindowContextSize(_dynamicRHI->getWindowContexts()[0], v_width, v_height);
        g_gbufferA = _dynamicRHI->createRenderTarget(v_width, v_height, render_target_type::COLOR, g_framesInFlight);
        g_gbufferB = _dynamicRHI->createRenderTarget(v_width, v_height, render_target_type::COLOR, g_framesInFlight);
        g_gbufferC = _dynamicRHI->createRenderTarget(v_width, v_height, render_target_type::COLOR, g_framesInFlight);
        g_gbufferDepth = _dynamicRHI->createRenderTarget(v_width, v_height, render_target_type::DEPTH, g_framesInFlight);
        g_gbufferSamplerHandle = _dynamicRHI->createSampler(resource_binding_mode::BINDFULL); 

        g_deferredLightingMat.reset(material::create_factory("deferred_lighting"));
        g_deferredLightingMat->setUseVertexIndexBuffers(false);
        g_deferredLightingMat->setDeferredMode(false);
    }

}
