#include "mesh.hpp"

#include "filesystem.hpp"

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#define TINYGLTF3_ENABLE_STB_IMAGE
#include "tiny_gltf_v3.h"

namespace
{
    const char* severity_to_string(tg3_severity severity)
    {
        switch (severity)
        {
            case TG3_SEVERITY_INFO:
                return "info";
            case TG3_SEVERITY_WARNING:
                return "warning";
            case TG3_SEVERITY_ERROR:
                return "error";
            default:
                return "unknown";
        }
    }

    const tg3_accessor* get_accessor(const tg3_model& model, int32_t accessorIndex)
    {
        if (accessorIndex < 0 || static_cast<uint32_t>(accessorIndex) >= model.accessors_count)
        {
            return nullptr;
        }

        return &model.accessors[accessorIndex];
    }

    const tg3_buffer_view* get_buffer_view(const tg3_model& model, int32_t bufferViewIndex)
    {
        if (bufferViewIndex < 0 || static_cast<uint32_t>(bufferViewIndex) >= model.buffer_views_count)
        {
            return nullptr;
        }

        return &model.buffer_views[bufferViewIndex];
    }

    const tg3_buffer* get_buffer(const tg3_model& model, int32_t bufferIndex)
    {
        if (bufferIndex < 0 || static_cast<uint32_t>(bufferIndex) >= model.buffers_count)
        {
            return nullptr;
        }

        return &model.buffers[bufferIndex];
    }

    const uint8_t* get_accessor_data(const tg3_model& model, const tg3_accessor& accessor, int32_t& outStride)
    {
        const tg3_buffer_view* bufferView = get_buffer_view(model, accessor.buffer_view);
        if (!bufferView)
        {
            return nullptr;
        }

        const tg3_buffer* buffer = get_buffer(model, bufferView->buffer);
        if (!buffer || !buffer->data.data)
        {
            return nullptr;
        }

        outStride = tg3_accessor_byte_stride(&accessor, bufferView);
        if (outStride <= 0)
        {
            return nullptr;
        }

        const uint64_t dataOffset = bufferView->byte_offset + accessor.byte_offset;
        if (dataOffset >= buffer->data.count)
        {
            return nullptr;
        }

        return buffer->data.data + dataOffset;
    }

