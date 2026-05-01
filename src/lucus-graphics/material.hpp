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
        intrusive_ptr<texture> texture;
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

        std::vector<texture_slot> _textures;

        // transient
        pipeline_state_handle _pso_handle;
    };
}