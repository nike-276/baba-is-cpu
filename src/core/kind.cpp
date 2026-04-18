#include "kind.hpp"

#include <array>
#include <utility>

namespace baba::core {

namespace {

struct Entry { std::string_view name; Kind kind; };

constexpr std::array kTable = {
    Entry{"baba",   Kind::N_Baba},
    Entry{"wall",   Kind::N_Wall},
    Entry{"flag",   Kind::N_Flag},
    Entry{"rock",   Kind::N_Rock},
    Entry{"water",  Kind::N_Water},
    Entry{"lava",   Kind::N_Lava},
    Entry{"skull",  Kind::N_Skull},
    Entry{"door",   Kind::N_Door},
    Entry{"key",    Kind::N_Key},
    Entry{"text",   Kind::N_Text},

    Entry{"is",       Kind::O_Is},
    Entry{"and",      Kind::O_And},
    Entry{"not",      Kind::O_Not},
    Entry{"on",       Kind::O_On},
    Entry{"make",     Kind::O_Make},

    Entry{"you",    Kind::P_You},
    Entry{"push",   Kind::P_Push},
    Entry{"stop",   Kind::P_Stop},
    Entry{"win",    Kind::P_Win},
    Entry{"defeat", Kind::P_Defeat},
    Entry{"sink",   Kind::P_Sink},
    Entry{"hot",    Kind::P_Hot},
    Entry{"melt",   Kind::P_Melt},
    Entry{"open",   Kind::P_Open},
    Entry{"shut",   Kind::P_Shut},
    Entry{"move",      Kind::P_Move},
    Entry{"eat",       Kind::P_Eat},
    Entry{"weak",      Kind::P_Weak},
    Entry{"auto",      Kind::P_Auto},
    Entry{"fall",      Kind::P_Fall},
    Entry{"fallup",    Kind::P_Fallup},
    Entry{"fallleft",  Kind::P_Fallleft},
    Entry{"fallright", Kind::P_Fallright},
    Entry{"left",      Kind::P_Left},
    Entry{"right",     Kind::P_Right},
    Entry{"up",        Kind::P_Up},
    Entry{"down",      Kind::P_Down},
};

}  // namespace

std::string_view kind_name(Kind k) {
    for (auto const& e : kTable) {
        if (e.kind == k) return e.name;
    }
    return "?";
}

std::optional<Kind> kind_from_name(std::string_view name) {
    for (auto const& e : kTable) {
        if (e.name == name) return e.kind;
    }
    return std::nullopt;
}

}  // namespace baba::core
