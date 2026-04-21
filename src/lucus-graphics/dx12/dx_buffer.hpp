#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class dx_buffer
    {
        public:
            void init(Com<ID3D12Device> device, size_t bufferSize);
            void cleanup();

            void write(const void* data, size_t size, size_t offset = 0);

            Com<ID3D12Resource> get() const { return _buffer; }

            void* getMappedData() const { return _mapped; }

        private:
            Com<ID3D12Device> _device;
            Com<ID3D12Resource> _buffer;
            void* _mapped{ nullptr };
            size_t alignedSize;
            size_t _size;
    };

    class dx_uniform_buffer
    {
        public:
            void init(Com<ID3D12Device> device, size_t bufferSize);
            void cleanup();

            void write(u32 index, const void* data, size_t size, size_t offset = 0);

            Com<ID3D12Resource> get(u32 index) const { return _buffers[index].get(); }

            void* getMappedData(u32 index) const { return _buffers[index].getMappedData(); }

        protected:

        private:
            std::array<dx_buffer, g_framesInFlight> _buffers;
    };
}
