#include "material.hpp"

using namespace lucus;

void material_instance::addTexture(texture* tex, u8 slot_index)
{
    assert(tex);

    _textures.push_back(texture_slot());

    texture_slot& ts = _textures.back();
    ts.tex = intrusive_ptr<texture>(tex);
    ts.slot_index = slot_index;
}

material::material()
{
    _rootInstance = intrusive_ptr<material_instance>(new material_instance(this));
    // new object starts at refcount 1 and intrusive_ptr adds another
    _rootInstance->releaseRef();
}

material* material::create_factory(const std::string& shaderName)
{
    material* mat = new material();
    mat->setShaderName(shaderName);
    return mat;
}

material_instance* material::createInstance()
{
    material_instance* inst = new material_instance(this);
    inst->_materialRef.reset(this);
    inst->_baseColor = _rootInstance->_baseColor;
    inst->_materialParams = _rootInstance->_materialParams;
    inst->_textures = _rootInstance->_textures;
    return inst;
}

void material::setShaderName(const std::string &shaderName)
{
    _shaderName = shaderName;
    _shaderNameDeferred = shaderName + "_deferred";
    resetPSOHandles();
}

const std::string& material::getShaderName(render_pass_name pass) const
{
    if (bDeferred && pass == render_pass_name::GBUFFER_PASS)
    {
        return _shaderNameDeferred;
    }
    return _shaderName;
}

void material::setUseVertexIndexBuffers(bool useVertexIndexBuffers)
{
    _useVertexIndexBuffers = useVertexIndexBuffers;
    resetPSOHandles();
}

void material::setDeferredMode(bool bInDeferred)
{
    bDeferred = bInDeferred;
    resetPSOHandles();
}

pipeline_state_desc material::getPipelineStateDesc(render_pass_name pass) const
{
    return pipeline_state_desc{
        .shaderName = getShaderName(pass),
        .pass = pass,
        .useVertexIndexBuffers = _useVertexIndexBuffers,
    };
}
