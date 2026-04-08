#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"

namespace lucus
{
    class mesh : public object
    {
        public:
            static mesh* create_factory(const std::string& filePath = std::string(), int drawCount = 0);
            

            const mesh_handle& getHandle() const { return _mesh_handle; }
            void setHandle(const mesh_handle& handle) { _mesh_handle = handle; }

            const std::vector<vertex>& getVertices() const { return _vertices; }
            void setVertices(std::vector<vertex> vertices) { _vertices = std::move(vertices); }

            const std::vector<uint32_t>& getIndices() const { return _indices; }
            void setIndices(std::vector<uint32_t> indices) { _indices = std::move(indices); }

            uint64_t getHash() const;

            uint32_t getDrawCount() const { return _vertices.size() > 0 ? static_cast<uint32_t>(_vertices.size()) : _initialDrawCount; }

        private:
            int _initialDrawCount{0};
            std::string _initialFilePath;

            std::vector<vertex> _vertices;
            std::vector<uint32_t> _indices;

            // transient
            mesh_handle _mesh_handle;
    };

    mesh* create_cube_factory();
    mesh* create_triangle_factory();
}
