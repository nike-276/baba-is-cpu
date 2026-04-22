#include "check.hpp"
#include "core/world.hpp"

#include <algorithm>

using namespace baba::core;

TEST(World, spawn_assigns_monotonic_ids) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Baba, false);
    auto b = w.spawn({1, 0}, Kind::N_Wall, false);
    auto c = w.spawn({0, 0}, Kind::N_Rock, false);
    CHECK_EQ(a, ObjectId{0});
    CHECK_EQ(b, ObjectId{1});
    CHECK_EQ(c, ObjectId{2});
    CHECK_EQ(w.object_count(), std::size_t{3});
    CHECK_EQ(w.next_id_preview(), ObjectId{3});
}

TEST(World, ids_never_reused_after_destroy) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Baba, false);
    CHECK(w.destroy(a));
    auto b = w.spawn({0, 0}, Kind::N_Baba, false);
    CHECK_NE(a, b);
    CHECK_EQ(b, ObjectId{1});
}

TEST(World, get_returns_nullptr_for_unknown_id) {
    World w;
    CHECK(w.get(ObjectId{999}) == nullptr);
}

TEST(World, at_returns_empty_vector_for_empty_cell) {
    World w;
    auto const& v = w.at({5, 5});
    CHECK_EQ(v.size(), std::size_t{0});
}

TEST(World, occupied_reflects_grid_state) {
    World w;
    CHECK_FALSE(w.occupied({0, 0}));
    auto id = w.spawn({0, 0}, Kind::N_Wall, false);
    CHECK(w.occupied({0, 0}));
    w.destroy(id);
    CHECK_FALSE(w.occupied({0, 0}));
}

TEST(World, move_updates_both_grid_and_object) {
    World w;
    auto id = w.spawn({1, 1}, Kind::N_Baba, false);
    CHECK(w.move(id, {2, 1}));
    CHECK_FALSE(w.occupied({1, 1}));
    CHECK(w.occupied({2, 1}));
    auto const* o = w.get(id);
    CHECK(o != nullptr);
    CHECK((o->pos == Coord{2, 1}));
}

TEST(World, move_to_same_position_is_noop_and_succeeds) {
    World w;
    auto id = w.spawn({3, 3}, Kind::N_Rock, false);
    CHECK(w.move(id, {3, 3}));
    CHECK(w.occupied({3, 3}));
    CHECK_EQ(w.at({3, 3}).size(), std::size_t{1});
}

TEST(World, empty_cell_invariant_no_empty_vectors) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Wall, false);
    auto b = w.spawn({0, 0}, Kind::N_Rock, false);
    CHECK_EQ(w.cell_count(), std::size_t{1});

    w.destroy(a);
    CHECK_EQ(w.cell_count(), std::size_t{1});  // still has b

    w.destroy(b);
    CHECK_EQ(w.cell_count(), std::size_t{0});  // empty cell purged
    CHECK_FALSE(w.occupied({0, 0}));
}

TEST(World, multiple_objects_per_cell_preserved) {
    World w;
    w.spawn({4, 4}, Kind::N_Baba, false);
    w.spawn({4, 4}, Kind::N_Flag, false);
    auto const& v = w.at({4, 4});
    CHECK_EQ(v.size(), std::size_t{2});
}

TEST(World, face_changes_facing_direction) {
    World w;
    auto id = w.spawn({0, 0}, Kind::N_Baba, false, Direction::Right);
    CHECK(w.face(id, Direction::Up));
    CHECK(w.get(id)->facing == Direction::Up);
}

TEST(World, mutators_return_false_on_unknown_id) {
    World w;
    CHECK_FALSE(w.move(ObjectId{99}, {0, 0}));
    CHECK_FALSE(w.face(ObjectId{99}, Direction::Up));
    CHECK_FALSE(w.destroy(ObjectId{99}));
}

TEST(World, all_ids_contains_all_live_objects) {
    World w;
    w.spawn({0, 0}, Kind::N_Baba, false);
    w.spawn({1, 0}, Kind::N_Wall, false);
    w.spawn({2, 0}, Kind::N_Rock, false);
    auto ids = w.all_ids();
    CHECK_EQ(ids.size(), std::size_t{3});
    auto sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    CHECK_EQ(sorted[0], ObjectId{0});
    CHECK_EQ(sorted[1], ObjectId{1});
    CHECK_EQ(sorted[2], ObjectId{2});
}

