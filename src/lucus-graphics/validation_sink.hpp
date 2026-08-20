#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "render_types.hpp"

namespace lucus
{
    enum class validation_backend : u8
    {
        VULKAN = 0,
        DX12   = 1,
        METAL  = 2,
    };

    enum class validation_severity : u8
    {
        INFO    = 0,
        WARNING = 1,
        ERROR   = 2,
    };

    const char* to_string(validation_backend backend);
    const char* to_string(validation_severity severity);

    struct validation_suppression
    {
        validation_backend backend;
        std::string id;
        std::string reason;
    };

    struct validation_counts
    {
        u32 errors { 0 };
        u32 warnings { 0 };
        u32 infos { 0 };
        u32 suppressed_errors { 0 };
        u32 suppressed_warnings { 0 };
        u32 suppressed_infos { 0 };
    };

    class validation_sink : public singleton<validation_sink>
    {
    public:
        static constexpr u32 g_maxPrintsPerMessage { 3 };

        void configure(const rhi_init_desc& desc);
        void setStatus(const validation_status& status);

        void suppress(const validation_suppression& suppression);

        void report(validation_backend backend,
                    validation_severity severity,
                    const std::string& id,
                    const std::string& message);

        validation_counts counts() const;
        u32 errorCount() const;

        void printSummary() const;

    private:
        struct message_record
        {
            validation_backend backend;
            validation_severity severity;
            std::string id;
            u32 occurrences { 0 };
            bool suppressed { false };
        };

        static std::string makeKey(validation_backend backend, const std::string& id);

        const validation_suppression* findSuppression(validation_backend backend, const std::string& id) const;

        mutable std::mutex _mutex;

        rhi_init_desc _desc {};
        validation_status _status {};
        bool _enabled { false };

        std::vector<validation_suppression> _suppressions;
        std::unordered_map<std::string, message_record> _records;
        validation_counts _counts {};
    };
}