    bool read_positions(const tg3_model& model, const tg3_primitive& primitive, std::vector<lucus::vertex>& outVertices)
    {
        int32_t positionAccessorIndex = -1;
        for (uint32_t attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
        {
            const tg3_str_int_pair& attribute = primitive.attributes[attributeIndex];
            if (tg3_str_equals_cstr(attribute.key, "POSITION"))
            {
                positionAccessorIndex = attribute.value;
                break;
            }
        }

        if (positionAccessorIndex < 0)
        {
            std::printf("mesh::create_gltf_factory: primitive has no POSITION attribute.\n");
            return false;
        }

        const tg3_accessor* accessor = get_accessor(model, positionAccessorIndex);
        if (!accessor)
        {
            std::printf("mesh::create_gltf_factory: invalid POSITION accessor.\n");
            return false;
        }

        if (accessor->component_type != TG3_COMPONENT_TYPE_FLOAT || accessor->type != TG3_TYPE_VEC3)
        {
            std::printf("mesh::create_gltf_factory: POSITION accessor must be float VEC3.\n");
            return false;
        }

        int32_t stride = 0;
        const uint8_t* data = get_accessor_data(model, *accessor, stride);
        if (!data)
        {
            std::printf("mesh::create_gltf_factory: failed to access POSITION buffer data.\n");
            return false;
        }

        outVertices.resize(static_cast<size_t>(accessor->count));
        for (uint64_t i = 0; i < accessor->count; ++i)
        {
            const float* position = reinterpret_cast<const float*>(data + (i * stride));
            outVertices[static_cast<size_t>(i)].position = glm::vec3(position[0], position[1], position[2]);
            // std::printf("DEBUG::mesh::create_gltf_factory: %lu POS (%f, %f, %f).\n", i, position[0], position[1], position[2]);
        }

        return true;
    }

    int32_t find_attribute_accessor_index(const tg3_primitive& primitive, const char* attributeName)
    {
        for (uint32_t attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
        {
            const tg3_str_int_pair& attribute = primitive.attributes[attributeIndex];
            // printf("DEBUG::find_attribute_accessor_index: checking attribute %d: %.*s\n", attributeIndex, (int)attribute.key.len, attribute.key.data);
            if (tg3_str_equals_cstr(attribute.key, attributeName))
            {
                return attribute.value;
            }
        }

        return -1;
    }

    float read_color_component(const uint8_t* data, int32_t componentType, bool normalized)
    {
        switch (componentType)
        {
            case TG3_COMPONENT_TYPE_FLOAT:
                return *reinterpret_cast<const float*>(data);
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
            {
                const float value = static_cast<float>(*reinterpret_cast<const uint8_t*>(data));
                return normalized ? (value / 255.0f) : value;
            }
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                const float value = static_cast<float>(*reinterpret_cast<const uint16_t*>(data));
                return normalized ? (value / 65535.0f) : value;
            }
            default:
                return 0.0f;
        }
    }

    bool read_vertex_colors(const tg3_model& model, const tg3_primitive& primitive, std::vector<lucus::vertex>& outVertices)
    {
        const int32_t colorAccessorIndex = find_attribute_accessor_index(primitive, "COLOR_0");
        if (colorAccessorIndex < 0)
        {
            return true;
        }

        const tg3_accessor* accessor = get_accessor(model, colorAccessorIndex);
        if (!accessor)
        {
            std::printf("mesh::create_gltf_factory: invalid COLOR_0 accessor.\n");
            return false;
        }

        if (accessor->type != TG3_TYPE_VEC3 && accessor->type != TG3_TYPE_VEC4)
        {
            std::printf("mesh::create_gltf_factory: COLOR_0 accessor must be VEC3 or VEC4.\n");
            return false;
        }

        if (accessor->component_type != TG3_COMPONENT_TYPE_FLOAT &&
            accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_BYTE &&
            accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
            std::printf("mesh::create_gltf_factory: unsupported COLOR_0 component type %d.\n", accessor->component_type);
            return false;
        }

        if (accessor->count != outVertices.size())
        {
            std::printf("mesh::create_gltf_factory: COLOR_0 count does not match POSITION count.\n");
            return false;
        }

        int32_t stride = 0;
        const uint8_t* data = get_accessor_data(model, *accessor, stride);
        if (!data)
        {
            std::printf("mesh::create_gltf_factory: failed to access COLOR_0 buffer data.\n");
            return false;
        }

        const int32_t componentSize = tg3_component_size(accessor->component_type);
        if (componentSize <= 0)
        {
            std::printf("mesh::create_gltf_factory: invalid COLOR_0 component size.\n");
            return false;
        }

        for (uint64_t i = 0; i < accessor->count; ++i)
        {
            const uint8_t* vertexData = data + (i * stride);

            outVertices[static_cast<size_t>(i)].color = glm::vec3(
                read_color_component(vertexData + (componentSize * 0), accessor->component_type, accessor->normalized),
                read_color_component(vertexData + (componentSize * 1), accessor->component_type, accessor->normalized),
                read_color_component(vertexData + (componentSize * 2), accessor->component_type, accessor->normalized));
            // printf("DEBUG::mesh::create_gltf_factory: %lu COL (%f, %f, %f).\n", i, outVertices[static_cast<size_t>(i)].color.r, outVertices[static_cast<size_t>(i)].color.g, outVertices[static_cast<size_t>(i)].color.b);
        }

        return true;
    }

    uint32_t read_index_value(const uint8_t* data, int32_t componentType)
    {
        switch (componentType)
        {
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                return *reinterpret_cast<const uint8_t*>(data);
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                return *reinterpret_cast<const uint16_t*>(data);
            case TG3_COMPONENT_TYPE_UNSIGNED_INT:
                return *reinterpret_cast<const uint32_t*>(data);
            default:
                return 0;
        }
    }

    bool read_indices(const tg3_model& model, const tg3_primitive& primitive, std::vector<uint32_t>& outIndices)
    {
        if (primitive.indices < 0)
        {
            return true;
        }

        const tg3_accessor* accessor = get_accessor(model, primitive.indices);
        if (!accessor)
        {
            std::printf("mesh::create_gltf_factory: invalid index accessor.\n");
            return false;
        }

        if (accessor->type != TG3_TYPE_SCALAR)
        {
            std::printf("mesh::create_gltf_factory: index accessor must be scalar.\n");
            return false;
        }

        if (accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_BYTE &&
            accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_SHORT &&
            accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_INT)
        {
            std::printf("mesh::create_gltf_factory: unsupported index component type %d.\n", accessor->component_type);
            return false;
        }

        int32_t stride = 0;
        const uint8_t* data = get_accessor_data(model, *accessor, stride);
        if (!data)
        {
            std::printf("mesh::create_gltf_factory: failed to access index buffer data.\n");
            return false;
        }

        outIndices.resize(static_cast<size_t>(accessor->count));
        for (uint64_t i = 0; i < accessor->count; ++i)
        {
            outIndices[static_cast<size_t>(i)] = read_index_value(data + (i * stride), accessor->component_type);
            // std::printf("DEBUG::mesh::create_gltf_factory: %lu IDX (%u).\n", i, outIndices[static_cast<size_t>(i)]);
        }

        return true;
    }

    const tg3_mesh* get_first_mesh(const tg3_model& model)
    {
        if (model.meshes_count == 0 || !model.meshes)
        {
            return nullptr;
        }

        return &model.meshes[0];
    }
}

using namespace lucus;

mesh* mesh::create_nodata_factory(int drawCount)
{
    mesh* m = new mesh();
    m->_initialDrawCount = drawCount;
    return m;
}

mesh* mesh::create_gltf_factory(const std::string& fileName)
{
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;

    const std::string filePath = filesystem::instance().get_path(fileName);
    const tg3_error_code result = tinygltf3::parse_file(model, errors, filePath.c_str());

    for (uint32_t i = 0; i < errors.count(); ++i)
    {
        const tg3_error_entry* entry = errors.entry(i);
        if (!entry)
        {
            continue;
        }

        std::fprintf(stderr, "[%s] %s\n", severity_to_string(entry->severity), entry->message);
    }

    if (result != TG3_OK)
    {
        std::printf("mesh::create_gltf_factory: failed to load glTF file %s.\n", filePath.c_str());
        return nullptr;
    }

    const tg3_mesh* sourceMesh = get_first_mesh(*model.get());
    if (!sourceMesh || sourceMesh->primitives_count == 0)
    {
        std::printf("mesh::create_gltf_factory: glTF file has no mesh primitives.\n");
        return nullptr;
    }

    const tg3_primitive& primitive = sourceMesh->primitives[0];
    if (primitive.mode != -1 && primitive.mode != TG3_MODE_TRIANGLES)
    {
        std::printf("mesh::create_gltf_factory: only triangle primitives are supported right now.\n");
        return nullptr;
    }

    std::vector<vertex> vertices;
    if (!read_positions(*model.get(), primitive, vertices))
    {
        return nullptr;
    }

    if (!read_vertex_colors(*model.get(), primitive, vertices))
    {
        return nullptr;
    }

    std::vector<uint32_t> indices;
    if (!read_indices(*model.get(), primitive, indices))
    {
        return nullptr;
    }

    mesh* loadedMesh = new mesh();
    loadedMesh->_initialFilePath = filePath;
    loadedMesh->setVertices(std::move(vertices));
    loadedMesh->setIndices(std::move(indices));

    return loadedMesh;
}

mesh* mesh::create_cube_factory()
{
    mesh* m = new mesh();
    m->_initialDrawCount = 36;
    m->_vertices.resize(8);

    m->_vertices[0].position = glm::vec3(-0.5f, -0.5f, -0.5f);
    m->_vertices[1].position = glm::vec3(0.5f, -0.5f, -0.5f);
    m->_vertices[2].position = glm::vec3(0.5f, 0.5f, -0.5f);
    m->_vertices[3].position = glm::vec3(-0.5f, 0.5f, -0.5f);
    m->_vertices[4].position = glm::vec3(-0.5f, -0.5f, 0.5f);
    m->_vertices[5].position = glm::vec3(0.5f, -0.5f, 0.5f);
    m->_vertices[6].position = glm::vec3(0.5f, 0.5f, 0.5f);
    m->_vertices[7].position = glm::vec3(-0.5f, 0.5f, 0.5f);

    m->_indices = {
        0, 3, 2, 2, 1, 0, // back
        4, 5, 6, 6, 7, 4, // front
        0, 4, 7, 7, 3, 0, // left
        1, 2, 6, 6, 5, 1, // right
        3, 7, 6, 6, 2, 3, // top
        0, 1, 5, 5, 4, 0  // bottom
    };

    m->_vertices[0].color = glm::vec3(1.0f, 0.0f, 0.0f);
    m->_vertices[1].color = glm::vec3(0.0f, 1.0f, 0.0f);
    m->_vertices[2].color = glm::vec3(0.0f, 0.0f, 1.0f);
    m->_vertices[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
    m->_vertices[4].color = glm::vec3(1.0f, 0.0f, 1.0f);
    m->_vertices[5].color = glm::vec3(0.0f, 1.0f, 1.0f);
    m->_vertices[6].color = glm::vec3(1.0f, 1.0f, 1.0f);
    m->_vertices[7].color = glm::vec3(0.0f, 0.0f, 0.0f);

    return m;
}

mesh* mesh::create_triangle_factory()
{
    mesh* m = new mesh();
    m->_initialDrawCount = 3;
    m->_vertices.resize(3);

    m->_vertices[1].position = glm::vec3(0.0f, -0.5f, 0.0f);
    m->_vertices[0].position = glm::vec3(0.5f, 0.5f, 0.0f);
    m->_vertices[2].position = glm::vec3(-0.5f, 0.5f, 0.0f);

    m->_vertices[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
    m->_vertices[0].color = glm::vec3(0.0f, 1.0f, 0.0f);
    m->_vertices[2].color = glm::vec3(0.0f, 0.0f, 1.0f);

    return m;
}

uint64_t mesh::getHash() const
{
    const uint64_t fileHash = static_cast<uint64_t>(std::hash<std::string>{}(_initialFilePath));
    const uint64_t drawCountHash = static_cast<uint64_t>(_initialDrawCount);
    return fileHash ^ (drawCountHash << 1);
}
