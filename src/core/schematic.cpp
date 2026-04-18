#include "schematic.hpp"

namespace baba::core {

Schematic rotate_schematic(Schematic const& schem, int steps_cw) {
    steps_cw = ((steps_cw % 4) + 4) % 4;
    if (steps_cw == 0) return schem;

    int32_t ox = schem.origin.x;
    int32_t oy = schem.origin.y;

    // Apply one 90° CW step: (dx, dy) → (-dy, dx)
    auto rotate_pos = [&](Coord pos) -> Coord {
        int32_t dx = pos.x - ox;
        int32_t dy = pos.y - oy;
        for (int s = 0; s < steps_cw; ++s) {
            int32_t ndx = -dy;
            int32_t ndy =  dx;
            dx = ndx;
            dy = ndy;
        }
        return {ox + dx, oy + dy};
    };

    // CW rotation of facing: Right→Down→Left→Up→Right
    auto rotate_dir = [&](Direction d) -> Direction {
        for (int s = 0; s < steps_cw; ++s) {
            switch (d) {
                case Direction::Right: d = Direction::Down;  break;
                case Direction::Down:  d = Direction::Left;  break;
                case Direction::Left:  d = Direction::Up;    break;
                case Direction::Up:    d = Direction::Right; break;
            }
        }
        return d;
    };

    Schematic result;
    result.name   = schem.name;
    result.origin = schem.origin;

    // Rotate objects (preserve spawn order for determinism).
    for (ObjectId id : schem.world.all_ids()) {
        Object const* o = schem.world.get(id);
        if (!o) continue;
        result.world.spawn(rotate_pos(o->pos), o->kind, o->text, rotate_dir(o->facing));
    }

    // Rotate tags.
    for (auto const& tag : schem.tags)
        result.tags.push_back({rotate_pos(tag.pos), tag.type});

    // Rotate nested ref positions (sub-schem content is rotated at paste time).
    for (auto const& ref : schem.nested)
        result.nested.push_back({rotate_pos(ref.pos), ref.path});

    return result;
}

}  // namespace baba::core
