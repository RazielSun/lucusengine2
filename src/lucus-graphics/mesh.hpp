#pragma once

#include "pch.hpp"

#include "object.hpp"

namespace lucus
{
    class mesh : public object
    {
        public:
            static mesh* create_factory() { return new mesh(); }

            int getDrawCount() const { return _drawCount; }
            void setDrawCount(int count) { _drawCount = count; }

            const mesh_handle& getHandle() const { return _mesh_handle; }
            void setHandle(const mesh_handle& handle) { _mesh_handle = handle; }

        private:
            int _drawCount{0};

            // transient
            mesh_handle _mesh_handle;
    };
}