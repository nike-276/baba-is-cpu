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
    N_Text,   // the abstract noun "TEXT" (matches every text object)

    // ---- Operators (text-only) ----
    O_Is,
    O_And,
    O_Not,

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
    P_Eat,
    P_Weak,

    Count_,
};

constexpr bool is_noun(Kind k) {
    return k >= Kind::N_Baba && k <= Kind::N_Text;
}
constexpr bool is_operator(Kind k) {
    return k >= Kind::O_Is && k <= Kind::O_Not;
}
constexpr bool is_property(Kind k) {
    return k >= Kind::P_You && k <= Kind::P_Weak;
}

// Lowercase canonical name as it appears in .level / .test files.
std::string_view kind_name(Kind k);

// Lookup by lowercase name. Returns std::nullopt for unknown names.
std::optional<Kind> kind_from_name(std::string_view name);

}  // namespace baba::core
