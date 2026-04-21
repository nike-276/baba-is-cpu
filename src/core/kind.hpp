// core/kind.hpp — single enum covering nouns, operators, and properties.
//
// A non-text object's `kind` must be a noun. A text object's `kind` may be
// any noun, operator, or property; the object itself carries `text=true`
// so the renderer / rule engine know to treat it as a word.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace baba::core {

enum class Kind : std::uint16_t {
    None = 0,

    // ---- Nouns (non-text objects use these) ----
    N_Baba,
    N_Wall,
    N_Flag,
    N_Rock,
    N_Water,
    N_Lava,
    N_Skull,
    N_Door,
    N_Key,
    N_Keke,
    N_Fofo,
    N_Me,
    N_Box,
    N_Leaf,
    N_Cloud,
    N_Sun,
    N_Moon,
    N_Star,
    N_Planet,
    N_Bolt,
    N_Love,
    N_Bomb,
    N_Wind,
    N_Track,
    N_Belt,
    // --- Extended noun catalog (from wiki: Category:Nouns) ---
    N_Algae,   N_Arm,      N_Arrow,   N_Badbad,  N_Banana,
    N_Bat,     N_Bean,     N_Bed,     N_Bee,     N_Bird,
    N_Blob,    N_Boat,     N_Boba,    N_Bog,     N_Bone,
    N_Book,    N_Bottle,   N_Brain,   N_Brick,   N_Bubble,
    N_Bucket,  N_Bug,      N_Bunny,   N_Burger,  N_Cactus,
    N_Cake,    N_Car,      N_Cart,    N_Cash,    N_Cat,
    N_Chair,   N_Cheese,   N_Chili,   N_Circle,  N_Cliff,
    N_Clock,   N_Cog,      N_Crab,    N_Crystal, N_Cup,
    N_Dog,     N_Donut,    N_Dot,     N_Drink,   N_Drum,
    N_Dust,    N_Ear,      N_Egg,     N_Eye,     N_Fence,
    N_Fire,    N_Fish,     N_Flower,  N_Foliage, N_Foot,
    N_Fort,    N_Fox,      N_Frog,    N_Fruit,   N_Fungi,
    N_Fungus,  N_Gate,     N_Gem,     N_Ghost,   N_Grass,
    N_Guitar,  N_Hand,     N_Hedge,   N_Hihat,   N_Hotdog,
    N_House,   N_Husk,     N_Husks,   N_Ice,     N_It,
    N_Jelly,   N_Jiji,     N_Knight,  N_Ladder,  N_Lamp,
    N_Lever,   N_Lift,     N_Lily,    N_Line,    N_Lizard,
    N_Lock,    N_Mirror,   N_Monitor, N_Monster, N_No,
    N_Nose,    N_Orb,      N_Palm,    N_Pants,   N_Paper,
    N_Pawn,    N_Piano,    N_Pillar,  N_Pipe,    N_Pixel,
    N_Pizza,   N_Plane,    N_Plank,   N_Potato,  N_Pumpkin,
    N_Reed,    N_Ring,     N_Road,    N_Robot,   N_Rocket,
    N_Rose,    N_Rubble,   N_Sax,     N_Scissors,N_Seed,
    N_Shell,   N_Shirt,    N_Shovel,  N_Sign,    N_Snail,
    N_Spike,   N_Sprout,   N_Square,  N_Statue,  N_Stick,
    N_Stump,   N_Sword,    N_Table,   N_Teeth,   N_TileObj,
    N_Tower,   N_Train,    N_Tree,    N_Trees,   N_Triangle,
    N_Trumpet, N_Turnip,   N_Turtle,  N_Ufo,     N_Vase,
    N_Vine,    N_What,     N_Worm,    N_Yes,
    N_Text,   // the abstract noun "TEXT" (matches every text object)

