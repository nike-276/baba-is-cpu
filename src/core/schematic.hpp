#pragma once

#include "coord.hpp"
#include "world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace baba::core {

struct SchemTag {
    Coord pos;
    enum class Type : std::uint8_t { Input, Output } type;
};

struct SchemRef {
    Coord       pos;
    std::string path;
};

struct Schematic {
    std::string           name;
    Coord                 origin{0, 0};
    World                 world;
    std::vector<SchemTag> tags;
    std::vector<SchemRef> nested;
};

// Rotate N × 90° CW in screen coordinates (y-down).
// Per step: (dx, dy) → (-dy, dx) relative to origin.
// Facing per step: Right→Down→Left→Up→Right.
Schematic rotate_schematic(Schematic const& schem, int steps_cw);

}  // namespace baba::core
