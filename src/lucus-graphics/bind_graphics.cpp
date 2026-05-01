#include "bind_graphics.hpp"

#include "script_manager.hpp"

#include "window_manager.hpp"
#include "texture_manager.hpp"
#include "renderer.hpp"
#include "render_object.hpp"
#include "camera.hpp"
#include "scene.hpp"

#include "gltf_utils.hpp"

#include "bind_math.hpp"

namespace utils
{
    constexpr const char* window_manager_class_name = "WindowManagerClass";
    constexpr const char* window_manager_global_name = "g_window_manager";

    constexpr const char* renderer_class_name = "RendererClass";
    constexpr const char* renderer_global_name = "g_renderer";

    constexpr const char* texture_manager_class_name = "TextureManagerClass";
    constexpr const char* texture_manager_global_name = "g_texture_manager";

    constexpr const char* mesh_class_name = "Mesh";
    constexpr const char* texture_class_name = "Texture";
    constexpr const char* material_class_name = "Material";
    constexpr const char* render_object_class_name = "RenderObject";
    constexpr const char* camera_class_name = "Camera";
    constexpr const char* dir_light_class_name = "DirectionalLight";
    constexpr const char* scene_class_name = "Scene";
}

namespace lucus
{
    void bind_mesh_class();
    void bind_texture_class();
    void bind_material_class();
    void bind_render_object_class();
    void bind_camera_class();
    void bind_dir_light_class();
    void bind_scene_class();

    void bind_render_mode_enum();

    void bind_window_manager_class_and_object();
    void bind_renderer_class_and_object();
    void bind_texture_manager_class_and_object();
}

void lucus::bind_graphics_module()
{
    bind_window_manager_class_and_object();

    bind_render_mode_enum();

    bind_mesh_class();
    bind_texture_class();
    bind_material_class();
    bind_render_object_class();
    bind_camera_class();
    bind_dir_light_class();
    bind_scene_class();

    bind_renderer_class_and_object();
    // bind_texture_manager_class_and_object();
}

void lucus::bind_render_mode_enum()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    r = engine->RegisterEnum("RenderMode"); assert(r >= 0);
    r = engine->RegisterEnumValue("RenderMode", "FORWARD",  (int)render_mode::FORWARD);  assert(r >= 0);
    r = engine->RegisterEnumValue("RenderMode", "DEFERRED", (int)render_mode::DEFERRED); assert(r >= 0);
}

