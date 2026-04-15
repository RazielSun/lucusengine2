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
            texture_handle tex_handle = _dynamicRHI->loadTexture(tex_ptr.get());
            if (tex_handle.is_valid())
            {
                tex_ptr->setHandle(tex_handle);
                texture_manager::instance().textureLoaded(tex_ptr.get());
            }
        }

        texture_manager::instance().resetPendingTextures();
    }

    for (const auto& context : _dynamicRHI->getWindowContexts())
    {
        if (_currentScene)
        {
            gpu_command_buffer cmd;
            processScene(_currentScene.get(), context, cmd);
        }
    }

    g_frameIndex++;
}

void renderer::cleanup()
{
    _dynamicRHI.reset();
}

void renderer::processScene(const scene* scn, const window_context_handle& ctx_handle, gpu_command_buffer& cmd)
{
    assert(scn);

    const camera* cam = scn->getCamera();
    if (!cam)
    {
        // TODO: error
        return;
    }

    u32 v_width, v_height;
    _dynamicRHI->getWindowContextSize(ctx_handle, v_width, v_height);

    if (v_width == 0 || v_height == 0)
    {
        // TODO: error
        return;
    }

    cmd.emplaceCommand<gpu_render_pass_begin_command>(0, 0, v_width, v_height);

    cmd.emplaceCommand<gpu_set_viewport_command>(0, 0, v_width, v_height, 0.f, 1.f);
    cmd.emplaceCommand<gpu_set_scissor_command>(0, 0, v_width, v_height);

    uniform_buffer_handle cam_handle = cam->getHandle();
    if (!cam_handle.is_valid())
    {
        cam_handle = _dynamicRHI->createUniformBuffer(uniform_buffer_type::FRAME, sizeof(frame_uniform_buffer));
        const_cast<camera*>(cam)->setHandle(cam_handle);
    }

    const float aspectRatio = static_cast<float>(v_width) / static_cast<float>(v_height);
    updateFrameUniformBuffer(scn->getCamera(), aspectRatio);

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
        if (!obj_handle.is_valid()) {
            obj_handle = _dynamicRHI->createUniformBuffer(uniform_buffer_type::OBJECT, sizeof(object_uniform_buffer));
            obj->setHandle(obj_handle);
        }

        updateObjectUniformBuffer(obj);
        
        pipeline_state_handle pso_handle = matInst->getPSOHandle();
        if (!pso_handle.is_valid()) {
            pso_handle = _dynamicRHI->createPSO(matInst);
            matInst->setPSOHandle(pso_handle);
        }
        
        cmd.emplaceCommand<gpu_bind_pipeline_command>(pso_handle);

        u8 binding = 0;
        if (matInst->useFrameUniformBuffer())
        {
            cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, cam_handle, binding);
            binding++;
        }

        if (matInst->useObjectUniformBuffer())
        {
            cmd.emplaceCommand<gpu_bind_uniform_buffer_command>(pso_handle, obj_handle, binding);
            binding++;
        }

        for (const auto& tex_slot : matInst->getTextures())
        {
            texture_handle tex_handle = tex_slot.texture->getHandle();
            cmd.emplaceCommand<gpu_bind_texture_command>(pso_handle, tex_handle, binding); // TODO: use Slot0/1/2 ?
            binding++;
        }

        mesh_handle msh_handle = meshInst->getHandle();
        if (!msh_handle.is_valid()) {
            msh_handle = _dynamicRHI->createMesh(meshInst);
            meshInst->setHandle(msh_handle);
        }

        if (meshInst->hasVertices())
        {
            cmd.emplaceCommand<gpu_bind_vertex_command>(msh_handle);
        }
        
        const u32 indexCount = meshInst->getIndexCount();
        if (indexCount > 0)
        {
            cmd.emplaceCommand<gpu_bind_index_command>(msh_handle);
            cmd.emplaceCommand<gpu_draw_indexed_command>(indexCount);
        }
        else
        {
            const u32 vertexCount = meshInst->getVertexCount();
            cmd.emplaceCommand<gpu_draw_vertex_command>(vertexCount);
        }
    }

    cmd.emplaceCommand<gpu_render_pass_end_command>();

    _dynamicRHI->execute(ctx_handle, g_frameIndex, cmd);
}

void renderer::updateFrameUniformBuffer(const camera* cmr, float aspectRatio)
{
    if (cmr)
    {
        frame_uniform_buffer fub;
        fub.view = cmr->getViewMatrix();
        fub.proj = cmr->getProjectionMatrix(aspectRatio);

        void* buffer_memory;
        _dynamicRHI->getUniformBufferMemory(cmr->getHandle(), g_frameIndex, buffer_memory);
        std::memcpy(static_cast<u8*>(buffer_memory), &fub, sizeof(fub));
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