#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "texture.hpp"
#include "intrusive_ptr.hpp"
#include "render_types.hpp"

namespace lucus
{
    class material : public object
    {
        friend class renderer;

    public:
        static material* create_factory(const std::string& shaderName);

        void setShaderName(const std::string& shaderName) { _shaderName = shaderName; }
        const std::string& getShaderName() const { return _shaderName; }

        bool isUseUniformBuffers() const { return _useUniformBuffers; }
        void setUseUniformBuffers(bool useUniformBuffers) { _useUniformBuffers = useUniformBuffers; }

        bool isUseVertexIndexBuffers() const { return _useVertexIndexBuffers; }
        void setUseVertexIndexBuffers(bool useVertexIndexBuffers) { _useVertexIndexBuffers = useVertexIndexBuffers; }

        uint32_t getTexturesCount() const { return _useTexturesCount; }
        void setTexturesCount(uint32_t count) { _useTexturesCount = count; }
        void setTexture(texture* tex, uint32_t index = 0);
        const std::vector<intrusive_ptr<texture>>& getTextures() const { return _textures; }

        uint64_t getHash() const;

    protected:
        const material_handle& getHandle() const { return _material_handle; }
        void setHandle(const material_handle& handle) { _material_handle = handle; }

    private:
        std::string _shaderName;
        bool _useUniformBuffers{false};
        bool _useVertexIndexBuffers{false};
        uint32_t _useTexturesCount{0};

        std::vector<intrusive_ptr<texture>> _textures;

        // transient
        material_handle _material_handle;
    };
}