void lucus::bind_window_manager_class_and_object()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(window_manager_class_name, 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = engine->RegisterObjectMethod(
        window_manager_class_name,
        "void CreateWindow(int, int, const string &in)",
        asMETHOD(window_manager, createWindow),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string property = std::string(window_manager_class_name) + " " + window_manager_global_name;
    r = engine->RegisterGlobalProperty(property.c_str(), &window_manager::instance()); assert(r >= 0);
}

void lucus::bind_renderer_class_and_object()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(renderer_class_name, 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    std::string method_set_current_scene = std::string("void SetCurrentScene(") + scene_class_name + "@)";
    r = engine->RegisterObjectMethod(
        renderer_class_name,
        method_set_current_scene.c_str(),
        asMETHOD(renderer, setCurrentScene),
        asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectMethod(
        renderer_class_name,
        "void SetRenderMode(RenderMode)",
        asMETHOD(renderer, setRenderMode),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string property = std::string(renderer_class_name) + " " + renderer_global_name;
    r = engine->RegisterGlobalProperty(property.c_str(), &renderer::instance()); assert(r >= 0);
}

void lucus::bind_mesh_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(mesh_class_name, 0, asOBJ_REF); assert(r >= 0);

    std::string method_factory = std::string(mesh_class_name) + "@ f(const string &in, int)";
    r = engine->RegisterObjectBehaviour(
        mesh_class_name,
        asBEHAVE_FACTORY,
        method_factory.c_str(),
        asFUNCTION(mesh::create_factory),
        asCALL_CDECL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        mesh_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(mesh, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        mesh_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(mesh, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->SetDefaultNamespace(mesh_class_name); assert(r >= 0);
    std::string create_cube = std::string(mesh_class_name) + std::string("@ cube()");
    r = engine->RegisterGlobalFunction(
        create_cube.c_str(),
        asFUNCTION(create_cube_factory),
        asCALL_CDECL
    ); assert(r >= 0);
    std::string create_triangle = std::string(mesh_class_name) + std::string("@ triangle()");
    r = engine->RegisterGlobalFunction(
        create_triangle.c_str(),
        asFUNCTION(create_triangle_factory),
        asCALL_CDECL
    ); assert(r >= 0);
    std::string load_mesh = std::string(mesh_class_name) + std::string("@ load(const string &in)");
    r = engine->RegisterGlobalFunction(
        load_mesh.c_str(),
        asFUNCTION(load_mesh_gltf),
        asCALL_CDECL
    ); assert(r >= 0);
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

void lucus::bind_material_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(material_class_name, 0, asOBJ_REF); assert(r >= 0);

    std::string method_create_factory = std::string(material_class_name) + "@ f(const string &in)";
    r = engine->RegisterObjectBehaviour(
        material_class_name,
        asBEHAVE_FACTORY,
        method_create_factory.c_str(),
        asFUNCTION(material::create_factory),
        asCALL_CDECL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        material_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(material, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        material_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(material, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectMethod(
        material_class_name,
        "void SetShaderName(const string &in)",
        asMETHOD(material, setShaderName),
        asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectMethod(
        material_class_name,
        "void SetUseFrameUniformBuffer(bool)",
        asMETHOD(material, setUseFrameUniformBuffer),
        asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectMethod(
        material_class_name,
        "void SetUseObjectUniformBuffer(bool)",
        asMETHOD(material, setUseObjectUniformBuffer),
        asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectMethod(
        material_class_name,
        "void SetUseVertexIndexBuffers(bool)",
        asMETHOD(material, setUseVertexIndexBuffers),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_texture = std::string("void SetTexture(") + texture_class_name + "@, int)";
    r = engine->RegisterObjectMethod(
        material_class_name,
        method_set_texture.c_str(),
        asMETHOD(material, addTexture),
        asCALL_THISCALL
    ); assert(r >= 0);
}

void lucus::bind_render_object_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(render_object_class_name, 0, asOBJ_REF); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        render_object_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(render_object, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        render_object_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(render_object, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_mesh = std::string("void SetMesh(") + mesh_class_name + "@)";
    r = engine->RegisterObjectMethod(
        render_object_class_name,
        method_set_mesh.c_str(),
        asMETHOD(render_object, setMesh),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_material = std::string("void SetMaterial(") + material_class_name + "@)";
    r = engine->RegisterObjectMethod(
        render_object_class_name,
        method_set_material.c_str(),
        asMETHOD(render_object, setMaterial),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_position = std::string("void SetPosition(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        render_object_class_name,
        method_set_position.c_str(),
        asMETHOD(render_object, setPosition),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_rotation_euler = std::string("void SetRotationEuler(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        render_object_class_name,
        method_set_rotation_euler.c_str(),
        asMETHOD(render_object, setRotationEuler),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_scale = std::string("void SetScale(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        render_object_class_name,
        method_set_scale.c_str(),
        asMETHOD(render_object, setScale),
        asCALL_THISCALL
    ); assert(r >= 0);
}

void lucus::bind_camera_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(camera_class_name, 0, asOBJ_REF); assert(r >= 0);

    std::string method_create_factory = std::string(camera_class_name) + "@ f()";
    r = engine->RegisterObjectBehaviour(
        camera_class_name,
        asBEHAVE_FACTORY,
        method_create_factory.c_str(),
        asFUNCTION(camera::create_factory),
        asCALL_CDECL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        camera_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(camera, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        camera_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(camera, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_position = std::string("void SetPosition(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        camera_class_name,
        method_set_position.c_str(),
        asMETHOD(camera, setPosition),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_rotation_euler = std::string("void SetRotationEuler(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        camera_class_name,
        method_set_rotation_euler.c_str(),
        asMETHOD(camera, setRotationEuler),
        asCALL_THISCALL
    ); assert(r >= 0);
}

void lucus::bind_scene_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(scene_class_name, 0, asOBJ_REF); assert(r >= 0);

    std::string method_create_factory = std::string(scene_class_name) + "@ f()";
    r = engine->RegisterObjectBehaviour(
        scene_class_name,
        asBEHAVE_FACTORY,
        method_create_factory.c_str(),
        asFUNCTION(scene::create_factory),
        asCALL_CDECL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        scene_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(scene, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        scene_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(scene, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_new_object = std::string(render_object_class_name) + "@ NewObject()";
    r = engine->RegisterObjectMethod(
        scene_class_name,
        method_new_object.c_str(),
        asMETHOD(scene, newObject),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_camera = std::string("void SetCamera(") + camera_class_name + "@)";
    r = engine->RegisterObjectMethod(
        scene_class_name,
        method_set_camera.c_str(),
        asMETHOD(scene, setCamera),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_dir_light = std::string("void SetDirectionalLight(") + dir_light_class_name + "@)";
    r = engine->RegisterObjectMethod(
        scene_class_name,
        method_set_dir_light.c_str(),
        asMETHOD(scene, setDirectionalLight),
        asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->SetDefaultNamespace(scene_class_name); assert(r >= 0);
    std::string load_scene = std::string(scene_class_name) + std::string("@ load(const string &in)");
    r = engine->RegisterGlobalFunction(
        load_scene.c_str(),
        asFUNCTION(load_scene_gltf),
        asCALL_CDECL
    ); assert(r >= 0);
    std::string load_scene_custom = std::string(scene_class_name) + std::string("@ load_with_material(const string &in, " + std::string(material_class_name) + "@)");
    r = engine->RegisterGlobalFunction(
        load_scene_custom.c_str(),
        asFUNCTION(load_scene_with_material_gltf),
        asCALL_CDECL
    ); assert(r >= 0);
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

void lucus::bind_texture_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(texture_class_name, 0, asOBJ_REF); assert(r >= 0);

    // std::string method_factory = std::string(texture_class_name) + "@ f()";
    // r = engine->RegisterObjectBehaviour(
    //     texture_class_name,
    //     asBEHAVE_FACTORY,
    //     method_factory.c_str(),
    //     asFUNCTION(texture::create_factory),
    //     asCALL_CDECL
    // ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        texture_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(mesh, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        texture_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(mesh, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->SetDefaultNamespace(texture_class_name); assert(r >= 0);
    std::string load_texture = std::string(texture_class_name) + std::string("@ load(const string &in)");
    r = engine->RegisterGlobalFunction(
        load_texture.c_str(),
        asFUNCTION(create_texture_factory),
        asCALL_CDECL
    ); assert(r >= 0);
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

void lucus::bind_texture_manager_class_and_object()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(texture_manager_class_name, 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    // std::string method_set_current_scene = std::string("void SetCurrentScene(") + scene_class_name + "@)";
    // r = engine->RegisterObjectMethod(
    //     renderer_class_name,
    //     method_set_current_scene.c_str(),
    //     asMETHOD(renderer, setCurrentScene),
    //     asCALL_THISCALL
    // ); assert(r >= 0);

    std::string property = std::string(texture_manager_class_name) + " " + texture_manager_global_name;
    r = engine->RegisterGlobalProperty(property.c_str(), &texture_manager::instance()); assert(r >= 0);
}

void lucus::bind_dir_light_class()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(dir_light_class_name, 0, asOBJ_REF); assert(r >= 0);

    std::string method_create_factory = std::string(dir_light_class_name) + "@ f()";
    r = engine->RegisterObjectBehaviour(
        dir_light_class_name,
        asBEHAVE_FACTORY,
        method_create_factory.c_str(),
        asFUNCTION(directional_light::create_factory),
        asCALL_CDECL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        dir_light_class_name, asBEHAVE_ADDREF, "void f()",
        asMETHOD(directional_light, addRef), asCALL_THISCALL
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        dir_light_class_name, asBEHAVE_RELEASE, "void f()",
        asMETHOD(directional_light, releaseRef), asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_position = std::string("void SetPosition(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        dir_light_class_name,
        method_set_position.c_str(),
        asMETHOD(directional_light, setPosition),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_rotation_euler = std::string("void SetRotationEuler(const ") + binds::vec3_class_name + " &in)";
    r = engine->RegisterObjectMethod(
        dir_light_class_name,
        method_set_rotation_euler.c_str(),
        asMETHOD(directional_light, setRotationEuler),
        asCALL_THISCALL
    ); assert(r >= 0);
}