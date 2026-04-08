#include "scene.hpp"

using namespace lucus;

scene* scene::create_factory()
{
    return new scene();
}

render_object* scene::newObject()
{
    render_object* renderObject = new render_object();
    _objects.push_back(intrusive_ptr<render_object>(renderObject));
    return renderObject;
}

