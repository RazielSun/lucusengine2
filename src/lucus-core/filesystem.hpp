#pragma once

#include "pch.hpp"

#include "singleton.hpp"

namespace lucus
{
    class filesystem : public singleton<filesystem>
    {
    public:
        void set_working_directory(const std::string& path);
        std::vector<char> read_file(const std::string& filename) const;
        std::vector<char> read_shader(const std::string& filename) const;

        std::string get_path(const std::string& filename) const;
        std::string get_script(const std::string& filename) const;
        std::string get_shader(const std::string& filename) const;

    private:
        std::string _workingDirectory;
    };
}