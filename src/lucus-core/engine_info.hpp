#pragma once

#include "pch.hpp"

#include "singleton.hpp"

namespace lucus
{
    class engine_info : public singleton<engine_info>
    {
        public:
            const std::string& name() const { return m_name; }
            // void set_name(const std::string& name) { m_name = name; }
            
        private:
            std::string m_name = "Lucus Engine";
    };

    void bind_engine_info();
    void bind_engine_info_object(engine_info& info, const std::string& name);
}