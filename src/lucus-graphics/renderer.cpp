#include "renderer.hpp"

#include "render_types.hpp"
#include "command_buffer.hpp"
#include "window.hpp"
#include "window_manager.hpp"

#include "texture_manager.hpp"

#include "dynamic_rhi.hpp"

using namespace lucus;

u32 renderer::g_frameIndex{0};

void renderer::init(window_handle handle)
{
    if (!_dynamicRHI)
    {
        _dynamicRHI = create_dynamic_rhi();
        _dynamicRHI->init(); // Device
        initDefaultResources();
    }
    
    if (_dynamicRHI) {
        auto window_context = _dynamicRHI->createWindowContext(handle);
        if (!window_context.is_valid())
        {
            // Handle error: Failed to create window context
            std::printf("Error: Failed to create window context for handle %u\n", handle.get());
        }
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
            mainPass(_currentScene.get(), context, cmd);

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
            pso_handle = _dynamicRHI->createPSO(matInst, render_pass_name::MAIN_PASS);
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
    const u32 v_width = 1024, v_height = 1024;

    if (!shadow_rt_handle.is_valid())
    {
        shadow_rt_handle = _dynamicRHI->createRenderTarget(g_framesInFlight,
            v_width,
            v_height,
            render_target_type::DEPTH_ONLY,
            render_pass_name::SHADOW_PASS,
            {});
    }

    cmd.emplaceCommand<gpu_image_barrier_command>(shadow_rt_handle,
        resource_state::UNDEFINED,
        resource_state::DEPTH_WRITE,
        image_barrier_aspect::DEPTH);

    // 2. ShadowPass Begin
    cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::SHADOW_PASS, v_width, v_height, shadow_rt_handle);

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, v_width, v_height);

    const directional_light* dir_light = scn->getDirectionalLight();
    uniform_buffer_handle view_handle = g_defaultViewBufferHandle;
    if (dir_light)
    {
        view_handle = dir_light->getViewProjHandle();
        if (!view_handle.is_valid())
        {
            view_handle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer));
            const_cast<directional_light*>(dir_light)->setViewProjHandle(view_handle);
        }

        updateFrameUniformBuffer(dir_light);
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

        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, view_handle, (u8)shader_binding::VIEW);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, obj_handle, (u8)shader_binding::OBJECT);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, g_defaultLightBufferHandle, (u8)shader_binding::LIGHT);

        texture_handle tex0 = g_defaultWhiteTexture->getHandle();
        assert(tex0.is_valid());
        cmd.emplaceCommand<gpu_bind_texture_command>(pso_handle, tex0, (u8)shader_binding::TEXTURE);

        texture_handle tex1 = g_defaultBlackTexture->getHandle();
        assert(tex1.is_valid());
        cmd.emplaceCommand<gpu_bind_texture_command>(pso_handle, tex1, (u8)shader_binding::SHADOW_MAP);

        cmd.emplaceCommand<gpu_bind_sampler_command>(pso_handle, g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(pso_handle, g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(); // FINISH BIND for DX12

        mesh_handle msh_handle = meshInst->getHandle();
        assert(msh_handle.is_valid());

        cmd.emplaceCommand<gpu_bind_vertex_command>(msh_handle, (u8)shader_binding::VERTEX);
        
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
    cmd.emplaceCommand<gpu_image_barrier_command>(shadow_rt_handle,
        resource_state::DEPTH_WRITE,
        resource_state::SHADER_READ,
        image_barrier_aspect::DEPTH);
}

void renderer::mainPass(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd)
{
    assert(scn);

    u32 v_width, v_height;
    _dynamicRHI->getWindowContextSize(ctx_handle, v_width, v_height);

    render_target_handle fb_handle = _dynamicRHI->getWindowContextRenderTarget(ctx_handle);

    if (v_width == 0 || v_height == 0 || !fb_handle.is_valid())
    {
        // TODO: error
        return;
    }

    cmd.emplaceCommand<gpu_render_pass_begin_command>(render_pass_name::MAIN_PASS, v_width, v_height, fb_handle);

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, v_width, v_height);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, v_width, v_height);

    const camera* cam = scn->getCamera();
    uniform_buffer_handle view_handle = g_defaultViewBufferHandle;
    if (cam)
    {
        view_handle = cam->getHandle();
        if (!view_handle.is_valid())
        {
            view_handle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer));
            const_cast<camera*>(cam)->setHandle(view_handle);
        }

        const float aspectRatio = static_cast<float>(v_width) / static_cast<float>(v_height);
        updateFrameUniformBuffer(cam, aspectRatio);
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

        // if (meshInst->getName().find("Plane") == std::string::npos)
        // {
        //     continue;
        // }

        uniform_buffer_handle obj_handle = obj->getHandle();
        assert(obj_handle.is_valid());
        
        pipeline_state_handle pso_handle = matInst->getPSOHandle();
        assert(pso_handle.is_valid());
        
        cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, view_handle, (u8)shader_binding::VIEW);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, matInst->useObjectUniformBuffer() ? obj_handle : g_defaultObjectBufferHandle, (u8)shader_binding::OBJECT);
        cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, light_handle, (u8)shader_binding::LIGHT);

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
        cmd.emplaceCommand<gpu_bind_texture_command>(pso_handle, tex0, (u8)shader_binding::TEXTURE);

        cmd.emplaceCommand<gpu_bind_render_target_command>(pso_handle, shadow_rt_handle, render_target_binding::DEPTH, (u8)shader_binding::SHADOW_MAP);

        cmd.emplaceCommand<gpu_bind_sampler_command>(pso_handle, g_defaultSamplerHandle, (u8)shader_binding::SAMPLER);
        cmd.emplaceCommand<gpu_bind_sampler_command>(pso_handle, g_shadowMapSamplerHandle, (u8)shader_binding::SHADOW_MAP_SAMPLER);

        cmd.emplaceCommand<gpu_bind_description_table_command>(); // FINISH BIND for DX12

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
            cmd.emplaceCommand<gpu_bind_vertex_command>(msh_handle, (u8)shader_binding::VERTEX);
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
}

