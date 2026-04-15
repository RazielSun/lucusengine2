#pragma once

#include "pch.hpp"

#include "core_types.hpp"

namespace lucus
{
    static constexpr uint32_t COMMAND_FIXED_SIZE = 32;
    static constexpr uint32_t COMMAND_ALIGNMENT  = alignof(std::max_align_t);

    enum class gpu_command_type : u16
    {
        RENDER_PASS_BEGIN,
        RENDER_PASS_END,
        SET_VIEWPORT,
        SET_SCISSOR,
        BIND_PIPELINE,
        BIND_UNIFORM_BUFFER,
        BIND_TEXTURE,
        BIND_VERTEX_BUFFER,
        BIND_INDEX_BUFFER,
        DRAW_INDEXED,
        DRAW_VERTEX,
    };

    struct gpu_command_base
    {
        gpu_command_type type;

        explicit gpu_command_base(gpu_command_type in_type) : type(in_type) {}
    };

    struct gpu_command_buffer
    {
        std::vector<u8> data;
        u32 commandCount = 0;

        template<class T>
        T* addCommand()
        {
            static_assert(std::is_base_of_v<gpu_command_base, T>);
            static_assert(std::is_trivially_copyable_v<T>);
            static_assert(sizeof(T) <= COMMAND_FIXED_SIZE);
            static_assert(alignof(T) <= COMMAND_ALIGNMENT);

            const size_t oldSize = data.size();
            const size_t newSize = oldSize + COMMAND_FIXED_SIZE;
            data.resize(newSize);

            void* dst = data.data() + oldSize;

            // optionally zero the slot
            std::memset(dst, 0, COMMAND_FIXED_SIZE);

            ++commandCount;
            return reinterpret_cast<T*>(dst);
        }

        template<class T, class... Args>
        T& emplaceCommand(Args&&... args)
        {
            T* ptr = addCommand<T>();
            new (ptr) T(std::forward<Args>(args)...);
            return *ptr;
        }
    };

    // Commands

    struct gpu_render_pass_begin_command : public gpu_command_base
    {
        gpu_render_pass_begin_command(u16 _ox, u16 _oy, u16 _ex, u16 _ey)
        : gpu_command_base(gpu_command_type::RENDER_PASS_BEGIN)
        , offset_x(_ox)
        , offset_y(_oy)
        , extent_x(_ex)
        , extent_y(_ey)
        {}

        u16 offset_x;
        u16 offset_y;
        u16 extent_x;
        u16 extent_y;
    };

    struct gpu_render_pass_end_command : public gpu_command_base
    {
        gpu_render_pass_end_command()
        : gpu_command_base(gpu_command_type::RENDER_PASS_END)
        {}
    };

    struct gpu_set_viewport_command : public gpu_command_base
    {
        gpu_set_viewport_command(u16 _x, u16 _y, u16 _width, u16 _height, float _minDepth, float _maxDepth)
        : gpu_command_base(gpu_command_type::SET_VIEWPORT)
        , x(_x)
        , y(_y)
        , width(_width)
        , height(_height)
        , minDepth(_minDepth)
        , maxDepth(_maxDepth)
        {}

        u16 x;
        u16 y;
        u16 width;
        u16 height;
        float minDepth;
        float maxDepth;
    };

    struct gpu_set_scissor_command : public gpu_command_base
    {
        gpu_set_scissor_command(u16 _ox, u16 _oy, u16 _ex, u16 _ey)
        : gpu_command_base(gpu_command_type::SET_SCISSOR)
        , offset_x(_ox)
        , offset_y(_oy)
        , extent_x(_ex)
        , extent_y(_ey)
        {}

        u16 offset_x;
        u16 offset_y;
        u16 extent_x;
        u16 extent_y;
    };

    struct gpu_bind_pipeline_command : public gpu_command_base
    {
        gpu_bind_pipeline_command(pipeline_state_handle _pso_handle) : gpu_command_base(gpu_command_type::BIND_PIPELINE), pso_handle(_pso_handle) {}

        pipeline_state_handle pso_handle;
    };

    struct gpu_bind_uniform_buffer_command : public gpu_command_base
    {
        gpu_bind_uniform_buffer_command(pipeline_state_handle _pso_handle, uniform_buffer_handle _ub_handle, u8 _position) : gpu_command_base(gpu_command_type::BIND_UNIFORM_BUFFER), pso_handle(_pso_handle), ub_handle(_ub_handle), position(_position) {}

        pipeline_state_handle pso_handle;
        uniform_buffer_handle ub_handle;
        u8 position;
    };

    struct gpu_bind_texture_command : public gpu_command_base
    {
        gpu_bind_texture_command(pipeline_state_handle _pso_handle, texture_handle _tex_handle, u8 _position) : gpu_command_base(gpu_command_type::BIND_TEXTURE), pso_handle(_pso_handle), tex_handle(_tex_handle), position(_position) {}
        
        pipeline_state_handle pso_handle;
        texture_handle tex_handle;
        u8 position;
    };

    struct gpu_bind_vertex_command : public gpu_command_base
    {
        gpu_bind_vertex_command(mesh_handle _msh_handle) : gpu_command_base(gpu_command_type::BIND_VERTEX_BUFFER), msh_handle(_msh_handle) {}

        mesh_handle msh_handle;
    };

    struct gpu_bind_index_command : public gpu_command_base
    {
        gpu_bind_index_command(mesh_handle _msh_handle) : gpu_command_base(gpu_command_type::BIND_INDEX_BUFFER), msh_handle(_msh_handle) {}

        mesh_handle msh_handle;
    };

    struct gpu_draw_vertex_command : public gpu_command_base
    {
        gpu_draw_vertex_command(u32 count) : gpu_command_base(gpu_command_type::DRAW_VERTEX), vertexCount(count) {}

        u32 vertexCount = 0;
    };

    struct gpu_draw_indexed_command : public gpu_command_base
    {
        gpu_draw_indexed_command(u32 count) : gpu_command_base(gpu_command_type::DRAW_INDEXED), indexCount(count) {}

        u32 indexCount = 0;
    };
}