#pragma once

#include "pch.hpp"

#include "object.hpp"

namespace lucus
{
    class mesh : public object
    {
        public:
            static mesh* create_factory() { return new mesh(); }

        private:
            
    };
}