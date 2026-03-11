#include "engine_info.hpp"

#include "script_manager.hpp"

namespace utils
{
    constexpr const char* engine_info_name = "EngineInfo";
}

using namespace lucus;

void lucus::bind_engine_info()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    r = engine->RegisterObjectType(engine_info_name, 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

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
}

void lucus::bind_engine_info_object(engine_info& info, const std::string& name)
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace utils;

    std::string property = std::string(engine_info_name) + " " + name;
    r = engine->RegisterGlobalProperty(property.c_str(), &info); assert(r >= 0);
}