TEST(World, all_cells_contains_all_occupied_cells) {
    World w;
    w.spawn({2, 1}, Kind::N_Wall, false);
    w.spawn({0, 0}, Kind::N_Wall, false);
    w.spawn({1, 1}, Kind::N_Wall, false);
    w.spawn({0, 1}, Kind::N_Wall, false);
    auto cells = w.all_cells();
    CHECK_EQ(cells.size(), std::size_t{4});
    auto sorted = cells;
    std::sort(sorted.begin(), sorted.end(), [](Coord a, Coord b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    CHECK((sorted[0] == Coord{0, 0}));
    CHECK((sorted[1] == Coord{0, 1}));
    CHECK((sorted[2] == Coord{1, 1}));
    CHECK((sorted[3] == Coord{2, 1}));
}

// ── respawn (undo-of-Destroy) ────────────────────────────────────────────────

TEST(World, respawn_restores_original_id) {
    World w;
    auto id = w.spawn({3, 3}, Kind::N_Baba, false, Direction::Right);
    w.destroy(id);
    // After destroy, id is gone.
    CHECK(w.get(id) == nullptr);
    // respawn must bring it back with the SAME id.
    w.respawn(id, {3, 3}, Kind::N_Baba, Kind::N_Baba, false, Direction::Right);
    auto const* o = w.get(id);
    CHECK(o != nullptr);
    CHECK((o->pos == Coord{3, 3}));
    CHECK(o->kind == Kind::N_Baba);
    CHECK_FALSE(o->text);
}

TEST(World, respawn_does_not_advance_next_id) {
    World w;
    auto id = w.spawn({0, 0}, Kind::N_Wall, false);
    ObjectId before = w.next_id_preview();
    w.destroy(id);
    w.respawn(id, {0, 0}, Kind::N_Wall, Kind::N_Wall, false, Direction::Right);
    // next_id_ must not change — we are not allocating a new slot.
    CHECK_EQ(w.next_id_preview(), before);
}

TEST(World, respawn_appears_in_grid) {
    World w;
    auto id = w.spawn({7, 2}, Kind::N_Rock, false);
    w.destroy(id);
    CHECK_FALSE(w.occupied({7, 2}));
    w.respawn(id, {7, 2}, Kind::N_Rock, Kind::N_Rock, false, Direction::Right);
    CHECK(w.occupied({7, 2}));
    auto const& cell = w.at({7, 2});
    CHECK_EQ(cell.size(), std::size_t{1});
    CHECK_EQ(cell[0], id);
}

TEST(World, subsequent_spawn_still_gets_fresh_id_after_respawn) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Baba, false);  // id=0
    w.destroy(a);
    w.respawn(a, {0, 0}, Kind::N_Baba, Kind::N_Baba, false, Direction::Right);
    // A regular spawn must still get a fresh monotonic id, not reuse a.
    auto b = w.spawn({1, 0}, Kind::N_Wall, false);
    CHECK_NE(a, b);
    CHECK_EQ(b, ObjectId{1});
}

// ── Kind index (objects_of_kind) ────────────────────────────────────────────

TEST(World, objects_of_kind_empty_for_unseen_kind) {
    World w;
    auto const& v = w.objects_of_kind(Kind::N_Baba);
    CHECK_EQ(v.size(), std::size_t{0});
}

TEST(World, objects_of_kind_tracks_spawns) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Baba, false);
    auto b = w.spawn({1, 0}, Kind::N_Baba, false);
    w.spawn({2, 0}, Kind::N_Wall, false);
    auto const& babas = w.objects_of_kind(Kind::N_Baba);
    CHECK_EQ(babas.size(), std::size_t{2});
    CHECK_EQ(babas[0], a);
    CHECK_EQ(babas[1], b);
    CHECK_EQ(w.objects_of_kind(Kind::N_Wall).size(), std::size_t{1});
}

