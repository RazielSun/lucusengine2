#pragma once

#include "pch.hpp"

namespace lucus
{
    template<class T>
    class intrusive_ptr
    {
    public:
        intrusive_ptr() = default;

        explicit intrusive_ptr(T* ptr)
            : m_ptr(ptr)
        {
            if (m_ptr) m_ptr->addRef();
        }

        intrusive_ptr(const intrusive_ptr& other)
            : m_ptr(other.m_ptr)
        {
            if (m_ptr) m_ptr->addRef();
        }

        intrusive_ptr(intrusive_ptr&& other) noexcept
            : m_ptr(other.m_ptr)
        {
            other.m_ptr = nullptr;
        }

        ~intrusive_ptr()
        {
            if (m_ptr) m_ptr->releaseRef();
        }

        intrusive_ptr& operator=(const intrusive_ptr& other)
        {
            if (this == &other) return *this;

            if (other.m_ptr) other.m_ptr->addRef();
            if (m_ptr) m_ptr->releaseRef();

            m_ptr = other.m_ptr;
            return *this;
        }

        intrusive_ptr& operator=(intrusive_ptr&& other) noexcept
        {
            if (this == &other) return *this;

            if (m_ptr) m_ptr->releaseRef();

            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            return *this;
        }

        T* get() const { return m_ptr; }
        T* operator->() const { return m_ptr; }
        explicit operator bool() const { return m_ptr != nullptr; }

        void reset(T* ptr = nullptr)
        {
            if (ptr) ptr->addRef();
            if (m_ptr) m_ptr->releaseRef();
            m_ptr = ptr;
        }

    private:
        T* m_ptr = nullptr;
    };
}