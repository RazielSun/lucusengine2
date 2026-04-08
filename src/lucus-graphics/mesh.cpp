#include "mesh.hpp"

#include "filesystem.hpp"


using namespace lucus;

mesh* mesh::create_factory(const std::string& filePath, int drawCount)
{
    mesh* m = new mesh();
    m->_initialFilePath = filePath;
    m->_initialDrawCount = drawCount;
    return m;
}

uint64_t mesh::getHash() const
{
    const uint64_t fileHash = static_cast<uint64_t>(std::hash<std::string>{}(_initialFilePath));
    const uint64_t drawCountHash = static_cast<uint64_t>(_initialDrawCount);
    return fileHash ^ (drawCountHash << 1);
}

mesh* lucus::create_cube_factory()
{
    std::vector<vertex> vertices;
    vertices.resize(8);

    vertices[0].position = glm::vec3(-0.5f, -0.5f, -0.5f);
    vertices[1].position = glm::vec3(0.5f, -0.5f, -0.5f);
    vertices[2].position = glm::vec3(0.5f, 0.5f, -0.5f);
    vertices[3].position = glm::vec3(-0.5f, 0.5f, -0.5f);
    vertices[4].position = glm::vec3(-0.5f, -0.5f, 0.5f);
    vertices[5].position = glm::vec3(0.5f, -0.5f, 0.5f);
    vertices[6].position = glm::vec3(0.5f, 0.5f, 0.5f);
    vertices[7].position = glm::vec3(-0.5f, 0.5f, 0.5f);

    vertices[0].color = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[1].color = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[2].color = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[3].color = glm::vec3(1.0f, 1.0f, 0.0f);
    vertices[4].color = glm::vec3(1.0f, 0.0f, 1.0f);
    vertices[5].color = glm::vec3(0.0f, 1.0f, 1.0f);
    vertices[6].color = glm::vec3(1.0f, 1.0f, 1.0f);
    vertices[7].color = glm::vec3(0.0f, 0.0f, 0.0f);

    std::vector<uint32_t> indices = {
        0, 3, 2, 2, 1, 0, // back
        4, 5, 6, 6, 7, 4, // front
        0, 4, 7, 7, 3, 0, // left
        1, 2, 6, 6, 5, 1, // right
        3, 7, 6, 6, 2, 3, // top
        0, 1, 5, 5, 4, 0  // bottom
    };

    mesh* m = mesh::create_factory("", static_cast<int>(indices.size()));
    m->setVertices(std::move(vertices));
    m->setIndices(std::move(indices));
    return m;
}

mesh* lucus::create_triangle_factory()
{
    std::vector<vertex> vertices;
    vertices.resize(3);

    vertices[1].position = glm::vec3(0.0f, -0.5f, 0.0f);
    vertices[0].position = glm::vec3(0.5f, 0.5f, 0.0f);
    vertices[2].position = glm::vec3(-0.5f, 0.5f, 0.0f);

    vertices[1].color = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[0].color = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[2].color = glm::vec3(0.0f, 0.0f, 1.0f);

    mesh* m = mesh::create_factory("", static_cast<int>(vertices.size()));
    m->setVertices(std::move(vertices));
    return m;
}
