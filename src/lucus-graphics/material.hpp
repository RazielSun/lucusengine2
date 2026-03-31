#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"

namespace lucus
{
    class material : public object
    {
        public:
            static material* create_factory(const std::string& shaderName);

            const material_handle& getHandle() const { return _material_handle; }
            void setHandle(const material_handle& handle) { _material_handle = handle; }

            void setShaderName(const std::string& shaderName) { _shaderName = shaderName; }
            const std::string& getShaderName() const { return _shaderName; }

            bool isUseUniformBuffers() const { return _useUniformBuffers; }
            void setUseUniformBuffers(bool useUniformBuffers) { _useUniformBuffers = useUniformBuffers; }

            uint32_t getHash() const;

        private:
            std::string _shaderName;
            bool _useUniformBuffers{false};

            // transient
            material_handle _material_handle;
    };
}