    // ---- Operators (text-only) ----
    O_Is,
    O_And,
    O_Not,
    O_On,
    O_Make,
    O_Eat,     // NOUN EAT NOUN: subject destroys target on contact
    O_Facing,  // NOUN [NOT] FACING <cond> IS PROPERTY: condition on facing tile or own direction
    O_Has,     // NOUN HAS NOUN [AND NOUN]*: spawn target when subject is destroyed
    O_Powered,  // prefix condition: [NOT] POWERED NOUN IS PROPERTY
    O_Powered2, // prefix condition: [NOT] POWERED2 NOUN IS PROPERTY (channel 2)
    O_Powered3, // prefix condition: [NOT] POWERED3 NOUN IS PROPERTY (channel 3)
    O_Follow,  // NOUN FOLLOW NOUN [AND NOUN]*: move toward nearest target each tick
    O_Fear,    // NOUN FEAR NOUN [AND NOUN]*: move away from adjacent target each tick
    O_Play,    // NOUN PLAY NOTE [OCTAVE] [ACCIDENTAL]: emit note each tick

    // ---- PLAY parameter tokens (text-only; not operators, not properties) ----
    // Letters A-Z: general text tiles; A-G also serve as PLAY note tokens.
    O_LetterA, O_LetterB, O_LetterC, O_LetterD, O_LetterE, O_LetterF, O_LetterG,
    O_LetterH, O_LetterI, O_LetterJ, O_LetterK, O_LetterL, O_LetterM, O_LetterN,
    O_LetterO, O_LetterP, O_LetterQ, O_LetterR, O_LetterS, O_LetterT, O_LetterU,
    O_LetterV, O_LetterW, O_LetterX, O_LetterY, O_LetterZ,
    O_Sharp,   // SHARP accidental modifier for PLAY
    O_Flat,    // FLAT accidental modifier for PLAY
    O_Num0, O_Num1, O_Num2, O_Num3, O_Num4,  // octave/number tokens for PLAY
    O_Num5, O_Num6, O_Num7, O_Num8, O_Num9,

    // ---- Properties (text-only) ----
    P_You,
    P_Push,
    P_Stop,
    P_Win,
    P_Defeat,
    P_Sink,
    P_Hot,
    P_Melt,
    P_Open,
    P_Shut,
    P_Move,
    P_Weak,
    P_Auto,
    P_Fall,
    P_Fallup,
    P_Fallleft,
    P_Fallright,
    P_Left,
    P_Right,
    P_Up,
    P_Down,
    P_Still,
    P_Shift,
    P_Swap,
    P_Revert,
    P_Nudgeright, // move right each tick without changing facing
    P_Nudgeup,    // move up each tick without changing facing
    P_Nudgeleft,  // move left each tick without changing facing
    P_Nudgedown,  // move down each tick without changing facing
    P_Power,   // makes the global POWERED condition true
    P_Power2,  // makes the global POWERED2 condition true (channel 2)
    P_Power3,  // makes the global POWERED3 condition true (channel 3)
    P_Word,    // object acts as its own text tile in rule formation

    Count_,
};

constexpr bool is_noun(Kind k) {
    return k >= Kind::N_Baba && k <= Kind::N_Text;
}
constexpr bool is_operator(Kind k) {
    return k >= Kind::O_Is && k <= Kind::O_Play;
}
constexpr bool is_property(Kind k) {
    return k >= Kind::P_You && k <= Kind::P_Word;
}
// PLAY parameter token predicates.
constexpr bool is_note_token(Kind k) {
    return k >= Kind::O_LetterA && k <= Kind::O_LetterG;
}
constexpr bool is_accidental_token(Kind k) {
    return k == Kind::O_Sharp || k == Kind::O_Flat;
}
constexpr bool is_num_token(Kind k) {
    return k >= Kind::O_Num0 && k <= Kind::O_Num9;
}
constexpr char note_to_char(Kind k) {
    return static_cast<char>('A' + (static_cast<int>(k) - static_cast<int>(Kind::O_LetterA)));
}
constexpr int num_to_int(Kind k) {
    return static_cast<int>(k) - static_cast<int>(Kind::O_Num0);
}

// Lowercase canonical name as it appears in .level / .test files.
std::string_view kind_name(Kind k);

// Lookup by lowercase name. Returns std::nullopt for unknown names.
std::optional<Kind> kind_from_name(std::string_view name);

}  // namespace baba::core
