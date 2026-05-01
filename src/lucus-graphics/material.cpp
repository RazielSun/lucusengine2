#include "material.hpp"

#include "renderer.hpp"

using namespace lucus;

material* material::create_factory(const std::string& shaderName)
{
    material* mat = new material();
    mat->setShaderName(shaderName);
    return mat;
}

void lucus::material::setShaderName(const std::string &shaderName)
{
    _shaderName = shaderName;
    _shaderNameDeferred = shaderName + "_deferred";
}

const std::string &lucus::material::getShaderName() const
{
    if (bDeferred && renderer::instance().getMode() == render_mode::DEFERRED)
    {
        return _shaderNameDeferred;
    }
    return _shaderName;
}

void material::addTexture(texture* tex, u8 slot_index)
{
    assert(tex);

    _textures.push_back(texture_slot());

    texture_slot& ts = _textures.back();
    ts.texture = intrusive_ptr<texture>(tex);
    ts.slot_index = slot_index;
}

void lucus::material::setDeferredMode(bool bInDeferred)
{
    bDeferred = bInDeferred;
}

u32 material::getHash() const
{
    const u32 shaderHash = static_cast<u32>(std::hash<std::string>{}(_shaderName));
    const u32 frameUniformBufferHash = static_cast<u32>(_useFrameUniformBuffer);
    const u32 objectUniformBufferHash = static_cast<u32>(_useObjectUniformBuffer);
    const u32 vertexIndexBufferHash = static_cast<u32>(_useVertexIndexBuffers);
    const u32 texturesCountHash = getTexturesCount();

    return shaderHash ^
        (frameUniformBufferHash << 1) ^
        (objectUniformBufferHash << 2) ^
        (vertexIndexBufferHash << 3) ^
        (texturesCountHash << 5);
}
