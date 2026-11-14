#include "engine.hpp"

#include "bind_core.hpp"
#include "bind_math.hpp"
#include "bind_graphics.hpp"

#include "commandline_args.hpp"
#include "filesystem.hpp"
#include "script_manager.hpp"
#include "window_manager.hpp"

#include "renderer.hpp"
#include "image_write.hpp"
#include "image_capture_selftest.hpp"
#include "validation_selftest.hpp"
#include "validation_sink.hpp"

using namespace lucus;

namespace
{
    struct run_finalizer
    {
        ~run_finalizer()
        {
            try
            {
                renderer::instance().cleanup();
            }
            catch (const std::exception& err)
            {
                std::fprintf(stderr, "Renderer cleanup failed: %s\n", err.what());
            }

            validation_sink::instance().printSummary();
        }
    };
}

void engine::run(int argc, char** argv)
{
    commandline_args::instance().init(argc, argv);

    if (commandline_args::instance().hasArgument("validation-selftest"))
    {
        if (!run_validation_selftest()) {
            throw std::runtime_error("Validation selftest failed");
        }
        return;
    }

    if (commandline_args::instance().hasArgument("capture-selftest"))
    {
        if (!run_image_capture_selftest())
        {
            throw std::runtime_error("Screenshot selftest failed");
        }
        return;
    }

    const auto& args = commandline_args::instance();
    const std::string screenshot_path = args.getArgument("screenshot");
    const std::string ticks_arg = args.getArgument("ticks");
    size_t parsed_ticks = 0;
    const int max_ticks = ticks_arg.empty() ? 0 : std::stoi(ticks_arg, &parsed_ticks);
    if (!ticks_arg.empty() && (parsed_ticks != ticks_arg.size() || max_ticks < 0))
    {
        throw std::runtime_error("--ticks requires a non-negative integer");
    }
    if (args.hasArgument("screenshot") && screenshot_path.empty())
    {
        throw std::runtime_error("--screenshot requires a PNG file path");
    }
    if (!screenshot_path.empty() && max_ticks <= 0)
    {
        throw std::runtime_error("--screenshot requires --ticks with a positive frame count");
    }
    bool screenshot_written = screenshot_path.empty();

    renderer::instance().setRHIInitDesc(rhi_init_desc_from_commandline());

    run_finalizer finalizer;

    script_manager::instance().init();

    bind_core_module();
    bind_math_module();
    bind_graphics_module();

    std::string run_script = commandline_args::instance().getArgument("script");
    if (run_script.empty())
    {
        run_script = "main.as";
    }

    if (!script_manager::instance().build_module(run_script, "Entry")) {
        throw std::runtime_error("Failed to build " + run_script + " script");
    }

    if (!script_manager::instance().run_func("Entry", "void main()")) {
        throw std::runtime_error("Failed to run main() from Entry module");
    }
    
    auto handle = window_manager::instance().getMainWindow();
    if (!handle.is_valid()) {
        throw std::runtime_error("Failed to get main window handle");
    }

    const validation_status* validation = renderer::instance().getValidationStatus();
    if (validation && !validation->satisfied) {
        throw std::runtime_error("Requested validation level is not available");
    }

    auto mainWindow = window_manager::instance().getWindow(handle);
    using clock = std::chrono::steady_clock;
    auto prevFrameTime = clock::now();
    int tick_count = 0;

    while (mainWindow && !mainWindow->shouldClose())
    {
        const auto now = clock::now();
        float dt = std::chrono::duration<float>(now - prevFrameTime).count();
        prevFrameTime = now;

        // Avoid very large frame steps after pauses (e.g. debugger breakpoints).
        constexpr float max_frame_dt = 0.1f;
        if (dt > max_frame_dt) {
            dt = max_frame_dt;
        }

        mainWindow->tick();
        if (mainWindow->shouldClose())
        {
            break;
        }

        const bool capture_tick = !screenshot_path.empty() && tick_count + 1 == max_ticks;
        image_capture image;
        renderer::instance().tick(dt, capture_tick ? &image : nullptr);
        if (capture_tick)
        {
            write_png(screenshot_path, image);
            screenshot_written = true;
            std::printf("Screenshot saved: %s (%ux%u, frame %d)\n",
                screenshot_path.c_str(), image.width, image.height, tick_count + 1);
        }

        if (max_ticks > 0)
        {
            tick_count++;
            if (tick_count >= max_ticks) {
                mainWindow->close();
            }
        }
    }

    if (!screenshot_written)
    {
        throw std::runtime_error("Window closed before the requested screenshot frame");
    }
}
