#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "texture.hpp"
#include "intrusive_ptr.hpp"
#include "render_types.hpp"

namespace lucus
{
    struct texture_slot
    {
        intrusive_ptr<texture> tex;
        u8 slot_index;
    };

    class material : public object
    {
        friend class renderer;

    public:
        static material* create_factory(const std::string& shaderName);

        void setShaderName(const std::string& shaderName);
        const std::string& getShaderName() const;

        bool useFrameUniformBuffer() const { return _useFrameUniformBuffer; }
        void setUseFrameUniformBuffer(bool useUniformBuffers) { _useFrameUniformBuffer = useUniformBuffers; }

        bool useObjectUniformBuffer() const { return _useObjectUniformBuffer; }
        void setUseObjectUniformBuffer(bool useUniformBuffers) { _useObjectUniformBuffer = useUniformBuffers; }

        bool useVertexIndexBuffers() const { return _useVertexIndexBuffers; }
        void setUseVertexIndexBuffers(bool useVertexIndexBuffers) { _useVertexIndexBuffers = useVertexIndexBuffers; }

        const glm::vec4& getBaseColor() const { return _baseColor; }
        void setBaseColor(float r, float g, float b, float a = 1.0f) { _baseColor = glm::vec4(r, g, b, a); }

        const glm::vec4& getMaterialParams() const { return _materialParams; }
        void setMaterialParams(float x, float y, float z, float w) { _materialParams = glm::vec4(x, y, z, w); }

        void addTexture(texture* tex, u8 slot_index = 0);
        const std::vector<texture_slot>& getTextures() const { return _textures; }
        u32 getTexturesCount() const { return _textures.size(); }

        void setDeferredMode(bool bInDeferred);

        u32 getHash() const;

    protected:
        const pipeline_state_handle& getPSOHandle() const { return _pso_handle; }
        void setPSOHandle(const pipeline_state_handle& handle) { _pso_handle = handle; }

    private:
        std::string _shaderName;
        bool bDeferred{true};
        std::string _shaderNameDeferred;

        bool _useFrameUniformBuffer{false};
        bool _useObjectUniformBuffer{false};
        bool _useVertexIndexBuffers{false};

        glm::vec4 _baseColor{1.0f};
        glm::vec4 _materialParams{0.0f};

        std::vector<texture_slot> _textures;

        pipeline_state_handle _pso_handle;
    };
}
