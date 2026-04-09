#pragma once

#include "pch.hpp"

#include "object.hpp"
#include "render_types.hpp"

namespace lucus
{
    class texture : public object
    {
    public:
        static texture* create_factory(const std::string& fileName);


        const std::string& getFileName() const { return _fileName; }
        
        const texture_handle& getHandle() const { return _texture_handle; }
        void setHandle(const texture_handle& handle) { _texture_handle = handle; }

        uint64_t getHash() const;

    private:
        std::string _fileName;

        // transient
        texture_handle _texture_handle;
    };
} // namespace lucus
