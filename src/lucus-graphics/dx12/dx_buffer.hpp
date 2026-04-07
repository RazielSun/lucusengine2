#pragma once

#include "dx_pch.hpp"

#include "render_types.hpp"

namespace lucus
{
    class dx_buffer
    {
        public:
            void init(Com<ID3D12Device> device, uint32_t bufferSize);
            void cleanup();

            void write(const void* data, size_t size, size_t offset = 0);

            Com<ID3D12Resource> get() const { return _buffer; }

        private:
            Com<ID3D12Device> _device;
            Com<ID3D12Resource> _buffer;
            void* _mapped{ nullptr };
    };

    class dx_uniform_buffer
    {
        public:
            void init(Com<ID3D12Device> device, uint32_t bufferSize);
            void cleanup();

            void write(uint32_t index, const void* data, size_t size, size_t offset = 0);

            Com<ID3D12Resource> get(uint32_t index) const { return _buffers[index].get(); }

        protected:

        private:
            std::array<dx_buffer, g_framesInFlight> _buffers;
    };
}
