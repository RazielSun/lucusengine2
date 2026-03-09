#pragma once

#include "pch.hpp"

namespace lucus
{
    template<class T>
    struct angelscript_release
    {
        void operator()(T* p) const
        {
            if (p) p->Release();
        }
    };

    template<class T>
    using angelscript_ptr = std::unique_ptr<T, angelscript_release<T>>;
}