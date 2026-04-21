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
        case K::N_Fofo:   return "FOFO";
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
        case K::N_Track:  return "TRCK";
        case K::N_Belt:    return "BELT";
        case K::N_Algae:   return "ALGA";   case K::N_Arm:     return "ARM";
        case K::N_Arrow:   return "ARRW";   case K::N_Badbad:  return "BADB";
        case K::N_Banana:  return "BANA";   case K::N_Bat:     return "BAT";
        case K::N_Bean:    return "BEAN";   case K::N_Bed:     return "BED";
        case K::N_Bee:     return "BEE";    case K::N_Bird:    return "BIRD";
        case K::N_Blob:    return "BLOB";   case K::N_Boat:    return "BOAT";
        case K::N_Boba:    return "BOBA";   case K::N_Bog:     return "BOG";
        case K::N_Bone:    return "BONE";   case K::N_Book:    return "BOOK";
        case K::N_Bottle:  return "BOTL";   case K::N_Brain:   return "BRAN";
        case K::N_Brick:   return "BRCK";   case K::N_Bubble:  return "BUBL";
        case K::N_Bucket:  return "BCKT";   case K::N_Bug:     return "BUG";
        case K::N_Bunny:   return "BUNY";   case K::N_Burger:  return "BRGR";
        case K::N_Cactus:  return "CACT";   case K::N_Cake:    return "CAKE";
        case K::N_Car:     return "CAR";    case K::N_Cart:    return "CART";
        case K::N_Cash:    return "CASH";   case K::N_Cat:     return "CAT";
        case K::N_Chair:   return "CHAR";   case K::N_Cheese:  return "CHSE";
        case K::N_Chili:   return "CHIL";   case K::N_Circle:  return "CIRC";
        case K::N_Cliff:   return "CLIF";   case K::N_Clock:   return "CLCK";
        case K::N_Cog:     return "COG";    case K::N_Crab:    return "CRAB";
        case K::N_Crystal: return "CRYS";   case K::N_Cup:     return "CUP";
        case K::N_Dog:     return "DOG";    case K::N_Donut:   return "DNUT";
        case K::N_Dot:     return "DOT";    case K::N_Drink:   return "DRNK";
        case K::N_Drum:    return "DRUM";   case K::N_Dust:    return "DUST";
        case K::N_Ear:     return "EAR";    case K::N_Egg:     return "EGG";
        case K::N_Eye:     return "EYE";    case K::N_Fence:   return "FNCE";
        case K::N_Fire:    return "FIRE";   case K::N_Fish:    return "FISH";
        case K::N_Flower:  return "FLWR";   case K::N_Foliage: return "FOLG";
        case K::N_Foot:    return "FOOT";   case K::N_Fort:    return "FORT";
        case K::N_Fox:     return "FOX";    case K::N_Frog:    return "FROG";
        case K::N_Fruit:   return "FRUT";   case K::N_Fungi:   return "FUNG";
        case K::N_Fungus:  return "FGUS";   case K::N_Gate:    return "GATE";
        case K::N_Gem:     return "GEM";    case K::N_Ghost:   return "GHST";
        case K::N_Grass:   return "GRAS";   case K::N_Guitar:  return "GTAR";
        case K::N_Hand:    return "HAND";   case K::N_Hedge:   return "HEDG";
        case K::N_Hihat:   return "HIHT";   case K::N_Hotdog:  return "HTDG";
        case K::N_House:   return "HOUS";   case K::N_Husk:    return "HUSK";
        case K::N_Husks:   return "HSKS";   case K::N_Ice:     return "ICE";
        case K::N_It:      return "IT";     case K::N_Jelly:   return "JELY";
        case K::N_Jiji:    return "JIJI";   case K::N_Knight:  return "KNGT";
        case K::N_Ladder:  return "LADR";   case K::N_Lamp:    return "LAMP";
        case K::N_Lever:   return "LEVR";   case K::N_Lift:    return "LIFT";
        case K::N_Lily:    return "LILY";   case K::N_Line:    return "LINE";
        case K::N_Lizard:  return "LIZD";   case K::N_Lock:    return "LOCK";
        case K::N_Mirror:  return "MIRR";   case K::N_Monitor: return "MNTR";
        case K::N_Monster: return "MNST";   case K::N_No:      return "NO";
        case K::N_Nose:    return "NOSE";   case K::N_Orb:     return "ORB";
        case K::N_Palm:    return "PALM";   case K::N_Pants:   return "PNTS";
        case K::N_Paper:   return "PAPR";   case K::N_Pawn:    return "PAWN";
        case K::N_Piano:   return "PIAN";   case K::N_Pillar:  return "PILR";
        case K::N_Pipe:    return "PIPE";   case K::N_Pixel:   return "PIXL";
        case K::N_Pizza:   return "PIZZ";   case K::N_Plane:   return "PLAN";
        case K::N_Plank:   return "PLNK";   case K::N_Potato:  return "PTAT";
        case K::N_Pumpkin: return "PUMP";   case K::N_Reed:    return "REED";
        case K::N_Ring:    return "RING";   case K::N_Road:    return "ROAD";
        case K::N_Robot:   return "ROBT";   case K::N_Rocket:  return "RCKT";
        case K::N_Rose:    return "ROSE";   case K::N_Rubble:  return "RBBL";
        case K::N_Sax:     return "SAX";    case K::N_Scissors:return "SCSR";
        case K::N_Seed:    return "SEED";   case K::N_Shell:   return "SHEL";
        case K::N_Shirt:   return "SHRT";   case K::N_Shovel:  return "SHVL";
        case K::N_Sign:    return "SIGN";   case K::N_Snail:   return "SNAL";
        case K::N_Spike:   return "SPKE";   case K::N_Sprout:  return "SPRT";
        case K::N_Square:  return "SQRE";   case K::N_Statue:  return "STAT";
        case K::N_Stick:   return "STCK";   case K::N_Stump:   return "STMP";
        case K::N_Sword:   return "SWRD";   case K::N_Table:   return "TABL";
        case K::N_Teeth:   return "TETH";   case K::N_TileObj: return "TILE";
        case K::N_Tower:   return "TOWR";   case K::N_Train:   return "TRAN";
        case K::N_Tree:    return "TREE";   case K::N_Trees:   return "TRES";
        case K::N_Triangle:return "TRI";    case K::N_Trumpet: return "TRMP";
        case K::N_Turnip:  return "TRNP";   case K::N_Turtle:  return "TRTL";
        case K::N_Ufo:     return "UFO";    case K::N_Vase:    return "VASE";
        case K::N_Vine:    return "VINE";   case K::N_What:    return "WHAT";
        case K::N_Worm:    return "WORM";   case K::N_Yes:     return "YES";
        case K::N_Text:    return "TEXT";
        default:           return "???";
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
            case K::N_Fofo:   return {{100,200,140,255},            BLACK, object_label(k)};
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
            case K::N_Track:  return {{90, 80, 70, 255},            WHITE, object_label(k)};
            case K::N_Belt:    return {{200,140, 40,255}, BLACK, object_label(k)};
            case K::N_Algae:   return {{ 80,170, 80,255}, BLACK, object_label(k)};
            case K::N_Arm:     return {{210,160,130,255}, BLACK, object_label(k)};
            case K::N_Arrow:   return {{200,200,200,255}, BLACK, object_label(k)};
            case K::N_Badbad:  return {{180, 60, 60,255}, WHITE, object_label(k)};
            case K::N_Banana:  return {{255,230, 50,255}, BLACK, object_label(k)};
            case K::N_Bat:     return {{ 80, 60,100,255}, WHITE, object_label(k)};
            case K::N_Bean:    return {{100,160, 80,255}, BLACK, object_label(k)};
            case K::N_Bed:     return {{160,120,200,255}, BLACK, object_label(k)};
            case K::N_Bee:     return {{240,200, 40,255}, BLACK, object_label(k)};
            case K::N_Bird:    return {{100,160,220,255}, BLACK, object_label(k)};
            case K::N_Blob:    return {{ 80,180, 80,255}, BLACK, object_label(k)};
            case K::N_Boat:    return {{140,100, 60,255}, WHITE, object_label(k)};
            case K::N_Boba:    return {{200,100,160,255}, WHITE, object_label(k)};
            case K::N_Bog:     return {{ 70, 90, 50,255}, WHITE, object_label(k)};
            case K::N_Bone:    return {{220,210,180,255}, BLACK, object_label(k)};
            case K::N_Book:    return {{160, 80, 40,255}, WHITE, object_label(k)};
            case K::N_Bottle:  return {{100,180,160,255}, BLACK, object_label(k)};
            case K::N_Brain:   return {{220,150,160,255}, BLACK, object_label(k)};
            case K::N_Brick:   return {{180, 80, 60,255}, WHITE, object_label(k)};
            case K::N_Bubble:  return {{160,220,240,255}, BLACK, object_label(k)};
            case K::N_Bucket:  return {{140,160,180,255}, BLACK, object_label(k)};
            case K::N_Bug:     return {{ 80,140, 60,255}, WHITE, object_label(k)};
            case K::N_Bunny:   return {{240,220,220,255}, BLACK, object_label(k)};
            case K::N_Burger:  return {{200,140, 60,255}, BLACK, object_label(k)};
            case K::N_Cactus:  return {{ 60,160, 80,255}, WHITE, object_label(k)};
            case K::N_Cake:    return {{240,180,180,255}, BLACK, object_label(k)};
            case K::N_Car:     return {{ 60,100,200,255}, WHITE, object_label(k)};
            case K::N_Cart:    return {{160,120, 80,255}, WHITE, object_label(k)};
            case K::N_Cash:    return {{ 80,200, 80,255}, BLACK, object_label(k)};
            case K::N_Cat:     return {{200,160,100,255}, BLACK, object_label(k)};
            case K::N_Chair:   return {{140,100, 60,255}, WHITE, object_label(k)};
            case K::N_Cheese:  return {{240,220, 60,255}, BLACK, object_label(k)};
            case K::N_Chili:   return {{220, 60, 60,255}, WHITE, object_label(k)};
            case K::N_Circle:  return {{180,180,220,255}, BLACK, object_label(k)};
            case K::N_Cliff:   return {{120,100, 80,255}, WHITE, object_label(k)};
            case K::N_Clock:   return {{200,200,160,255}, BLACK, object_label(k)};
            case K::N_Cog:     return {{160,160,160,255}, BLACK, object_label(k)};
            case K::N_Crab:    return {{220,100, 60,255}, WHITE, object_label(k)};
            case K::N_Crystal: return {{160,200,220,255}, BLACK, object_label(k)};
            case K::N_Cup:     return {{200,180,160,255}, BLACK, object_label(k)};
            case K::N_Dog:     return {{180,140,100,255}, BLACK, object_label(k)};
            case K::N_Donut:   return {{240,160,120,255}, BLACK, object_label(k)};
            case K::N_Dot:     return {{200,200,200,255}, BLACK, object_label(k)};
            case K::N_Drink:   return {{ 80,160,240,255}, WHITE, object_label(k)};
            case K::N_Drum:    return {{200,120, 80,255}, BLACK, object_label(k)};
            case K::N_Dust:    return {{200,190,160,255}, BLACK, object_label(k)};
            case K::N_Ear:     return {{220,160,140,255}, BLACK, object_label(k)};
            case K::N_Egg:     return {{240,230,210,255}, BLACK, object_label(k)};
            case K::N_Eye:     return {{ 60,180,220,255}, BLACK, object_label(k)};
            case K::N_Fence:   return {{180,140, 80,255}, BLACK, object_label(k)};
            case K::N_Fire:    return {{240,100, 20,255}, WHITE, object_label(k)};
            case K::N_Fish:    return {{100,180,220,255}, BLACK, object_label(k)};
            case K::N_Flower:  return {{240,100,160,255}, WHITE, object_label(k)};
            case K::N_Foliage: return {{ 80,160, 60,255}, WHITE, object_label(k)};
            case K::N_Foot:    return {{200,160,120,255}, BLACK, object_label(k)};
            case K::N_Fort:    return {{140,120,100,255}, WHITE, object_label(k)};
            case K::N_Fox:     return {{220,120, 40,255}, BLACK, object_label(k)};
            case K::N_Frog:    return {{ 60,180,100,255}, BLACK, object_label(k)};
            case K::N_Fruit:   return {{220, 80, 80,255}, WHITE, object_label(k)};
            case K::N_Fungi:   return {{200,140,100,255}, BLACK, object_label(k)};
            case K::N_Fungus:  return {{180,120, 80,255}, WHITE, object_label(k)};
            case K::N_Gate:    return {{120,100, 80,255}, WHITE, object_label(k)};
            case K::N_Gem:     return {{160, 80,220,255}, WHITE, object_label(k)};
            case K::N_Ghost:   return {{200,200,220,255}, BLACK, object_label(k)};
            case K::N_Grass:   return {{ 80,180, 60,255}, BLACK, object_label(k)};
            case K::N_Guitar:  return {{180,120, 60,255}, WHITE, object_label(k)};
            case K::N_Hand:    return {{220,170,130,255}, BLACK, object_label(k)};
            case K::N_Hedge:   return {{ 60,140, 60,255}, WHITE, object_label(k)};
            case K::N_Hihat:   return {{200,180, 80,255}, BLACK, object_label(k)};
            case K::N_Hotdog:  return {{220,100, 60,255}, WHITE, object_label(k)};
            case K::N_House:   return {{180,120, 80,255}, WHITE, object_label(k)};
            case K::N_Husk:    return {{180,160,100,255}, BLACK, object_label(k)};
            case K::N_Husks:   return {{160,140, 80,255}, BLACK, object_label(k)};
            case K::N_Ice:     return {{180,220,240,255}, BLACK, object_label(k)};
            case K::N_It:      return {{160, 60,200,255}, WHITE, object_label(k)};
            case K::N_Jelly:   return {{180,100,200,255}, WHITE, object_label(k)};
            case K::N_Jiji:    return {{ 40, 40, 40,255}, WHITE, object_label(k)};
            case K::N_Knight:  return {{180,180,200,255}, BLACK, object_label(k)};
            case K::N_Ladder:  return {{160,140,100,255}, BLACK, object_label(k)};
            case K::N_Lamp:    return {{240,220,120,255}, BLACK, object_label(k)};
            case K::N_Lever:   return {{140,140,140,255}, BLACK, object_label(k)};
            case K::N_Lift:    return {{160,140,120,255}, BLACK, object_label(k)};
            case K::N_Lily:    return {{240,200,220,255}, BLACK, object_label(k)};
            case K::N_Line:    return {{180,180,180,255}, BLACK, object_label(k)};
            case K::N_Lizard:  return {{ 80,160, 80,255}, BLACK, object_label(k)};
            case K::N_Lock:    return {{160,120, 80,255}, WHITE, object_label(k)};
            case K::N_Mirror:  return {{200,220,240,255}, BLACK, object_label(k)};
            case K::N_Monitor: return {{ 60, 80,100,255}, WHITE, object_label(k)};
            case K::N_Monster: return {{100, 60, 60,255}, WHITE, object_label(k)};
            case K::N_No:      return {{220, 60, 60,255}, WHITE, object_label(k)};
            case K::N_Nose:    return {{220,160,140,255}, BLACK, object_label(k)};
            case K::N_Orb:     return {{100,200,200,255}, BLACK, object_label(k)};
            case K::N_Palm:    return {{ 80,180, 60,255}, BLACK, object_label(k)};
            case K::N_Pants:   return {{ 80,100,180,255}, WHITE, object_label(k)};
            case K::N_Paper:   return {{240,240,220,255}, BLACK, object_label(k)};
            case K::N_Pawn:    return {{200,200,200,255}, BLACK, object_label(k)};
            case K::N_Piano:   return {{ 30, 30, 30,255}, WHITE, object_label(k)};
            case K::N_Pillar:  return {{160,150,130,255}, BLACK, object_label(k)};
            case K::N_Pipe:    return {{120,130,140,255}, WHITE, object_label(k)};
            case K::N_Pixel:   return {{200,100,200,255}, WHITE, object_label(k)};
            case K::N_Pizza:   return {{220,160, 80,255}, BLACK, object_label(k)};
            case K::N_Plane:   return {{180,200,220,255}, BLACK, object_label(k)};
            case K::N_Plank:   return {{160,120, 80,255}, WHITE, object_label(k)};
            case K::N_Potato:  return {{200,170,100,255}, BLACK, object_label(k)};
            case K::N_Pumpkin: return {{220,120, 40,255}, BLACK, object_label(k)};
            case K::N_Reed:    return {{140,180,100,255}, BLACK, object_label(k)};
            case K::N_Ring:    return {{220,190, 60,255}, BLACK, object_label(k)};
            case K::N_Road:    return {{120,120,120,255}, WHITE, object_label(k)};
            case K::N_Robot:   return {{140,160,180,255}, BLACK, object_label(k)};
            case K::N_Rocket:  return {{200,200,200,255}, BLACK, object_label(k)};
            case K::N_Rose:    return {{220, 60, 80,255}, WHITE, object_label(k)};
            case K::N_Rubble:  return {{140,130,120,255}, WHITE, object_label(k)};
            case K::N_Sax:     return {{200,160, 60,255}, BLACK, object_label(k)};
            case K::N_Scissors:return {{180,180,200,255}, BLACK, object_label(k)};
            case K::N_Seed:    return {{160,130, 80,255}, WHITE, object_label(k)};
            case K::N_Shell:   return {{220,190,160,255}, BLACK, object_label(k)};
            case K::N_Shirt:   return {{ 80,140,220,255}, WHITE, object_label(k)};
            case K::N_Shovel:  return {{160,140,100,255}, BLACK, object_label(k)};
            case K::N_Sign:    return {{200,180,120,255}, BLACK, object_label(k)};
            case K::N_Snail:   return {{160,140,100,255}, WHITE, object_label(k)};
            case K::N_Spike:   return {{160,160,160,255}, BLACK, object_label(k)};
            case K::N_Sprout:  return {{100,200, 80,255}, BLACK, object_label(k)};
            case K::N_Square:  return {{160,160,200,255}, BLACK, object_label(k)};
            case K::N_Statue:  return {{180,170,160,255}, BLACK, object_label(k)};
            case K::N_Stick:   return {{160,130, 90,255}, BLACK, object_label(k)};
            case K::N_Stump:   return {{140,100, 60,255}, WHITE, object_label(k)};
            case K::N_Sword:   return {{180,200,220,255}, BLACK, object_label(k)};
            case K::N_Table:   return {{160,120, 80,255}, WHITE, object_label(k)};
            case K::N_Teeth:   return {{240,240,200,255}, BLACK, object_label(k)};
            case K::N_TileObj: return {{180,180,180,255}, BLACK, object_label(k)};
            case K::N_Tower:   return {{140,130,120,255}, WHITE, object_label(k)};
            case K::N_Train:   return {{ 80, 80,100,255}, WHITE, object_label(k)};
            case K::N_Tree:    return {{ 60,140, 60,255}, WHITE, object_label(k)};
            case K::N_Trees:   return {{ 60,120, 60,255}, WHITE, object_label(k)};
            case K::N_Triangle:return {{200,180,220,255}, BLACK, object_label(k)};
            case K::N_Trumpet: return {{220,180, 60,255}, BLACK, object_label(k)};
            case K::N_Turnip:  return {{200,140,180,255}, BLACK, object_label(k)};
            case K::N_Turtle:  return {{ 60,140, 80,255}, WHITE, object_label(k)};
            case K::N_Ufo:     return {{140,220,200,255}, BLACK, object_label(k)};
            case K::N_Vase:    return {{100,160,200,255}, BLACK, object_label(k)};
            case K::N_Vine:    return {{ 80,160, 60,255}, WHITE, object_label(k)};
            case K::N_What:    return {{200,200,200,255}, BLACK, object_label(k)};
            case K::N_Worm:    return {{200,140,120,255}, BLACK, object_label(k)};
            case K::N_Yes:     return {{ 80,220, 80,255}, BLACK, object_label(k)};
            default:           return {{ 80, 80, 80,255}, WHITE, "???"};
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
