#pragma once

#include "pch.hpp"

#include <windows.h>

#include <wrl.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include <d3dx12.h>

#ifndef NDEBUG
#include <dxgidebug.h>
#endif

namespace lucus
{
    template<typename T>
    using Com = Microsoft::WRL::ComPtr<T>;

    inline void ThrowIfFailed(HRESULT hr, const char* msg)
    {
        if (FAILED(hr))
        {
            throw std::runtime_error(msg);
        }
    }

    inline std::wstring utf8_to_wstring(const std::string& str)
    {
        if (str.empty())
            return {};

        int size_needed = MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            static_cast<int>(str.size()),
            nullptr,
            0
        );

        if (size_needed <= 0)
            return {};

        std::wstring result;
        result.resize(size_needed);

        MultiByteToWideChar(
            CP_UTF8,
            0,
            str.c_str(),
            static_cast<int>(str.size()),
            &result[0],
            size_needed
        );

        return result;
    }

}