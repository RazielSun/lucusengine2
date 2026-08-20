#include "dynamic_rhi.hpp"

#include "validation_sink.hpp"

using namespace lucus;

void dynamic_rhi::beginValidationSetup(const rhi_init_desc& desc)
{
    _initDesc = desc;
    _validationStatus = validation_status{ desc.validation, rhi_validation_level::OFF, {}, true };

    validation_sink::instance().configure(desc);
}

void dynamic_rhi::finishValidationSetup()
{
    const validation_status& status = _validationStatus;
    const bool downgraded = static_cast<u8>(status.effective) < static_cast<u8>(status.requested);

    std::printf("[validation] requested=%s effective=%s%s%s\n",
                to_string(status.requested),
                to_string(status.effective),
                status.detail.empty() ? "" : " - ",
                status.detail.c_str());

    if (downgraded && _initDesc.validation_required)
    {
        _validationStatus.satisfied = false;
        std::fprintf(stderr,
                     "[validation] '%s' was requested with --validation-required, but only '%s' is available: %s\n",
                     to_string(status.requested),
                     to_string(status.effective),
                     status.detail.empty() ? "no detail" : status.detail.c_str());
    }

    validation_sink::instance().setStatus(_validationStatus);
}
