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
        auto viewport = _dynamicRHI->createViewport(handle);
        if (!_mainViewport.is_valid()) {
            _mainViewport = viewport;
        }
    }
}

void renderer::tick(float dt)
{
    _dynamicRHI->beginFrame(_mainViewport);

    command_buffer cmd;
    updateFrameUniformBuffer(cmd.frame_ubo);
    processObjects(cmd);

    _dynamicRHI->submit(cmd);

    _dynamicRHI->endFrame();
}

void renderer::cleanup()
{
    _dynamicRHI.reset();
}

render_object* renderer::emplaceRenderObject()
{
    render_object* renderObject = new render_object();
    _renderObjects.push_back(intrusive_ptr<render_object>(renderObject));
    return renderObject;
}

void renderer::updateFrameUniformBuffer(frame_uniform_buffer& ubo)
{
    if (_camera) {
        ubo.model = glm::mat4(1.0f); // TEST ONLY
        ubo.view = _camera->getViewMatrix();
        ubo.proj = _camera->getProjectionMatrix();
    }
}

void renderer::processObjects(command_buffer& cmd)
{
    for (const auto& renderObject : _renderObjects)
    {
        if (!renderObject)
        {
            std::printf("Warning: Null render object found in renderer's render object list.\n");
            continue;
        }

        render_object* obj = renderObject.get();
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
        // instance.mesh = meshInst->getHandle();
        instance.material = materialInst->getHandle();
        if (!instance.material.is_valid()) {
            instance.material = _dynamicRHI->createMaterial(materialInst);
            materialInst->setHandle(instance.material);
        }
        cmd.render_list.push_back(instance);
    }
}
