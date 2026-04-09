#pragma once

#include "pch.hpp"

#include "singleton.hpp"

#include "texture.hpp"
#include "intrusive_ptr.hpp"

namespace lucus
{
    class texture_manager : public singleton<texture_manager>
    {
    public:
        void textureTrack(texture* tex);
        void textureLoaded(texture* tex);

        bool hasPendingTextures() const { return !_pending.empty(); }
        std::vector<intrusive_ptr<texture>>& getPendingTextures() { return _pending; }
        void resetPendingTextures() { _pending.clear(); }

    protected:
        //

    private:
        std::vector<intrusive_ptr<texture>> _pending;
        std::vector<intrusive_ptr<texture>> _textures;
    };
}