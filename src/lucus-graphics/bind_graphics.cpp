#include "bind_graphics.hpp"

#include "script_manager.hpp"

#include "window_manager.hpp"
#include "renderer.hpp"
#include "render_object.hpp"
#include "camera.hpp"

#include "bind_math.hpp"

namespace utils
{
    constexpr const char* window_manager_class_name = "WindowManagerClass";
    constexpr const char* window_manager_global_name = "g_window_manager";

    constexpr const char* renderer_class_name = "RendererClass";
    constexpr const char* renderer_global_name = "g_renderer";

    constexpr const char* mesh_class_name = "Mesh";
    constexpr const char* material_class_name = "Material";
    constexpr const char* render_object_class_name = "RenderObject";
    constexpr const char* camera_class_name = "Camera";
}

namespace lucus
{
    void bind_mesh_class();
    void bind_material_class();
    void bind_render_object_class();
    void bind_camera_class();

    void bind_window_manager_class_and_object();
    void bind_renderer_class_and_object();
}

void lucus::bind_graphics_module()
{
    bind_window_manager_class_and_object();

    bind_mesh_class();
    bind_material_class();
    bind_render_object_class();
    bind_camera_class();

    bind_renderer_class_and_object();
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

    std::string method_emplace = std::string(render_object_class_name) + "@ EmplaceRenderObject()";
    r = engine->RegisterObjectMethod(
        renderer_class_name,
        method_emplace.c_str(),
        asMETHOD(renderer, emplaceRenderObject),
        asCALL_THISCALL
    ); assert(r >= 0);

    std::string method_set_camera = std::string("void SetCamera(") + camera_class_name + "@)";
    r = engine->RegisterObjectMethod(
        renderer_class_name,
        method_set_camera.c_str(),
        asMETHOD(renderer, setCamera),
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

    std::string method_create_factory = std::string(mesh_class_name) + "@ f()";
    r = engine->RegisterObjectBehaviour(
        mesh_class_name,
        asBEHAVE_FACTORY,
        method_create_factory.c_str(),
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

    r = engine->RegisterObjectMethod(
        mesh_class_name,
        "void SetDrawCount(int)",
        asMETHOD(mesh, setDrawCount),
        asCALL_THISCALL
    ); assert(r >= 0);
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
        "void SetUseUniformBuffers(bool)",
        asMETHOD(material, setUseUniformBuffers),
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