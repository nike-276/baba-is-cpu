// core/direction.hpp — cardinal facing / movement direction.
#pragma once

#include "coord.hpp"
#include <cstdint>
#include <string_view>

namespace baba::core {

enum class Direction : std::uint8_t {
    Right = 0,
    Up    = 1,
    Left  = 2,
    Down  = 3,
};

constexpr Coord step(Direction d) {
    switch (d) {
        case Direction::Right: return { 1,  0};
        case Direction::Up:    return { 0, -1};
        case Direction::Left:  return {-1,  0};
        case Direction::Down:  return { 0,  1};
    }
    return {0, 0};
}

constexpr Direction opposite(Direction d) {
    switch (d) {
        case Direction::Right: return Direction::Left;
        case Direction::Up:    return Direction::Down;
        case Direction::Left:  return Direction::Right;
        case Direction::Down:  return Direction::Up;
    }
    return Direction::Right;
}

constexpr std::string_view direction_name(Direction d) {
    switch (d) {
        case Direction::Right: return "right";
        case Direction::Up:    return "up";
        case Direction::Left:  return "left";
        case Direction::Down:  return "down";
    }
    return "right";
}

}  // namespace baba::core
