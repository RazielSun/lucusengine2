#include "commandline_args.hpp"

#include "filesystem.hpp"

#include <filesystem>

using namespace lucus;

void commandline_args::init(int argc, char** argv)
{
    _args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        _args.emplace_back(argv[i]);
    }

    const bool path_specified = (argc > 1);

    namespace fs = std::filesystem;
    fs::path p = fs::absolute(path_specified ? argv[1] : argv[0]);
    if (!path_specified)
    {
        p = p.parent_path();
    }

    if (!fs::exists(p))
        throw std::runtime_error("Provided path does not exist");

    std::string root_path = fs::weakly_canonical(p).string();
    filesystem::instance().set_working_directory(root_path);
}