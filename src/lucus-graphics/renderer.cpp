#include "renderer.hpp"

#include "render_types.hpp"
#include "window.hpp"
#include "window_manager.hpp"

#include "dynamic_rhi.hpp"

using namespace lucus;

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
    for (const auto& context : _dynamicRHI->getWindowContexts())
    {
        if (_currentScene)
        {
            command_buffer cmd;
            processScene(_currentScene.get(), context, cmd);

            _dynamicRHI->beginFrame(context);
            _dynamicRHI->submit(context, cmd);
            _dynamicRHI->endFrame(context);
        }
    }
}

void renderer::cleanup()
{
    _dynamicRHI.reset();
}

void renderer::processScene(const scene* scn, const window_context_handle& ctx_handle, command_buffer& cmd)
{
    assert(scn);

    updateFrameUniformBuffer(scn->getCamera(), ctx_handle, cmd.frame_ubo);

    for (const auto& obj_ptr : scn->objects())
    {
        if (!obj_ptr)
        {
            std::printf("Warning: Null render object found in renderer's render object list.\n");
            continue;
        }

        render_object* obj = obj_ptr.get();
        mesh* meshInst = obj->getMesh();
        if (!meshInst) {
            std::printf("Warning: Render object has no mesh assigned.\n");
            continue;
        }

        material* materialInst = obj->getMaterial();
        if (!materialInst) {
            std::printf("Warning: Render object has no material assigned.\n");
            continue;
        }
        
        render_instance instance;
        instance.object = obj->getHandle();
        if (!instance.object.is_valid()) {
            instance.object = _dynamicRHI->createUniformBuffer(obj);
            obj->setHandle(instance.object);
        }
        instance.object_data.model = math::transform_to_mat4(obj->getTransform());

        instance.mesh = meshInst->getHandle();
        if (!instance.mesh.is_valid()) {
            instance.mesh = _dynamicRHI->createMesh(meshInst);
            meshInst->setHandle(instance.mesh);
        }

        instance.material = materialInst->getHandle();
        if (!instance.material.is_valid()) {
            instance.material = _dynamicRHI->createMaterial(materialInst);
            materialInst->setHandle(instance.material);
        }

        cmd.render_list.push_back(instance);
    }
}

void renderer::updateFrameUniformBuffer(const camera* cmr, const window_context_handle& ctx_handle, frame_uniform_buffer& ubo)
{
    if (cmr)
    {
        ubo.view = cmr->getViewMatrix();

        float aspectRatio = _dynamicRHI->getWindowContextAspectRatio(ctx_handle);
        ubo.proj = cmr->getProjectionMatrix(aspectRatio);
    }
}
