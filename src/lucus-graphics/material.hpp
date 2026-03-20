#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"

namespace lucus
{
    class material : public object
    {
        public:
            static material* create_factory(const std::string& shaderName, int renderPass);

            void setShaderName(const std::string& shaderName) { _shaderName = shaderName; }
            const std::string& getShaderName() const { return _shaderName; }

            void setRenderPass(int renderPass) { _renderPass = renderPass; }
            int getRenderPass() const { return _renderPass; }

            const material_handle& getHandle() const { return _material_handle; }
            void setHandle(const material_handle& handle) { _material_handle = handle; }

        private:
            std::string _shaderName;
            int _renderPass{0};

            // transient
            material_handle _material_handle;
    };
}