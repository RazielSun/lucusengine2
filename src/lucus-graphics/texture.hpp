#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"

namespace lucus
{
    class texture : public object
    {
        friend class renderer;

    public:
        static texture* create_factory(const std::string& fileName);

        const std::string& getFileName() const { return _fileName; }

        u32 getHash() const;
    
    protected:
        const texture_handle& getHandle() const { return _texture_handle; }
        void setHandle(const texture_handle& handle) { _texture_handle = handle; }

    private:
        std::string _fileName;

        // transient
        texture_handle _texture_handle;
    };
} // namespace lucus
