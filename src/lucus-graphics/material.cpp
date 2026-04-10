#include "material.hpp"

using namespace lucus;

material* material::create_factory(const std::string& shaderName)
{
    material* mat = new material();
    mat->setShaderName(shaderName);
    return mat;
}

void material::setTexture(texture* tex, uint32_t index)
{
    assert(tex);

    std::vector<intrusive_ptr<texture>>::iterator it;

    it = _textures.begin() + index;
    _textures.insert(it, intrusive_ptr<texture>(tex));
}

uint64_t material::getHash() const
{
    const uint64_t shaderHash = static_cast<uint64_t>(std::hash<std::string>{}(_shaderName));
    const uint64_t uniformBufferHash = static_cast<uint64_t>(_useUniformBuffers);
    const uint64_t vertexIndexBufferHash = static_cast<uint64_t>(_useVertexIndexBuffers);
    const uint64_t texturesCountHash = static_cast<uint64_t>(_useTexturesCount);

    return shaderHash ^ (uniformBufferHash << 1) ^ (vertexIndexBufferHash << 2) ^ (texturesCountHash << 3);
}
