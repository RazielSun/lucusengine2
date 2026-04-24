#include "dx_utils.hpp"

D3D12_RESOURCE_STATES lucus::utils::to_dx12_resource_state(lucus::resource_state state)
{
    using namespace lucus;
    
    switch (state)
    {
        case resource_state::UNDEFINED:
            return D3D12_RESOURCE_STATE_COMMON;
        case resource_state::DEPTH_WRITE:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case resource_state::SHADER_READ:
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case resource_state::COLOR_WRITE:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case resource_state::PRESENT:
            return D3D12_RESOURCE_STATE_PRESENT;
        case resource_state::COPY_SRC:
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case resource_state::COPY_DST:
            return D3D12_RESOURCE_STATE_COPY_DEST;
    }

    assert(false);
    return D3D12_RESOURCE_STATE_COMMON;
}