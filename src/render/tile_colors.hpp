#pragma once

#include "core/kind.hpp"
#include <raylib.h>

namespace baba::render {

struct TileStyle {
    Color bg;
    Color text_fg;
    const char* label;  // short label, 4–6 chars
};

inline TileStyle style_for(core::Kind k, bool is_text) {
    using K = core::Kind;
    // Non-text (object) styles
    if (!is_text) {
        switch (k) {
            case K::N_Baba:  return {PINK,      BLACK, "BABA"};
            case K::N_Wall:  return {GRAY,      BLACK, "WALL"};
            case K::N_Flag:  return {YELLOW,    BLACK, "FLAG"};
            case K::N_Rock:  return {BROWN,     WHITE, "ROCK"};
            case K::N_Water: return {BLUE,      WHITE, "WATR"};
            case K::N_Lava:  return {ORANGE,    BLACK, "LAVA"};
            case K::N_Skull: return {DARKGRAY,  WHITE, "SKUL"};
            case K::N_Door:  return {DARKBROWN, WHITE, "DOOR"};
            case K::N_Key:   return {GOLD,      BLACK, "KEY"};
            default:         return {DARKGRAY,  WHITE, "???"};
        }
    }
    // Text (word tile) styles — white background with a small tint per role
    if (core::is_noun(k))     return {WHITE,    BLACK, core::kind_name(k).data()};
    if (core::is_operator(k)) return {SKYBLUE,  BLACK, core::kind_name(k).data()};
    if (core::is_property(k)) return {LIME,     BLACK, core::kind_name(k).data()};
    return {LIGHTGRAY, BLACK, "???"};
}

}  // namespace baba::render
