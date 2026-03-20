#pragma once

namespace lucus
{
    constexpr uint32_t g_framesInFlight{ 2 };
    constexpr uint32_t g_swapchainImageCount{ 3 };

    struct resource_handle
    {
        resource_handle() : index(-1) {}
        explicit resource_handle(int idx) : index(idx) {}
        
        int get() const { return index; }
        bool is_valid() const { return index >= 0; }

        void invalidate() { index = -1; }

    protected:
        int index;
    };

    struct window_handle : public resource_handle
    {
        window_handle() : resource_handle() {}
        explicit window_handle(int idx) : resource_handle(idx) {}
    };

    struct viewport_handle : public resource_handle
    {
        viewport_handle() : resource_handle() {}
        explicit viewport_handle(int idx) : resource_handle(idx) {}
    };

    struct material_handle
    {
        material_handle() : index(0) {}
        explicit material_handle(uint32_t idx) : index(idx) {}
        
        uint32_t get() const { return index; }
        bool is_valid() const { return index > 0; }

        void invalidate() { index = 0; }

    protected:
        uint32_t index;
    };

    struct mesh_handle : public resource_handle
    {
        mesh_handle() : resource_handle() {}
        explicit mesh_handle(int idx) : resource_handle(idx) {}
    };

    struct render_instance
    {
        mesh_handle mesh;
        material_handle material;
    };

    struct command_buffer
    {
        std::vector<render_instance> render_list;
    };

    
}