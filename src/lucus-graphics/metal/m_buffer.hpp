#pragma once

#include "m_pch.hpp"

namespace lucus
{
    class m_buffer
    {
        public:
            void init(id<MTLDevice> device, size_t size, u32 count, MTLResourceOptions options = MTLResourceStorageModeShared);
            void cleanup();

            void write(const void* data, size_t size, size_t offset = 0);

            size_t getItemSize() const { return itemSize; }
            void* getMappedData(u32 index) const;

            id<MTLBuffer> get() const { return _buffer; }

        private:
            id<MTLBuffer> _buffer{ nil };
            size_t itemSize;
            size_t bufferSize;
            size_t alignedSize;
    };
}