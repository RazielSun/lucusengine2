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

            void write(uint32_t index, const void* data, size_t size);

            Com<ID3D12Resource> get(uint32_t index) const { return uniformBuffers[index]; }

        protected:

        private:
            Com<ID3D12Device> _device;

            std::array<Com<ID3D12Resource>, g_framesInFlight> uniformBuffers;
            std::vector<void*> uniformBuffersMapped;
    };
}
