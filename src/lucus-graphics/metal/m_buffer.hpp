#pragma once

#include "m_pch.hpp"

namespace lucus
{
    class m_buffer
    {
        public:
            void init(id<MTLDevice> device, size_t size, MTLResourceOptions options = MTLResourceStorageModeShared);
            void cleanup();

            void write(const void* data, size_t size, size_t offset = 0);

            id<MTLBuffer> get() const { return _buffer; }

        private:
            id<MTLBuffer> _buffer{ nil };
    };
}