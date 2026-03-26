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
    // TODO: Other params
    return std::hash<std::string>{}(_shaderName);
}