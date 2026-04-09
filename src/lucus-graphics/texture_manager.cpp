#include "texture_manager.hpp"

using namespace lucus;

void texture_manager::textureTrack(texture* tex)
{
    assert(tex);

    _pending.push_back(intrusive_ptr<texture>(tex));
}

void texture_manager::textureLoaded(texture* tex)
{
    assert(tex);

    _textures.push_back(intrusive_ptr<texture>(tex));
}