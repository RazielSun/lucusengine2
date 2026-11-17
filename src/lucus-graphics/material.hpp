#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "texture.hpp"
#include "intrusive_ptr.hpp"
#include "render_types.hpp"

namespace lucus
{
    class material;

    struct texture_slot
    {
        intrusive_ptr<texture> tex;
        u8 slot_index;
    };

    class material_instance : public object
    {
        friend class material;
        friend class renderer;

    public:
        material* getMaterial() const { return _material; }

        const glm::vec4& getBaseColor() const { return _baseColor; }
        void setBaseColor(float r, float g, float b, float a = 1.0f) { _baseColor = glm::vec4(r, g, b, a); }

        const glm::vec4& getMaterialParams() const { return _materialParams; }
        void setMaterialParams(float x, float y, float z, float w) { _materialParams = glm::vec4(x, y, z, w); }

        void addTexture(texture* tex, u8 slot_index = 0);
        const std::vector<texture_slot>& getTextures() const { return _textures; }
        u32 getTexturesCount() const { return _textures.size(); }

    protected:
        explicit material_instance(material* mat) : _material(mat) {}

        const uniform_buffer_handle& getHandle() const { return _handle; }
        void setHandle(const uniform_buffer_handle& handle) { _handle = handle; }

    private:
        material* _material;
        // empty for the root instance: material owns it, a strong ref back would be a cycle
        intrusive_ptr<material> _materialRef;

        glm::vec4 _baseColor{1.0f};
        glm::vec4 _materialParams{0.0f};

        std::vector<texture_slot> _textures;

        uniform_buffer_handle _handle;
    };

    class material : public object
    {
        friend class renderer;

    public:
        static material* create_factory(const std::string& shaderName);

        material_instance* createInstance();
        material_instance* getRootInstance() const { return _rootInstance.get(); }

        void setShaderName(const std::string& shaderName);
        const std::string& getShaderName(render_pass_name pass) const;

        bool useFrameUniformBuffer() const { return _useFrameUniformBuffer; }
        void setUseFrameUniformBuffer(bool useUniformBuffers) { _useFrameUniformBuffer = useUniformBuffers; }

        bool useObjectUniformBuffer() const { return _useObjectUniformBuffer; }
        void setUseObjectUniformBuffer(bool useUniformBuffers) { _useObjectUniformBuffer = useUniformBuffers; }

        bool useVertexIndexBuffers() const { return _useVertexIndexBuffers; }
        void setUseVertexIndexBuffers(bool useVertexIndexBuffers);

        void setDeferredMode(bool bInDeferred);

        pipeline_state_desc getPipelineStateDesc(render_pass_name pass) const;

    protected:
        material();

        const pipeline_state_handle& getPSOHandle(render_pass_name pass) const { return _psoHandles[static_cast<size_t>(pass)]; }
        void setPSOHandle(render_pass_name pass, const pipeline_state_handle& handle) { _psoHandles[static_cast<size_t>(pass)] = handle; }

    private:
        void resetPSOHandles() { _psoHandles = {}; }

        std::string _shaderName;
        bool bDeferred{true};
        std::string _shaderNameDeferred;

        bool _useFrameUniformBuffer{false};
        bool _useObjectUniformBuffer{false};
        bool _useVertexIndexBuffers{false};

        intrusive_ptr<material_instance> _rootInstance;

        std::array<pipeline_state_handle, render_pass_count> _psoHandles{};
    };
}
