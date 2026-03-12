#include "filesystem.hpp"

#include <fstream>

using namespace lucus;

void filesystem::set_working_directory(const std::string& path)
{
    _workingDirectory = path;
    if (_workingDirectory.back() != '/') {
        _workingDirectory.push_back('/');
    }
    std::printf("set_working_directory: %s\n", _workingDirectory.c_str());
}

std::vector<char> filesystem::read_file(const std::string& filename)
{
    std::string filepath = _workingDirectory + filename;

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::string filesystem::get_path(const std::string& filename)
{
    return _workingDirectory + filename;
}

std::string filesystem::get_script(const std::string& filename)
{
    return _workingDirectory + "content/scripts/" + filename;
}
