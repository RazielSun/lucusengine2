#pragma once

#include "pch.hpp"

#include "core_types.hpp"
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
        mutable std::atomic<u32> _refCount{1};
    };

}