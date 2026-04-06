#include "material.hpp"

using namespace lucus;

material* material::create_factory(const std::string& shaderName)
{
    material* mat = new material();
    mat->setShaderName(shaderName);
    return mat;
}

uint64_t material::getHash() const
{
    const uint64_t shaderHash = static_cast<uint64_t>(std::hash<std::string>{}(_shaderName));
    const uint64_t uniformBufferHash = static_cast<uint64_t>(_useUniformBuffers);
    const uint64_t vertexIndexBufferHash = static_cast<uint64_t>(_useVertexIndexBuffers);

    return shaderHash ^ (uniformBufferHash << 1) ^ (vertexIndexBufferHash << 2);
}