TEST(World, objects_of_kind_text_buckets_into_ntext) {
    World w;
    w.spawn({0, 0}, Kind::N_Baba, /*text=*/true);   // "BABA" text tile
    w.spawn({1, 0}, Kind::N_Baba, /*text=*/false);  // a real baba object
    CHECK_EQ(w.objects_of_kind(Kind::N_Baba).size(), std::size_t{1});
    CHECK_EQ(w.objects_of_kind(Kind::N_Text).size(), std::size_t{1});
}

TEST(World, objects_of_kind_destroy_removes_id) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Rock, false);
    auto b = w.spawn({1, 0}, Kind::N_Rock, false);
    CHECK(w.destroy(a));
    auto const& rocks = w.objects_of_kind(Kind::N_Rock);
    CHECK_EQ(rocks.size(), std::size_t{1});
    CHECK_EQ(rocks[0], b);
}

TEST(World, objects_of_kind_retype_moves_between_buckets) {
    World w;
    auto id = w.spawn({0, 0}, Kind::N_Baba, false);
    CHECK(w.retype(id, Kind::N_Keke));
    CHECK_EQ(w.objects_of_kind(Kind::N_Baba).size(), std::size_t{0});
    auto const& kekes = w.objects_of_kind(Kind::N_Keke);
    CHECK_EQ(kekes.size(), std::size_t{1});
    CHECK_EQ(kekes[0], id);
}

TEST(World, objects_of_kind_flip_text_moves_to_ntext_and_back) {
    World w;
    auto id = w.spawn({0, 0}, Kind::N_Wall, /*text=*/false);
    CHECK(w.flip_text(id));
    CHECK_EQ(w.objects_of_kind(Kind::N_Wall).size(), std::size_t{0});
    CHECK_EQ(w.objects_of_kind(Kind::N_Text).size(), std::size_t{1});

    CHECK(w.flip_text(id));
    CHECK_EQ(w.objects_of_kind(Kind::N_Text).size(), std::size_t{0});
    auto const& walls = w.objects_of_kind(Kind::N_Wall);
    CHECK_EQ(walls.size(), std::size_t{1});
    CHECK_EQ(walls[0], id);
}

TEST(World, objects_of_kind_move_and_face_preserve_bucket) {
    World w;
    auto id = w.spawn({0, 0}, Kind::N_Baba, false);
    CHECK(w.move(id, {5, 5}));
    CHECK(w.face(id, Direction::Up));
    auto const& babas = w.objects_of_kind(Kind::N_Baba);
    CHECK_EQ(babas.size(), std::size_t{1});
    CHECK_EQ(babas[0], id);
}

TEST(World, objects_of_kind_respawn_inserts_sorted) {
    World w;
    auto a = w.spawn({0, 0}, Kind::N_Rock, false);  // id=0
    auto b = w.spawn({1, 0}, Kind::N_Rock, false);  // id=1
    auto c = w.spawn({2, 0}, Kind::N_Rock, false);  // id=2
    CHECK(w.destroy(b));
    // Now respawn id=1 (non-monotonic w.r.t. next_id_=3): must land in the
    // middle of the bucket to preserve ascending-id invariant.
    w.respawn(b, {1, 0}, Kind::N_Rock, Kind::N_Rock, false, Direction::Right);
    auto const& rocks = w.objects_of_kind(Kind::N_Rock);
    CHECK_EQ(rocks.size(), std::size_t{3});
    CHECK_EQ(rocks[0], a);
    CHECK_EQ(rocks[1], b);
    CHECK_EQ(rocks[2], c);
}

TEST(World, objects_of_kind_respawn_text_goes_to_ntext_bucket) {
    World w;
    auto id = w.spawn({0, 0}, Kind::N_Baba, /*text=*/true);  // BABA text tile
    CHECK(w.destroy(id));
    w.respawn(id, {0, 0}, Kind::N_Baba, Kind::N_Baba, /*text=*/true, Direction::Right);
    CHECK_EQ(w.objects_of_kind(Kind::N_Baba).size(), std::size_t{0});
    CHECK_EQ(w.objects_of_kind(Kind::N_Text).size(), std::size_t{1});
}
