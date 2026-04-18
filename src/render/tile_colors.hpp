#pragma once

#include "core/kind.hpp"
#include <raylib.h>

namespace baba::render {

struct TileStyle {
    Color bg;
    Color text_fg;
    const char* label;  // short label, 4–6 chars
};

// Short uppercase labels for object tiles.
inline const char* object_label(core::Kind k) {
    using K = core::Kind;
    switch (k) {
        case K::N_Baba:  return "BABA";
        case K::N_Wall:  return "WALL";
        case K::N_Flag:  return "FLAG";
        case K::N_Rock:  return "ROCK";
        case K::N_Water: return "WATR";
        case K::N_Lava:  return "LAVA";
        case K::N_Skull: return "SKUL";
        case K::N_Door:  return "DOOR";
        case K::N_Key:    return "KEY";
        case K::N_Keke:   return "KEKE";
        case K::N_Me:     return "ME";
        case K::N_Box:    return "BOX";
        case K::N_Leaf:   return "LEAF";
        case K::N_Cloud:  return "CLUD";
        case K::N_Sun:    return "SUN";
        case K::N_Moon:   return "MOON";
        case K::N_Star:   return "STAR";
        case K::N_Planet: return "PLNT";
        case K::N_Bolt:   return "BOLT";
        case K::N_Love:   return "LOVE";
        case K::N_Bomb:   return "BOMB";
        case K::N_Wind:   return "WIND";
        default:          return "???";
    }
}

inline TileStyle style_for(core::Kind k, bool is_text) {
    using K = core::Kind;

    // ── Object tiles: solid vivid color, short ALL-CAPS label ────────────
    if (!is_text) {
        switch (k) {
            case K::N_Baba:  return {PINK,                        BLACK, object_label(k)};
            case K::N_Wall:  return {{120,120,120,255},            BLACK, object_label(k)};
            case K::N_Flag:  return {YELLOW,                      BLACK, object_label(k)};
            case K::N_Rock:  return {{139,90,43,255},              WHITE, object_label(k)};
            case K::N_Water: return {{30,100,220,255},             WHITE, object_label(k)};
            case K::N_Lava:  return {ORANGE,                      BLACK, object_label(k)};
            case K::N_Skull: return {{60,60,60,255},               WHITE, object_label(k)};
            case K::N_Door:  return {{100,60,20,255},              WHITE, object_label(k)};
            case K::N_Key:    return {GOLD,                         BLACK, object_label(k)};
            case K::N_Keke:   return {{255,100,60,255},              BLACK, object_label(k)};
            case K::N_Me:     return {{180,140,255,255},            BLACK, object_label(k)};
            case K::N_Box:    return {{160,110,60,255},             WHITE, object_label(k)};
            case K::N_Leaf:   return {{80,180,60,255},              BLACK, object_label(k)};
            case K::N_Cloud:  return {{200,220,255,255},            BLACK, object_label(k)};
            case K::N_Sun:    return {{255,220,0,255},              BLACK, object_label(k)};
            case K::N_Moon:   return {{200,200,140,255},            BLACK, object_label(k)};
            case K::N_Star:   return {{255,240,80,255},             BLACK, object_label(k)};
            case K::N_Planet: return {{80,160,200,255},             BLACK, object_label(k)};
            case K::N_Bolt:   return {{255,200,0,255},              BLACK, object_label(k)};
            case K::N_Love:   return {{255,80,120,255},             WHITE, object_label(k)};
            case K::N_Bomb:   return {{40,40,40,255},               WHITE, object_label(k)};
            case K::N_Wind:   return {{160,220,240,255},            BLACK, object_label(k)};
            default:          return {{80,80,80,255},               WHITE, "???"};
        }
    }

    // ── Text (word) tiles: cream background, lowercase name, thick border ─
    // The border is drawn by renderer.cpp; here we just set colors + label.
    // Nouns:       cream bg, dark blue text
    // Operators:   light blue bg, dark text  (IS, AND, NOT)
    // Properties:  light green bg, dark text  (YOU, WIN, PUSH, …)
    if (core::is_noun(k))
        return {{240,230,200,255}, {20,20,80,255},  core::kind_name(k).data()};
    if (core::is_operator(k))
        return {{180,220,255,255}, {10,30,80,255},  core::kind_name(k).data()};
    if (core::is_property(k))
        return {{190,255,190,255}, {10,70,10,255},  core::kind_name(k).data()};
    return {LIGHTGRAY, BLACK, "???"};
}

}  // namespace baba::render
