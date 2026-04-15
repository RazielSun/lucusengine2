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

u32 texture::getHash() const
{
    const u32 fileHash = static_cast<u32>(std::hash<std::string>{}(_fileName));
    return fileHash;
}