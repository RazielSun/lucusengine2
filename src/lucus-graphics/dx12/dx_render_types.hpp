#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    struct dx_commandbuffer_pool
    {
        public:
            void init(Com<ID3D12Device> device);
            void cleanup();
        
            std::array<Com<ID3D12CommandAllocator>, g_framesInFlight> commandAllocators{};
    };

    struct dx_viewport
    {
        D3D12_VIEWPORT viewport{};
        D3D12_RECT scissor{};
    };
}