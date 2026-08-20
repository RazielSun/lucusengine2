#include "validation_selftest.hpp"

#include <thread>

#include "render_types.hpp"
#include "validation_sink.hpp"

using namespace lucus;

namespace
{
    u32 g_checks = 0;
    u32 g_failures = 0;

    void check(bool condition, const std::string& what)
    {
        g_checks++;
        if (!condition)
        {
            g_failures++;
            std::fprintf(stderr, "[selftest] FAIL %s\n", what.c_str());
        }
    }

    void check_parse(const std::vector<std::string>& args,
                     rhi_validation_level level,
                     bool required,
                     bool fatal)
    {
        const std::string what = "parse [" + [&args] {
            std::string joined;
            for (const std::string& arg : args)
                joined += (joined.empty() ? "" : " ") + arg;
            return joined;
        }() + "]";

        try
        {
            const rhi_init_desc desc = parse_rhi_init_desc(args);
            check(desc.validation == level, what + " level=" + to_string(desc.validation) +
                                            " expected=" + to_string(level));
            check(desc.validation_required == required, what + " required");
            check(desc.validation_fatal == fatal, what + " fatal");
        }
        catch (const std::exception& err)
        {
            check(false, what + " threw unexpectedly: " + err.what());
        }
    }

    void check_parse_rejects(const std::vector<std::string>& args)
    {
        std::string joined;
        for (const std::string& arg : args)
            joined += (joined.empty() ? "" : " ") + arg;

        try
        {
            parse_rhi_init_desc(args);
            check(false, "parse [" + joined + "] was accepted, expected a rejection");
        }
        catch (const std::exception&)
        {
            check(true, "parse [" + joined + "] rejected");
        }
    }

    void test_parser()
    {
        const std::string app = "app";

        check_parse({ app }, rhi_validation_level::ON, false, false);
        check_parse({ app, "--validation=off" }, rhi_validation_level::OFF, false, false);
        check_parse({ app, "--validation=0" }, rhi_validation_level::OFF, false, false);
        check_parse({ app, "--no-validation" }, rhi_validation_level::OFF, false, false);
        check_parse({ app, "--validation=on" }, rhi_validation_level::ON, false, false);
        check_parse({ app, "--validation=1" }, rhi_validation_level::ON, false, false);
        check_parse({ app, "--validation=gpu" }, rhi_validation_level::GPU, false, false);
        check_parse({ app, "--validation=2" }, rhi_validation_level::GPU, false, false);
        check_parse({ app, "--validation-required" }, rhi_validation_level::ON, true, false);
        check_parse({ app, "--validation-fatal" }, rhi_validation_level::ON, false, true);
        check_parse({ app, "--validation=gpu", "--validation-required", "--validation-fatal" },
                    rhi_validation_level::GPU, true, true);
        check_parse({ app, "--script", "tests/cube_test.as", "--validation=gpu", "--ticks", "5" },
                    rhi_validation_level::GPU, false, false);
        check_parse({ app, "--validation=off", "--validation=gpu" }, rhi_validation_level::OFF, false, false);

        check_parse_rejects({ app, "--validation" });
        check_parse_rejects({ app, "--validation=" });
        check_parse_rejects({ app, "--validation=bogus" });
        check_parse_rejects({ app, "--validation", "on" });
        check_parse_rejects({ app, "--validation", "--script", "tests/cube_test.as" });
        check_parse_rejects({ app, "--validation=on", "--no-validation" });
        check_parse_rejects({ app, "--no-validation", "--validation-required" });
        check_parse_rejects({ app, "--no-validation", "--validation-fatal" });
    }

    rhi_init_desc desc_with(rhi_validation_level level)
    {
        rhi_init_desc desc{};
        desc.validation = level;
        return desc;
    }

