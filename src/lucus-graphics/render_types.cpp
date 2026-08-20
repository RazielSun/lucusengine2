#include "render_types.hpp"

#include "commandline_args.hpp"

using namespace lucus;

namespace
{
    const char* kValidationValues = "off, on, gpu (or 0, 1, 2)";

    rhi_validation_level parse_validation_level(const std::string& value)
    {
        if (value == "off" || value == "0")
            return rhi_validation_level::OFF;
        if (value == "on" || value == "1")
            return rhi_validation_level::ON;
        if (value == "gpu" || value == "2")
            return rhi_validation_level::GPU;

        throw std::runtime_error("Invalid value for --validation: '" + value +
                                 "'. Accepted values: " + kValidationValues);
    }
}

const char* lucus::to_string(rhi_validation_level level)
{
    switch (level)
    {
        case rhi_validation_level::OFF: return "off";
        case rhi_validation_level::ON:  return "on";
        case rhi_validation_level::GPU: return "gpu";
    }
    return "unknown";
}

rhi_init_desc lucus::rhi_init_desc_from_commandline()
{
    const commandline_args& args = commandline_args::instance();

    rhi_init_desc desc{};

    const bool has_level = args.hasArgument("validation");
    const bool has_disable = args.hasArgument("no-validation");

    if (has_level && has_disable)
        throw std::runtime_error("--validation and --no-validation cannot be used together");

    if (has_level)
    {
        const std::vector<std::string>& raw = args.args();
        const bool bare_flag = std::find(raw.begin(), raw.end(), "--validation") != raw.end();

        const std::string value = bare_flag ? std::string{} : args.getArgument("validation");
        if (value.empty())
            throw std::runtime_error("--validation requires a value: --validation=<" +
                                     std::string(kValidationValues) + ">");

        desc.validation = parse_validation_level(value);
    }
    else if (has_disable)
    {
        desc.validation = rhi_validation_level::OFF;
    }

    desc.validation_required = args.hasArgument("validation-required");
    desc.validation_fatal = args.hasArgument("validation-fatal");

    if (desc.validation == rhi_validation_level::OFF)
    {
        if (desc.validation_required)
            throw std::runtime_error("--validation-required cannot be used with validation turned off");
        if (desc.validation_fatal)
            throw std::runtime_error("--validation-fatal cannot be used with validation turned off");
    }

    return desc;
}
