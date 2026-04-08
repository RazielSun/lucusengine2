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

    std::string dir = getArgument("directory");

    const bool path_specified = !dir.empty();

    namespace fs = std::filesystem;
    fs::path p = fs::absolute(path_specified ? dir : argv[0]);
    if (!path_specified)
    {
        p = p.parent_path();
    }

    if (!fs::exists(p))
        throw std::runtime_error("Provided path does not exist");

    std::string root_path = fs::weakly_canonical(p).string();
    filesystem::instance().set_working_directory(root_path);
}

std::string commandline_args::getArgument(const std::string& name) const
{
    if (name.empty())
        return "";

    const std::string prefixed_name = "--" + name;
    const std::string equals_prefix = prefixed_name + '=';

    for (std::size_t i = 0; i < _args.size(); ++i)
    {
        const std::string& arg = _args[i];
        if (arg == prefixed_name)
        {
            if ((i + 1) < _args.size())
            {
                const std::string& next_arg = _args[i + 1];
                if (!next_arg.empty() && next_arg[0] == '-')
                    throw std::runtime_error("Missing value for argument: " + prefixed_name);

                return next_arg;
            }

            return "";
        }

        if (arg.size() > equals_prefix.size() &&
            arg.compare(0, equals_prefix.size(), equals_prefix) == 0)
        {
            return arg.substr(equals_prefix.size());
        }
    }

    return "";
}
