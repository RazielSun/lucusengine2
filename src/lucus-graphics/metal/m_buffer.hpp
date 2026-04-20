#pragma once

#include "m_pch.hpp"

#include "core_types.hpp"
#include "render_types.hpp"

namespace lucus
{
    class m_buffer
    {
        public:
            void init(id<MTLDevice> device, size_t size, u32 count, shader_binding_stage in_stage = shader_binding_stage::VERTEX, MTLResourceOptions options = MTLResourceStorageModeShared);
            void cleanup();

            void write(const void* data, size_t size, size_t offset = 0);

            size_t getItemSize() const { return itemSize; }
            void* getMappedData(u32 index) const;

            id<MTLBuffer> get() const { return _buffer; }

            shader_binding_stage getStage() const { return stage; }

        private:
            id<MTLBuffer> _buffer{ nil };
            size_t itemSize;
            size_t bufferSize;
            size_t alignedSize;
            shader_binding_stage stage;
    };
}