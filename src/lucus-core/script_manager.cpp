#include "script_manager.hpp"

#include "filesystem.hpp"

#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>

using namespace lucus;

void script_manager::init()
{
    _engine.reset(asCreateScriptEngine());
    if (!_engine) {
        throw std::runtime_error("Failed to create AngelScript engine");
    }

	// Set the message callback to print the human readable messages that the engine gives in case of errors
	int r = _engine->SetMessageCallback(asMETHOD(script_manager, message_callback), this, asCALL_THISCALL); assert( r >= 0 );

    // Register the string type
	RegisterStdString(_engine.get());
}

void script_manager::message_callback(const asSMessageInfo &msg)
{
	const char* type = "ERR";
    if (msg.type == asMSGTYPE_WARNING) type = "WARN";
    else if (msg.type == asMSGTYPE_INFORMATION) type = "INFO";

    std::printf("%s (%d, %d): %s: %s\n",
        msg.section,
        msg.row,
        msg.col,
        type,
        msg.message);
}

bool script_manager::build_module(const std::string& filename, const std::string& moduleName)
{
    CScriptBuilder builder;

    int r = builder.StartNewModule(_engine.get(), moduleName.c_str());
    if (r < 0)
        return false;

    std::string path = filesystem::instance().get_script(filename);
    r = builder.AddSectionFromFile(path.c_str());
    if (r < 0)
        return false;

    r = builder.BuildModule();
    if (r < 0)
        return false;

    return true;
}

bool script_manager::run_func(const std::string& moduleName, const std::string& functionName)
{
    asIScriptModule* mod = _engine->GetModule(moduleName.c_str(), asGM_ONLY_IF_EXISTS);
    if (!mod)
        return false;

    // TODO: many arguments?
    asIScriptFunction* func = mod->GetFunctionByDecl(functionName.c_str());
    if (!func)
        return false;

    asIScriptContext* ctx = _engine->CreateContext();
    if (!ctx)
        return false;

    ctx->Prepare(func);
    int r = ctx->Execute();

    if (r != asEXECUTION_FINISHED)
    {
        if (r == asEXECUTION_EXCEPTION)
        {
            std::printf("Run script exception: %s\n", ctx->GetExceptionString());
        }

        ctx->Release();
        return false;
    }

    ctx->Release();
    return true;
}