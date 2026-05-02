#include "texture.hpp"

#include "texture_manager.hpp"

using namespace lucus;

texture* texture::create_factory()
{
    texture* tex = new texture();
    texture_manager::instance().textureTrack(tex);
    return tex;
}

texture* texture::create_one_factory(u8 r, u8 g, u8 b, u8 a)
{
    static u32 s_uid = 0xFFFF0000;
    texture* tex = create_factory();
    tex->_uid = ++s_uid;
    tex->data = { r, g, b, a};
    tex->imageWidth = 1;
    tex->imageHeight = 1;
    tex->imageChannels = 4; // TODO ?
    tex->bInitialized = true;
    return tex;
}

void texture::initResource()
{
    if (!_fileName.empty())
    {
        int texWidth, texHeight, texChannels;
        _imageResource = load_texture(_fileName, texWidth, texHeight, texChannels, _imageFormat);

        imageWidth = static_cast<u32>(texWidth);
        imageHeight = static_cast<u32>(texHeight);
        imageChannels = static_cast<u32>(texChannels);
    }

    bInitialized = true;
}

void texture::freeResource()
{
    if (_imageResource)
    {
        _imageResource.reset();
    }
}

u32 texture::getHash() const
{
    if (_uid) return _uid;
    return static_cast<u32>(std::hash<std::string>{}(_fileName));
}

texture* lucus::create_texture_factory(const std::string& fileName)
{
    texture* tex = texture::create_factory();
    tex->setFileName(fileName);
    return tex;
}