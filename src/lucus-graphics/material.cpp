#include "material.hpp"

using namespace lucus;

material* material::create_factory(const std::string& shaderName, int renderPass)
{
    material* mat = new material();
    mat->setShaderName(shaderName);
    mat->setRenderPass(renderPass);
    return mat;
}