#include "bind_core.hpp"

#include "script_manager.hpp"

#include "engine_info.hpp"

namespace utils
{
    constexpr const char* engine_info_class_name = "EngineInfo";
    constexpr const char* engine_info_global_name = "g_engine_info";
}

namespace lucus
{
    void bind_engine_info_class_and_object();
}

void lucus::bind_core_module()
{
    bind_engine_info_class_and_object();
}

void lucus::bind_engine_info_class_and_object()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace lucus;
    using namespace utils;

    r = engine->RegisterObjectType(engine_info_class_name, 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    // r = engine->RegisterObjectMethod(
    //     engine_info_name,
    //     "void SetAppName(const string &in)",
    //     asMETHOD(application_info, set_app_name),
    //     asCALL_THISCALL
    // ); assert(r >= 0);

    // r = engine->RegisterObjectMethod(
    //     application_info_name,
    //     "void SetWindowSize(int, int)",
    //     asMETHOD(application_info, set_window_size),
    //     asCALL_THISCALL
    // ); assert(r >= 0);

    // r = engine->RegisterObjectMethod(
    //     application_info_name,
    //     "void SetResizable(bool)",
    //     asMETHOD(application_info, set_resizable),
    //     asCALL_THISCALL
    // ); assert(r >= 0);

    // r = engine->RegisterObjectMethod(
    //     application_info_name,
    //     "void SetFullscreen(bool)",
    //     asMETHOD(application_info, set_fullscreen),
    //     asCALL_THISCALL
    // ); assert(r >= 0);

    engine_info& engine_info = engine_info::instance();
    std::string property = std::string(engine_info_class_name) + " " + engine_info_global_name;
    r = engine->RegisterGlobalProperty(property.c_str(), &engine_info); assert(r >= 0);
}