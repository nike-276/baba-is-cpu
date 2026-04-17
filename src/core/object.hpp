// core/object.hpp — a single thing in the world (sprite OR text).
#pragma once

#include "coord.hpp"
#include "direction.hpp"
#include "kind.hpp"
#include <cstdint>

namespace baba::core {

using ObjectId = std::uint32_t;
constexpr ObjectId kInvalidId = static_cast<ObjectId>(-1);

struct Object {
    ObjectId  id{kInvalidId};
    Coord     pos{};
    Kind      kind{Kind::None};
    bool      text{false};
    Direction facing{Direction::Right};
};

}  // namespace baba::core