void renderer::updateFrameUniformBuffer(const camera* cmr, float aspectRatio)
{
    if (cmr)
    {
        frame_uniform_buffer buffer;
        buffer.view = cmr->getViewMatrix();
        buffer.proj = cmr->getProjectionMatrix(aspectRatio);

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(cmr->getHandle(), g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &buffer, sizeof(buffer));
    }
}

void renderer::updateFrameUniformBuffer(const directional_light* dir_light)
{
    if (dir_light)
    {
        frame_uniform_buffer buffer;
        buffer.view = dir_light->getViewMatrix();
        buffer.proj = dir_light->getProjectionMatrix();

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
        buffer.ambient = glm::vec4(0.f, 0.f, 1.f, 0.1f);
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
        object_uniform_buffer oub;
        oub.model = math::transform_to_mat4(obj->getTransform());

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(obj->getHandle(), g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &oub, sizeof(oub));
    }
}

void renderer::initDefaultResources()
{
    g_defaultViewBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(frame_uniform_buffer));
    g_defaultObjectBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(object_uniform_buffer));
    g_defaultLightBufferHandle = _dynamicRHI->createUniformBuffer(sizeof(light_uniform_buffer), shader_binding_stage::BOTH);

    g_defaultSamplerHandle = _dynamicRHI->createSampler();
    g_shadowMapSamplerHandle = _dynamicRHI->createSampler(); // TODO: create new sampler

    g_defaultWhiteTexture.reset(texture::create_one_factory(255, 255, 255, 255));
    g_defaultBlackTexture.reset(texture::create_one_factory(0, 0, 0, 255));

    g_shadowMat.reset(material::create_factory("shadow_map"));
    g_shadowMat->setUseVertexIndexBuffers(true);
}
