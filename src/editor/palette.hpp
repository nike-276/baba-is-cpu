#pragma once

#include "core/direction.hpp"
#include "core/kind.hpp"

#include <string>
#include <vector>

namespace baba::editor {

struct PaletteEntry {
    core::Kind      kind;
    bool            is_text;
    core::Direction default_facing{core::Direction::Right};
    std::string     display_name;
};

inline std::vector<PaletteEntry> default_palette() {
    using K = core::Kind;
    using D = core::Direction;

    std::vector<PaletteEntry> p;
    auto add = [&](K k, bool text, const char* name) {
        p.push_back({k, text, D::Right, name});
    };

    // Objects
    add(K::N_Baba,  false, "baba");
    add(K::N_Wall,  false, "wall");
    add(K::N_Flag,  false, "flag");
    add(K::N_Rock,  false, "rock");
    add(K::N_Water, false, "water");
    add(K::N_Lava,  false, "lava");
    add(K::N_Skull, false, "skull");
    add(K::N_Door,  false, "door");
    add(K::N_Key,   false, "key");

    // Text: nouns
    add(K::N_Baba,  true, "T:baba");
    add(K::N_Wall,  true, "T:wall");
    add(K::N_Flag,  true, "T:flag");
    add(K::N_Rock,  true, "T:rock");
    add(K::N_Water, true, "T:water");
    add(K::N_Lava,  true, "T:lava");
    add(K::N_Skull, true, "T:skull");
    add(K::N_Door,  true, "T:door");
    add(K::N_Key,   true, "T:key");

    // Text: operators
    add(K::O_Is,  true, "T:is");
    add(K::O_And, true, "T:and");
    add(K::O_Not, true, "T:not");

    // Text: properties
    add(K::P_You,    true, "T:you");
    add(K::P_Push,   true, "T:push");
    add(K::P_Stop,   true, "T:stop");
    add(K::P_Win,    true, "T:win");
    add(K::P_Defeat, true, "T:defeat");
    add(K::P_Sink,   true, "T:sink");
    add(K::P_Hot,    true, "T:hot");
    add(K::P_Melt,   true, "T:melt");
    add(K::P_Open,   true, "T:open");
    add(K::P_Shut,   true, "T:shut");
    add(K::P_Move,   true, "T:move");
    add(K::P_Eat,    true, "T:eat");
    add(K::P_Weak,   true, "T:weak");

    return p;
}

}  // namespace baba::editor
