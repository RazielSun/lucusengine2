#pragma once

#include "pch.hpp"

#include "singleton.hpp"

namespace lucus
{
    std::string get_argument(const std::vector<std::string>& args, const std::string& name);
    bool has_argument(const std::vector<std::string>& args, const std::string& name);

    class commandline_args : public singleton<commandline_args>
    {
    public:
        void init(int argc, char** argv);
        const std::vector<std::string>& args() const { return _args; }

        std::string getArgument(const std::string& name) const { return get_argument(_args, name); }
        bool hasArgument(const std::string& name) const { return has_argument(_args, name); }

    private:
        std::vector<std::string> _args;
    };
}
