#include "engine.hpp"

#include "bind_core.hpp"
#include "bind_math.hpp"
#include "bind_graphics.hpp"

#include "commandline_args.hpp"
#include "filesystem.hpp"
#include "script_manager.hpp"
#include "window_manager.hpp"

#include "renderer.hpp"

using namespace lucus;

void engine::run(int argc, char** argv)
{
    commandline_args::instance().init(argc, argv);

    script_manager::instance().init();

    bind_core_module();
    bind_math_module();
    bind_graphics_module();

    if (!script_manager::instance().build_module("main.as", "Entry")) {
        throw std::runtime_error("Failed to build main.as script");
    }

    if (!script_manager::instance().run_func("Entry", "void main()")) {
        throw std::runtime_error("Failed to run main() from Entry module");
    }

    auto handle = window_manager::instance().getMainWindow();
    if (!handle.is_valid()) {
        throw std::runtime_error("Failed to get main window handle");
    }

    auto mainWindow = window_manager::instance().getWindow(handle);
    using clock = std::chrono::steady_clock;
    auto prevFrameTime = clock::now();

    while (mainWindow && !mainWindow->shouldClose())
    {
        const auto now = clock::now();
        float dt = std::chrono::duration<float>(now - prevFrameTime).count();
        prevFrameTime = now;

        // Avoid very large frame steps after pauses (e.g. debugger breakpoints).
        constexpr float max_frame_dt = 0.1f; // 100 ms
        if (dt > max_frame_dt) {
            dt = max_frame_dt;
        }

        mainWindow->tick();

        renderer::instance().tick(dt);
    }
    
    renderer::instance().cleanup();
}
