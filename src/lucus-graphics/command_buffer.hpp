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
        BIND_SAMPLER,
        BIND_DESCRIPTION_TABLE,
        BIND_VERTEX_BUFFER,
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
        gpu_render_pass_begin_command(i32 _ox, i32 _oy, u32 _ex, u32 _ey)
        : gpu_command_base(gpu_command_type::RENDER_PASS_BEGIN)
        , offset_x(_ox)
        , offset_y(_oy)
        , extent_x(_ex)
        , extent_y(_ey)
        {}

        i32 offset_x;
        i32 offset_y;
        u32 extent_x;
        u32 extent_y;
    };

    struct gpu_render_pass_end_command : public gpu_command_base
    {
        gpu_render_pass_end_command()
        : gpu_command_base(gpu_command_type::RENDER_PASS_END)
        {}
    };

    struct gpu_set_viewport_command : public gpu_command_base
    {
        gpu_set_viewport_command(u32 _x, u32 _y, u32 _width, u32 _height)
        : gpu_command_base(gpu_command_type::SET_VIEWPORT)
        , x(_x)
        , y(_y)
        , width(_width)
        , height(_height)
        {}

        u32 x;
        u32 y;
        u32 width;
        u32 height;
    };

    struct gpu_set_scissor_command : public gpu_command_base
    {
        gpu_set_scissor_command(i32 _ox, i32 _oy, u32 _ex, u32 _ey)
        : gpu_command_base(gpu_command_type::SET_SCISSOR)
        , offset_x(_ox)
        , offset_y(_oy)
        , extent_x(_ex)
        , extent_y(_ey)
        {}

        i32 offset_x;
        i32 offset_y;
        u32 extent_x;
        u32 extent_y;
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

    struct gpu_bind_sampler_command : public gpu_command_base
    {
        gpu_bind_sampler_command(pipeline_state_handle _pso_handle, sampler_handle _smpl_handle, u8 _position) : gpu_command_base(gpu_command_type::BIND_SAMPLER), pso_handle(_pso_handle), smpl_handle(_smpl_handle), position(_position) {}
        
        pipeline_state_handle pso_handle;
        sampler_handle smpl_handle;
        u8 position;
    };

    struct gpu_bind_description_table_command : public gpu_command_base
    {
        gpu_bind_description_table_command() : gpu_command_base(gpu_command_type::BIND_DESCRIPTION_TABLE) {}
    };

    struct gpu_bind_vertex_command : public gpu_command_base
    {
        gpu_bind_vertex_command(mesh_handle _msh_handle, u8 _position) : gpu_command_base(gpu_command_type::BIND_VERTEX_BUFFER), msh_handle(_msh_handle), position(_position) {}

        mesh_handle msh_handle;
        u8 position;
    };

    struct gpu_draw_vertex_command : public gpu_command_base
    {
        gpu_draw_vertex_command(u32 count) : gpu_command_base(gpu_command_type::DRAW_VERTEX), vertexCount(count) {}

        u32 vertexCount = 0;
    };

    struct gpu_draw_indexed_command : public gpu_command_base
    {
        gpu_draw_indexed_command(mesh_handle _msh_handle, u32 count) : gpu_command_base(gpu_command_type::DRAW_INDEXED), msh_handle(_msh_handle), indexCount(count) {}

        mesh_handle msh_handle;
        u32 indexCount = 0;
    };
}