#pragma once

#include "pch.hpp"

#include <windows.h>

#include <wrl.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#ifndef NDEBUG
#include <dxgidebug.h>
#endif

namespace lucus
{
    template<typename T>
    using Com = Microsoft::WRL::ComPtr<T>;

    // Helper utility converts D3D API failures into exceptions.
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

    // Function that reads from a binary file asynchronously.
	// inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	// {
	// 	using namespace Windows::Storage;
	// 	using namespace Concurrency;

	// 	auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

	// 	return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([](StorageFile^ file)
	// 	{
	// 		return FileIO::ReadBufferAsync(file);
	// 	}).then([](Streams::IBuffer^ fileBuffer) -> std::vector<byte>
	// 	{
	// 		std::vector<byte> returnBuffer;
	// 		returnBuffer.resize(fileBuffer->Length);
	// 		Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
	// 		return returnBuffer;
	// 	});
	// }
}