#pragma once

#include "dx_pch.hpp"
#include "dx_render_types.hpp"

namespace lucus
{
    namespace utils
    {
        D3D12_RESOURCE_STATES to_dx12_resource_state(resource_state state);
    }
}
