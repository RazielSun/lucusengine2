#include "render_object.hpp"

using namespace lucus;

void render_object::setMaterialInstance(material_instance* instance)
{
    _materialInstance.reset(instance);
    _material.reset(instance ? instance->getMaterial() : nullptr);
}
