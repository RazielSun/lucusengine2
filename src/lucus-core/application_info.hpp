#pragma once

#include "pch.hpp"

#include "singleton.hpp"

namespace lucus
{
    class application_info : public singleton<application_info>
    {
        public:
            const std::string& app_name() const { return m_app_name; }
            void set_app_name(const std::string& name) { m_app_name = name; }
            
            int width() const { return m_width; }
            int height() const { return m_height; }
            void set_window_size(int width, int height)
            {
                m_width = width;
                m_height = height;
            }
            
            bool resizable() const { return m_resizable; }
            void set_resizable(bool value) { m_resizable = value; }

            bool fullscreen() const { return m_fullscreen; }
            void set_fullscreen(bool value) { m_fullscreen = value; }

        private:
            std::string m_app_name = "App";
            int m_width = 1280;
            int m_height = 720;
            bool m_resizable = true;
            bool m_fullscreen = false;
    };

    void bind_application_info();
    void bind_application_info_object(application_info& info, const std::string& name);
}