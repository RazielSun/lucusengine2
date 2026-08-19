#pragma once

#include "pch.hpp"

#include "singleton.hpp"

namespace lucus
{
    class engine_info : public singleton<engine_info>
    {
        public:
            const std::string& name() const { return m_name; }
            
        private:
            std::string m_name = "Lucus Engine";
    };
}