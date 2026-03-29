#pragma once

#include "dx_pch.hpp"

namespace lucus
{
    class dx_buffer
    {
        public:
            void init(Com<ID3D12Device> device, uint32_t bufferSize);
            void cleanup();

            void write(uint32_t index, const void* data, size_t size);

        protected:

        private:
            Com<ID3D12Device> _device;

            std::array<Com<ID3D12Resource>, g_framesInFlight> uniformBuffers;
            std::vector<void*> uniformBuffersMapped;
    };
}