    void test_counting()
    {
        validation_sink& sink = validation_sink::instance();
        sink.configure(desc_with(rhi_validation_level::ON));

        check(sink.counts().errors == 0, "counters start at zero");

        sink.report(validation_backend::VULKAN, validation_severity::ERROR, "VUID-A", "first");
        sink.report(validation_backend::VULKAN, validation_severity::WARNING, "VUID-B", "second");
        sink.report(validation_backend::VULKAN, validation_severity::INFO, "VUID-C", "third");

        const validation_counts counts = sink.counts();
        check(counts.errors == 1, "one error counted");
        check(counts.warnings == 1, "one warning counted");
        check(counts.infos == 1, "one info counted");
        check(sink.errorCount() == 1, "errorCount matches");
    }

    void test_duplicates()
    {
        validation_sink& sink = validation_sink::instance();
        sink.configure(desc_with(rhi_validation_level::ON));

        for (u32 i = 0; i < 10; ++i)
            sink.report(validation_backend::VULKAN, validation_severity::ERROR, "VUID-DUP", "repeated");

        check(sink.counts().errors == 10, "every occurrence is counted, printing cap does not drop any");
    }

    void test_suppression()
    {
        validation_sink& sink = validation_sink::instance();
        sink.configure(desc_with(rhi_validation_level::ON));
        sink.suppress({ validation_backend::DX12, "1234", "known and accepted for the selftest" });

        sink.report(validation_backend::DX12, validation_severity::ERROR, "1234", "suppressed one");
        sink.report(validation_backend::VULKAN, validation_severity::ERROR, "1234", "same id, other backend");

        const validation_counts counts = sink.counts();
        check(counts.suppressed_errors == 1, "suppressed error is counted separately");
        check(counts.errors == 1, "suppression is qualified by backend");
    }

    void test_reset()
    {
        validation_sink& sink = validation_sink::instance();
        sink.configure(desc_with(rhi_validation_level::ON));
        sink.suppress({ validation_backend::VULKAN, "VUID-R", "cleared by the next configure" });
        sink.report(validation_backend::VULKAN, validation_severity::ERROR, "VUID-X", "before reset");

        sink.configure(desc_with(rhi_validation_level::ON));

        const validation_counts counts = sink.counts();
        check(counts.errors == 0 && counts.suppressed_errors == 0, "configure resets the counters");

        sink.report(validation_backend::VULKAN, validation_severity::ERROR, "VUID-R", "after reset");
        check(sink.counts().errors == 1, "configure clears the suppression list");
    }

    void test_off_emits_nothing()
    {
        validation_sink& sink = validation_sink::instance();
        sink.configure(desc_with(rhi_validation_level::OFF));

        sink.report(validation_backend::VULKAN, validation_severity::ERROR, "VUID-OFF", "must be dropped");

        check(sink.counts().errors == 0, "validation off drops reports");
    }

    void test_concurrent_reports()
    {
        validation_sink& sink = validation_sink::instance();
        sink.configure(desc_with(rhi_validation_level::ON));
        sink.suppress({ validation_backend::VULKAN, "VUID-CONCURRENT", "keeps the selftest output readable" });

        constexpr u32 thread_count = 4;
        constexpr u32 per_thread = 250;

        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (u32 t = 0; t < thread_count; ++t)
        {
            threads.emplace_back([&sink] {
                for (u32 i = 0; i < per_thread; ++i)
                    sink.report(validation_backend::VULKAN, validation_severity::ERROR, "VUID-CONCURRENT", "from a thread");
            });
        }

        for (std::thread& thread : threads)
            thread.join();

        check(sink.counts().suppressed_errors == thread_count * per_thread, "no report is lost across threads");
    }
}

bool lucus::run_validation_selftest()
{
    g_checks = 0;
    g_failures = 0;

    test_parser();
    test_counting();
    test_duplicates();
    test_suppression();
    test_reset();
    test_off_emits_nothing();
    test_concurrent_reports();

    validation_sink::instance().configure(desc_with(rhi_validation_level::OFF));

    std::printf("[selftest] %u checks, %u failures\n", g_checks, g_failures);
    return g_failures == 0;
}
