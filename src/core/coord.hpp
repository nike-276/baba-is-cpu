// core/coord.hpp — 2D integer grid coordinate.
//
// Convention: y grows DOWNWARD (screen coordinates). The world is infinite
// (sparse spatial index), so coordinates are signed and unbounded.
#pragma once

#include <cstdint>
#include <functional>

namespace baba::core {

struct Coord {
    std::int32_t x{0};
    std::int32_t y{0};

    constexpr bool operator==(Coord const& other) const = default;
};

struct CoordHash {
    std::size_t operator()(Coord const& c) const noexcept {
        // Splat into 64 bits, then hash.
        std::uint64_t packed =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32) |
             static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.y));
        return std::hash<std::uint64_t>{}(packed);
    }
};

}  // namespace baba::core
