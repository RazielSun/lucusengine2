#include "texture.hpp"

#include "texture_manager.hpp"


using namespace lucus;

texture* texture::create_factory(const std::string& fileName)
{
    texture* tex = new texture();
    tex->_fileName = fileName;
    texture_manager::instance().textureTrack(tex);
    return tex;
}

uint64_t texture::getHash() const
{
    const uint64_t fileHash = static_cast<uint64_t>(std::hash<std::string>{}(_fileName));
    return fileHash;
}