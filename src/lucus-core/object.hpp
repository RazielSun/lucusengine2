#pragma once

#include "pch.hpp"

#include "intrusive_ptr.hpp"

namespace lucus {

    class object {
    public:
        void addRef();
        void releaseRef();

    protected:
        object() = default;
        object(const object&) {}
        object(object&&) {}
        object& operator=(const object&) { return *this; }
        object& operator=(object&&) { return *this; }
        virtual ~object();

    private:
        mutable std::atomic<uint32_t> _refCount{1};
    };

}