#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"

namespace lucus
{
    class mesh : public object
    {
        friend class renderer;

    public:
        static mesh* create_factory(const std::string& filePath = std::string(), int drawCount = 0);

        const std::vector<vertex>& getVertices() const { return _vertices; }
        void setVertices(std::vector<vertex> vertices) { _vertices = std::move(vertices); }

        const std::vector<u32>& getIndices() const { return _indices; }
        void setIndices(std::vector<u32> indices) { _indices = std::move(indices); }

        bool hasVertices() const { return !_vertices.empty(); }

        u32 getVertexCount() const { return _vertices.size() > 0 ? static_cast<u32>(_vertices.size()) : _initialDrawCount; }
        u32 getIndexCount() const { return static_cast<u32>(_indices.size()); }

        u32 getHash() const;
    
    protected:
        const mesh_handle& getHandle() const { return _mesh_handle; }
        void setHandle(const mesh_handle& handle) { _mesh_handle = handle; }

    private:
        int _initialDrawCount{0};
        std::string _initialFilePath;

        std::vector<vertex> _vertices;
        std::vector<u32> _indices;

        // transient
        mesh_handle _mesh_handle;
    };

    mesh* create_cube_factory();
    mesh* create_triangle_factory();
}
