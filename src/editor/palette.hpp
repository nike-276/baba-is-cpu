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
    add(K::N_Baba,   false, "baba");
    add(K::N_Wall,   false, "wall");
    add(K::N_Flag,   false, "flag");
    add(K::N_Rock,   false, "rock");
    add(K::N_Water,  false, "water");
    add(K::N_Lava,   false, "lava");
    add(K::N_Skull,  false, "skull");
    add(K::N_Door,   false, "door");
    add(K::N_Key,    false, "key");
    add(K::N_Keke,   false, "keke");
    add(K::N_Fofo,   false, "fofo");
    add(K::N_Me,     false, "me");
    add(K::N_Box,    false, "box");
    add(K::N_Leaf,   false, "leaf");
    add(K::N_Cloud,  false, "cloud");
    add(K::N_Sun,    false, "sun");
    add(K::N_Moon,   false, "moon");
    add(K::N_Star,   false, "star");
    add(K::N_Planet, false, "planet");
    add(K::N_Bolt,   false, "bolt");
    add(K::N_Love,   false, "love");
    add(K::N_Bomb,   false, "bomb");
    add(K::N_Wind,   false, "wind");

    // Text: nouns
    add(K::N_Baba,   true, "T:baba");
    add(K::N_Wall,   true, "T:wall");
    add(K::N_Flag,   true, "T:flag");
    add(K::N_Rock,   true, "T:rock");
    add(K::N_Water,  true, "T:water");
    add(K::N_Lava,   true, "T:lava");
    add(K::N_Skull,  true, "T:skull");
    add(K::N_Door,   true, "T:door");
    add(K::N_Key,    true, "T:key");
    add(K::N_Keke,   true, "T:keke");
    add(K::N_Fofo,   true, "T:fofo");
    add(K::N_Me,     true, "T:me");
    add(K::N_Box,    true, "T:box");
    add(K::N_Leaf,   true, "T:leaf");
    add(K::N_Cloud,  true, "T:cloud");
    add(K::N_Sun,    true, "T:sun");
    add(K::N_Moon,   true, "T:moon");
    add(K::N_Star,   true, "T:star");
    add(K::N_Planet, true, "T:planet");
    add(K::N_Bolt,   true, "T:bolt");
    add(K::N_Love,   true, "T:love");
    add(K::N_Bomb,   true, "T:bomb");
    add(K::N_Wind,   true, "T:wind");

    // Text: operators
    add(K::O_Is,   true, "T:is");
    add(K::O_And,  true, "T:and");
    add(K::O_Not,  true, "T:not");
    add(K::O_On,   true, "T:on");
    add(K::O_Make, true, "T:make");
    add(K::O_Eat,  true, "T:eat");

    // Text: properties
    add(K::P_You,       true, "T:you");
    add(K::P_Push,      true, "T:push");
    add(K::P_Stop,      true, "T:stop");
    add(K::P_Win,       true, "T:win");
    add(K::P_Defeat,    true, "T:defeat");
    add(K::P_Sink,      true, "T:sink");
    add(K::P_Hot,       true, "T:hot");
    add(K::P_Melt,      true, "T:melt");
    add(K::P_Open,      true, "T:open");
    add(K::P_Shut,      true, "T:shut");
    add(K::P_Move,      true, "T:move");
    add(K::P_Weak,      true, "T:weak");
    add(K::P_Auto,      true, "T:auto");
    add(K::P_Fall,      true, "T:fall");
    add(K::P_Fallup,    true, "T:fallup");
    add(K::P_Fallleft,  true, "T:fallleft");
    add(K::P_Fallright, true, "T:fallright");
    add(K::P_Left,      true, "T:left");
    add(K::P_Right,     true, "T:right");
    add(K::P_Up,        true, "T:up");
    add(K::P_Down,      true, "T:down");

    return p;
}

}  // namespace baba::editor
