#include "material.hpp"

using namespace lucus;

material* material::create_factory(const std::string& shaderName)
{
    material* mat = new material();
    mat->setShaderName(shaderName);
    return mat;
}

uint32_t material::getHash() const
{
    const uint32_t shaderHash = static_cast<uint32_t>(std::hash<std::string>{}(_shaderName));
    const uint32_t uniformBufferHash = static_cast<uint32_t>(_useUniformBuffers);

    return shaderHash ^ (uniformBufferHash << 1);
}
