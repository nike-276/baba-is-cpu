#pragma once

#include "core/kind.hpp"

#include <raylib.h>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace baba::render {

// Loads PNG sprite images from assets/sprites/objects/ and assets/sprites/text/.
// Filename convention: {kind_name}.png  (lowercase, e.g. "baba.png").
// Falls back gracefully — missing textures return nullptr.
class SpriteAtlas {
public:
    SpriteAtlas() = default;

    ~SpriteAtlas() {
        for (auto& [k, t] : obj_) UnloadTexture(t);
        for (auto& [k, t] : txt_) UnloadTexture(t);
    }

    // Load all sprites found in the given base directory.
    // base_dir/objects/*.png → object sprites
    // base_dir/text/*.png    → text-tile sprites
    void load(std::filesystem::path const& base_dir) {
        load_dir(base_dir / "objects", obj_);
        load_dir(base_dir / "text",    txt_);
    }

    // Returns pointer to texture, or nullptr if not found.
    Texture2D const* get(core::Kind k, bool is_text) const {
        auto const& map = is_text ? txt_ : obj_;
        auto it = map.find(static_cast<uint16_t>(k));
        return (it != map.end()) ? &it->second : nullptr;
    }

    bool empty() const { return obj_.empty() && txt_.empty(); }

private:
    using Map = std::unordered_map<uint16_t, Texture2D>;
    Map obj_;
    Map txt_;

    void load_dir(std::filesystem::path const& dir, Map& map) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) return;
        for (auto const& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.path().extension() != ".png") continue;
            std::string stem = entry.path().stem().string();
            auto k = core::kind_from_name(stem);
            if (!k) continue;
            Texture2D tex = LoadTexture(entry.path().string().c_str());
            if (tex.id == 0) continue;
            SetTextureFilter(tex, TEXTURE_FILTER_POINT);
            map[static_cast<uint16_t>(*k)] = tex;
        }
    }
};

}  // namespace baba::render
