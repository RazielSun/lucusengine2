#pragma once

#include "pch.hpp"

#include "singleton.hpp"
#include "angelscript_utils.hpp"

#include "angelscript.h"

namespace lucus
{
    class script_manager : public singleton<script_manager>
    {
    public:
        void init();

        asIScriptEngine* get_engine() const { return _engine.get(); }

        bool run_script(const std::string& filename);

    protected:
        void message_callback(const asSMessageInfo &msg);

    private:
        angelscript_ptr<asIScriptEngine> _engine;
    };
}