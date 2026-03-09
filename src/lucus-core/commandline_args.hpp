#pragma once

#include "pch.hpp"

#include "singleton.hpp"

namespace lucus
{
    class commandline_args : public singleton<commandline_args>
    {
    public:
        void init(int argc, char** argv);
        const std::vector<std::string>& args() const { return _args; }

    private:
        std::vector<std::string> _args;
    };
}