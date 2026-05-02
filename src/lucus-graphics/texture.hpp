#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"
#include "gltf_utils.hpp"

namespace lucus
{
    class texture : public object
    {
        friend class renderer;

    public:
        static texture* create_factory();
        static texture* create_one_factory(u8 r, u8 g, u8 b, u8 a);

        const std::string& getFileName() const { return _fileName; }
        void setFileName(const std::string& inFileName) { _fileName = inFileName; }

        bool IsInitialized() const { return bInitialized; }

        void initResource();
        void freeResource();
        const u8* getResource() const { return (_imageResource) ? _imageResource.get() : data.data(); }

        u32 getWidth() const { return imageWidth; }
        u32 getHeight() const { return imageHeight; }
        u32 getBytesPerPixel() const { return imageBytesPerPixel; }

        u32 getHash() const;

        /// How this texture is registered for sampling (classic descriptor vs bindless heap). Read by the RHI when creating GPU resources.
        void setResourceBindingMode(resource_binding_mode mode) { _resourceBindingMode = mode; }
        resource_binding_mode getResourceBindingMode() const { return _resourceBindingMode; }

        /// Valid after GPU create when binding mode was bindless.
        u32 getBindlessTextureSlot() const { return _bindlessTextureSlot; }
        void setBindlessTextureSlot(u32 slot) { _bindlessTextureSlot = slot; }

    protected:
        const texture_handle& getHandle() const { return _texture_handle; }
        void setHandle(const texture_handle& handle) { _texture_handle = handle; }

    private:
        std::string _fileName;
        u32 _uid {0};

        bool bInitialized{false};
        texture_format _imageFormat = texture_format::RGB_ALPHA;
        stbi_image_ptr _imageResource { nullptr };
        u32 imageWidth;
        u32 imageHeight;
        u32 imageChannels;
        u32 imageBytesPerPixel{4};

        // custom
        std::vector<u8> data;

        // transient
        texture_handle _texture_handle;
        resource_binding_mode _resourceBindingMode{resource_binding_mode::BINDFULL};
        u32 _bindlessTextureSlot{std::numeric_limits<u32>::max()};
    };

    texture* create_texture_factory(const std::string& fileName);
} // namespace lucus
