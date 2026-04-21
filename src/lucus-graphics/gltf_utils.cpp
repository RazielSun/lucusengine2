#include "gltf_utils.hpp"

#include "filesystem.hpp"

#include "mesh.hpp"
#include "camera.hpp"
#include "lights.hpp"
#include "scene.hpp"

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#define TINYGLTF3_ENABLE_STB_IMAGE
#include "tiny_gltf_v3.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace utils
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

    bool read_tex_coords(const tg3_model& model, const tg3_primitive& primitive, std::vector<lucus::vertex>& outVertices)
    {
        const int32_t texCoordAccessorIndex = find_attribute_accessor_index(primitive, "TEXCOORD_0");
        if (texCoordAccessorIndex < 0)
        {
            return true;
        }

        const tg3_accessor* accessor = get_accessor(model, texCoordAccessorIndex);
        if (!accessor)
        {
            std::printf("mesh::create_gltf_factory: invalid TEXCOORD_0 accessor.\n");
            return false;
        }

        if (accessor->type != TG3_TYPE_VEC2)
        {
            std::printf("mesh::create_gltf_factory: TEXCOORD_0 accessor must be VEC2.\n");
            return false;
        }

        if (accessor->component_type != TG3_COMPONENT_TYPE_FLOAT &&
            accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_BYTE &&
            accessor->component_type != TG3_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
            std::printf("mesh::create_gltf_factory: unsupported TEXCOORD_0 component type %d.\n", accessor->component_type);
            return false;
        }

        if (accessor->count != outVertices.size())
        {
            std::printf("mesh::create_gltf_factory: TEXCOORD_0 count does not match POSITION count.\n");
            return false;
        }

        int32_t stride = 0;
        const uint8_t* data = get_accessor_data(model, *accessor, stride);
        if (!data)
        {
            std::printf("mesh::create_gltf_factory: failed to access TEXCOORD_0 buffer data.\n");
            return false;
        }

        const int32_t componentSize = tg3_component_size(accessor->component_type);
        if (componentSize <= 0)
        {
            std::printf("mesh::create_gltf_factory: invalid TEXCOORD_0 component size.\n");
            return false;
        }

        for (uint64_t i = 0; i < accessor->count; ++i)
        {
            const uint8_t* vertexData = data + (i * stride);
            outVertices[static_cast<size_t>(i)].texCoords = glm::vec2(
                read_color_component(vertexData + (componentSize * 0), accessor->component_type, accessor->normalized),
                read_color_component(vertexData + (componentSize * 1), accessor->component_type, accessor->normalized));
        }

        return true;
    }

    bool read_normals(const tg3_model& model, const tg3_primitive& primitive, std::vector<lucus::vertex>& outVertices)
    {
        const int32_t normalAccessorIndex = find_attribute_accessor_index(primitive, "NORMAL");
        if (normalAccessorIndex < 0)
        {
            return true;
        }

        const tg3_accessor* accessor = get_accessor(model, normalAccessorIndex);
        if (!accessor)
        {
            std::printf("mesh::create_gltf_factory: invalid NORMAL accessor.\n");
            return false;
        }

        if (accessor->component_type != TG3_COMPONENT_TYPE_FLOAT || accessor->type != TG3_TYPE_VEC3)
        {
            std::printf("mesh::create_gltf_factory: NORMAL accessor must be float VEC3.\n");
            return false;
        }

        if (accessor->count != outVertices.size())
        {
            std::printf("mesh::create_gltf_factory: NORMAL count does not match POSITION count.\n");
            return false;
        }

        int32_t stride = 0;
        const uint8_t* data = get_accessor_data(model, *accessor, stride);
        if (!data)
        {
            std::printf("mesh::create_gltf_factory: failed to access NORMAL buffer data.\n");
            return false;
        }

        for (uint64_t i = 0; i < accessor->count; ++i)
        {
            const float* normal = reinterpret_cast<const float*>(data + (i * stride));
            outVertices[static_cast<size_t>(i)].normal = glm::vec3(normal[0], normal[1], normal[2]);
        }

        return true;
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

    const tg3_mesh* get_mesh(const tg3_model& model, int32_t meshIndex)
    {
        if (meshIndex < 0 || static_cast<uint32_t>(meshIndex) >= model.meshes_count)
        {
            return nullptr;
        }

        return &model.meshes[meshIndex];
    }

    const tg3_node* get_node(const tg3_model& model, int32_t nodeIndex)
    {
        if (nodeIndex < 0 || static_cast<uint32_t>(nodeIndex) >= model.nodes_count)
        {
            return nullptr;
        }

        return &model.nodes[nodeIndex];
    }

    const tg3_scene* get_scene(const tg3_model& model, int32_t sceneIndex)
    {
        if (sceneIndex < 0 || static_cast<uint32_t>(sceneIndex) >= model.scenes_count)
        {
            return nullptr;
        }

        return &model.scenes[sceneIndex];
    }

    const tg3_material* get_material(const tg3_model& model, int32_t materialIndex)
    {
        if (materialIndex < 0 || static_cast<uint32_t>(materialIndex) >= model.materials_count)
        {
            return nullptr;
        }

        return &model.materials[materialIndex];
    }

    const tg3_light* get_light(const tg3_model& model, int32_t lightIndex)
    {
        if (lightIndex < 0 || static_cast<uint32_t>(lightIndex) >= model.lights_count)
        {
            return nullptr;
        }

        return &model.lights[lightIndex];
    }

    std::string str_to_string(tg3_str str)
    {
        if (!str.data || str.len == 0)
        {
            return {};
        }

        return std::string(str.data, str.len);
    }

    lucus::material* create_material_from_gltf(const tg3_material& sourceMaterial)
    {
        std::string shaderName = str_to_string(sourceMaterial.name);
        if (shaderName.empty())
        {
            shaderName = "simple_unlit";
        }

        lucus::material* mat = lucus::material::create_factory(shaderName);
        mat->setUseFrameUniformBuffer(true);
        mat->setUseObjectUniformBuffer(true);
        mat->setUseVertexIndexBuffers(true);
        return mat;
    }

    lucus::transform matrix_to_transform(const glm::mat4& matrix);

    glm::mat4 get_node_local_matrix(const tg3_node& node)
    {
        if (node.has_matrix)
        {
            glm::mat4 matrix(1.0f);
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    matrix[column][row] = static_cast<float>(node.matrix[column * 4 + row]);
                }
            }
            return matrix;
        }

        const glm::vec3 translation(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2]));
        const glm::quat rotation(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
        const glm::vec3 scale(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]));

        return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
    }

    glm::quat get_node_local_rotation(const tg3_node& node)
    {
        if (node.has_matrix)
        {
            return matrix_to_transform(get_node_local_matrix(node)).rotation;
        }

        return glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
    }

    lucus::transform matrix_to_transform(const glm::mat4& matrix)
    {
        lucus::transform result;

        result.position = glm::vec3(matrix[3]);

        glm::vec3 xAxis = glm::vec3(matrix[0]);
        glm::vec3 yAxis = glm::vec3(matrix[1]);
        glm::vec3 zAxis = glm::vec3(matrix[2]);

        result.scale.x = glm::length(xAxis);
        result.scale.y = glm::length(yAxis);
        result.scale.z = glm::length(zAxis);

        if (result.scale.x > 0.0f)
        {
            xAxis /= result.scale.x;
        }
        if (result.scale.y > 0.0f)
        {
            yAxis /= result.scale.y;
        }
        if (result.scale.z > 0.0f)
        {
            zAxis /= result.scale.z;
        }

        glm::mat3 rotationMatrix(1.0f);
        rotationMatrix[0] = xAxis;
        rotationMatrix[1] = yAxis;
        rotationMatrix[2] = zAxis;

        if (glm::determinant(rotationMatrix) < 0.0f)
        {
            result.scale.x = -result.scale.x;
            rotationMatrix[0] = -rotationMatrix[0];
        }

        result.rotation = glm::normalize(glm::quat_cast(rotationMatrix));
        return result;
    }

    lucus::mesh* create_mesh_from_primitive(const tg3_model& model, const tg3_primitive& primitive, const std::string& meshPath)
    {
        if (primitive.mode != -1 && primitive.mode != TG3_MODE_TRIANGLES)
        {
            return nullptr;
        }

        std::vector<lucus::vertex> vertices;
        if (!read_positions(model, primitive, vertices))
        {
            return nullptr;
        }

        if (!read_vertex_colors(model, primitive, vertices))
        {
            return nullptr;
        }

        if (!read_normals(model, primitive, vertices))
        {
            return nullptr;
        }

        if (!read_tex_coords(model, primitive, vertices))
        {
            return nullptr;
        }

        std::vector<uint32_t> indices;
        if (!read_indices(model, primitive, indices))
        {
            return nullptr;
        }

        lucus::mesh* loadedMesh = lucus::mesh::create_factory(meshPath, static_cast<int>(vertices.size()));
        loadedMesh->setVertices(std::move(vertices));
        loadedMesh->setIndices(std::move(indices));
        return loadedMesh;
    }
}

