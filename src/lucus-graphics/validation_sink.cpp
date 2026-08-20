#include "validation_sink.hpp"

using namespace lucus;

const char* lucus::to_string(validation_backend backend)
{
    switch (backend)
    {
        case validation_backend::VULKAN: return "vulkan";
        case validation_backend::DX12:   return "dx12";
        case validation_backend::METAL:  return "metal";
    }
    return "unknown";
}

const char* lucus::to_string(validation_severity severity)
{
    switch (severity)
    {
        case validation_severity::INFO:    return "info";
        case validation_severity::WARNING: return "warning";
        case validation_severity::ERROR:   return "error";
    }
    return "unknown";
}

void validation_sink::configure(const rhi_init_desc& desc)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _desc = desc;
    _status = validation_status{ desc.validation, rhi_validation_level::OFF, {}, true };
    _enabled = desc.validation != rhi_validation_level::OFF;

    _records.clear();
    _counts = validation_counts{};
    _suppressions.clear();
}

void validation_sink::setStatus(const validation_status& status)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _status = status;
}

void validation_sink::suppress(const validation_suppression& suppression)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _suppressions.push_back(suppression);
}

std::string validation_sink::makeKey(validation_backend backend, const std::string& id)
{
    return std::string(to_string(backend)) + '/' + id;
}

const validation_suppression* validation_sink::findSuppression(validation_backend backend, const std::string& id) const
{
    for (const validation_suppression& suppression : _suppressions)
    {
        if (suppression.backend == backend && suppression.id == id)
            return &suppression;
    }
    return nullptr;
}

void validation_sink::report(validation_backend backend,
                             validation_severity severity,
                             const std::string& id,
                             const std::string& message)
{
    bool fatal = false;

    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_enabled)
            return;

        const std::string key = makeKey(backend, id);
        const bool suppressed = findSuppression(backend, id) != nullptr;

        message_record& record = _records[key];
        record.backend = backend;
        record.severity = severity;
        record.id = id;
        record.suppressed = suppressed;
        record.occurrences++;

        switch (severity)
        {
            case validation_severity::ERROR:
                (suppressed ? _counts.suppressed_errors : _counts.errors)++;
                break;
            case validation_severity::WARNING:
                (suppressed ? _counts.suppressed_warnings : _counts.warnings)++;
                break;
            case validation_severity::INFO:
                (suppressed ? _counts.suppressed_infos : _counts.infos)++;
                break;
        }

        if (!suppressed && record.occurrences <= g_maxPrintsPerMessage)
        {
            std::fprintf(stderr, "[validation][%s][%s] %s: %s\n",
                         to_string(backend), to_string(severity),
                         id.empty() ? "no-id" : id.c_str(),
                         message.c_str());

            if (record.occurrences == g_maxPrintsPerMessage)
            {
                std::fprintf(stderr, "[validation][%s][%s] %s: further occurrences are counted, not printed\n",
                             to_string(backend), to_string(severity),
                             id.empty() ? "no-id" : id.c_str());
            }
        }

        fatal = _desc.validation_fatal && !suppressed && severity == validation_severity::ERROR;
    }

    if (fatal)
    {
        printSummary();
        std::fflush(stderr);
        std::fflush(stdout);
        std::abort();
    }
}

validation_counts validation_sink::counts() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _counts;
}

u32 validation_sink::errorCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _counts.errors;
}

void validation_sink::printSummary() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::fprintf(stderr, "[validation] requested=%s effective=%s%s%s\n",
                 to_string(_status.requested),
                 to_string(_status.effective),
                 _status.detail.empty() ? "" : " - ",
                 _status.detail.c_str());

    if (!_enabled)
        return;

    const u32 suppressed_total = _counts.suppressed_errors + _counts.suppressed_warnings + _counts.suppressed_infos;

    std::fprintf(stderr, "[validation] %u errors, %u warnings (%u suppressed)\n",
                 _counts.errors, _counts.warnings, suppressed_total);

    for (const auto& [key, record] : _records)
    {
        if (record.severity == validation_severity::INFO)
            continue;

        std::fprintf(stderr, "[validation]   %s %s x%u%s\n",
                     to_string(record.severity),
                     key.c_str(),
                     record.occurrences,
                     record.suppressed ? " (suppressed)" : "");
    }
}