void lucus::stbi_deleter::operator()(u8* p) const
{
    stbi_image_free(p);
}

lucus::mesh* lucus::load_mesh_gltf(const std::string& fileName)
{
    using namespace utils;
    using namespace lucus;

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

    if (!read_normals(*model.get(), primitive, vertices))
    {
        return nullptr;
    }

    if (!read_tex_coords(*model.get(), primitive, vertices))
    {
        return nullptr;
    }

    std::vector<uint32_t> indices;
    if (!read_indices(*model.get(), primitive, indices))
    {
        return nullptr;
    }

    mesh* loadedMesh = mesh::create_factory(filePath, vertices.size());
    loadedMesh->setVertices(std::move(vertices));
    loadedMesh->setIndices(std::move(indices));

    return loadedMesh;
}

lucus::scene* lucus::load_scene_gltf(const std::string& fileName)
{
    return load_scene_with_material_gltf(fileName, nullptr);
}

lucus::scene* lucus::load_scene_with_material_gltf(const std::string& fileName, lucus::material* override_mat)
{
    using namespace utils;

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
        std::printf("load_scene_gltf: failed to load glTF file %s.\n", filePath.c_str());
        return nullptr;
    }

    const tg3_model& sourceModel = *model.get();
    scene* loadedScene = scene::create_factory();

    std::vector<intrusive_ptr<material>> materialCache(static_cast<size_t>(sourceModel.materials_count));
    if (!override_mat)
    {
        for (uint32_t materialIndex = 0; materialIndex < sourceModel.materials_count; ++materialIndex)
        {
            material* importedMaterial = create_material_from_gltf(sourceModel.materials[materialIndex]);
            materialCache[materialIndex] = intrusive_ptr<material>(importedMaterial);
            importedMaterial->releaseRef();
        }
    }

    std::vector<std::vector<mesh*>> meshCache(static_cast<size_t>(sourceModel.meshes_count));
    for (uint32_t meshIndex = 0; meshIndex < sourceModel.meshes_count; ++meshIndex)
    {
        const tg3_mesh& sourceMesh = sourceModel.meshes[meshIndex];
        meshCache[meshIndex].resize(static_cast<size_t>(sourceMesh.primitives_count), nullptr);

        for (uint32_t primitiveIndex = 0; primitiveIndex < sourceMesh.primitives_count; ++primitiveIndex)
        {
             std::string meshPath = filePath + "#" + str_to_string(sourceMesh.name);
            meshCache[meshIndex][primitiveIndex] = create_mesh_from_primitive(sourceModel, sourceMesh.primitives[primitiveIndex], meshPath);
        }
    }

    const auto loadNode = [&](auto&& self, int32_t nodeIndex, const glm::mat4& parentMatrix, const glm::quat& parentRotation) -> void
    {
        const tg3_node* node = get_node(sourceModel, nodeIndex);
        if (!node)
        {
            return;
        }

        const glm::mat4 worldMatrix = parentMatrix * get_node_local_matrix(*node);
        const glm::quat worldRotation = glm::normalize(parentRotation * get_node_local_rotation(*node));

        if (!loadedScene->getDirectionalLight() && node->light >= 0)
        {
            const tg3_light* sourceLight = get_light(sourceModel, node->light);
            if (sourceLight && tg3_str_equals_cstr(sourceLight->type, "directional"))
            {
                directional_light* dirLight = directional_light::create_factory();
                const transform lightTransform = matrix_to_transform(worldMatrix);
                dirLight->setPosition(lightTransform.position);
                dirLight->setRotation(worldRotation);
                loadedScene->setDirectionalLight(dirLight);
                dirLight->releaseRef();
            }
        }

        if (node->mesh >= 0)
        {
            const tg3_mesh* sourceMesh = get_mesh(sourceModel, node->mesh);
            if (sourceMesh)
            {
                const std::vector<mesh*>& primitiveMeshes = meshCache[static_cast<size_t>(node->mesh)];
                for (uint32_t primitiveIndex = 0; primitiveIndex < sourceMesh->primitives_count; ++primitiveIndex)
                {
                    mesh* primitiveMesh = primitiveMeshes[primitiveIndex];
                    if (!primitiveMesh)
                    {
                        continue;
                    }

                    render_object* obj = loadedScene->newObject();
                    obj->setMesh(primitiveMesh);
                    material* primitiveMaterial = override_mat;
                    if (!primitiveMaterial)
                    {
                        const tg3_material* sourceMaterial = get_material(sourceModel, sourceMesh->primitives[primitiveIndex].material);
                        if (sourceMaterial)
                        {
                            primitiveMaterial = materialCache[static_cast<size_t>(sourceMesh->primitives[primitiveIndex].material)].get();
                        }
                    }
                    obj->setMaterial(primitiveMaterial);
                    obj->setTransform(matrix_to_transform(worldMatrix));
                }
            }
        }

        if (!loadedScene->getCamera() &&
            node->camera >= 0 &&
            static_cast<uint32_t>(node->camera) < sourceModel.cameras_count)
        {
            camera* sceneCamera = camera::create_factory();
            const tg3_camera& sourceCamera = sourceModel.cameras[node->camera];
            const transform cameraTransform = matrix_to_transform(worldMatrix);
            sceneCamera->setPosition(cameraTransform.position);
            sceneCamera->setRotation(worldRotation);
            if (tg3_str_equals_cstr(sourceCamera.type, "perspective"))
            {
                sceneCamera->setFovYRadians(static_cast<float>(sourceCamera.perspective.yfov));
                sceneCamera->setZNear(static_cast<float>(sourceCamera.perspective.znear));
                if (sourceCamera.perspective.zfar > 0.0)
                {
                    sceneCamera->setZFar(static_cast<float>(sourceCamera.perspective.zfar));
                }
            }
            loadedScene->setCamera(sceneCamera);
        }

        for (uint32_t childIndex = 0; childIndex < node->children_count; ++childIndex)
        {
            self(self, node->children[childIndex], worldMatrix, worldRotation);
        }
    };

    const tg3_scene* sourceScene = nullptr;
    if (sourceModel.default_scene >= 0)
    {
        sourceScene = get_scene(sourceModel, sourceModel.default_scene);
    }
    if (!sourceScene && sourceModel.scenes_count > 0)
    {
        sourceScene = &sourceModel.scenes[0];
    }

    if (sourceScene)
    {
        for (uint32_t nodeIndex = 0; nodeIndex < sourceScene->nodes_count; ++nodeIndex)
        {
            loadNode(loadNode, sourceScene->nodes[nodeIndex], glm::mat4(1.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }
    }
    else
    {
        std::vector<uint8_t> isChild(static_cast<size_t>(sourceModel.nodes_count), 0);
        for (uint32_t nodeIndex = 0; nodeIndex < sourceModel.nodes_count; ++nodeIndex)
        {
            const tg3_node& node = sourceModel.nodes[nodeIndex];
            for (uint32_t childIndex = 0; childIndex < node.children_count; ++childIndex)
            {
                const int32_t childNodeIndex = node.children[childIndex];
                if (childNodeIndex >= 0 && static_cast<uint32_t>(childNodeIndex) < sourceModel.nodes_count)
                {
                    isChild[static_cast<size_t>(childNodeIndex)] = 1;
                }
            }
        }

        for (uint32_t nodeIndex = 0; nodeIndex < sourceModel.nodes_count; ++nodeIndex)
        {
            if (!isChild[static_cast<size_t>(nodeIndex)])
            {
                loadNode(loadNode, static_cast<int32_t>(nodeIndex), glm::mat4(1.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            }
        }
    }

    return loadedScene;
}

lucus::stbi_image_ptr lucus::load_texture(const std::string& fileName, int& texWidth, int& texHeight, int& texChannels, texture_format format)
{
    const std::string filePath = filesystem::instance().get_path(fileName);
    stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, (u8)format);
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    return stbi_image_ptr(pixels);
}
