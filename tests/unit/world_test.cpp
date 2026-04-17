#include "check.hpp"
#include "core/world.hpp"

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

TEST(World, all_ids_returns_ascending_order) {
    World w;
    w.spawn({0, 0}, Kind::N_Baba, false);
    w.spawn({1, 0}, Kind::N_Wall, false);
    w.spawn({2, 0}, Kind::N_Rock, false);
    auto ids = w.all_ids();
    CHECK_EQ(ids.size(), std::size_t{3});
    CHECK_EQ(ids[0], ObjectId{0});
    CHECK_EQ(ids[1], ObjectId{1});
    CHECK_EQ(ids[2], ObjectId{2});
}

TEST(World, all_cells_returns_ascending_y_then_x) {
    World w;
    w.spawn({2, 1}, Kind::N_Wall, false);
    w.spawn({0, 0}, Kind::N_Wall, false);
    w.spawn({1, 1}, Kind::N_Wall, false);
    w.spawn({0, 1}, Kind::N_Wall, false);
    auto cells = w.all_cells();
    CHECK_EQ(cells.size(), std::size_t{4});
    CHECK(cells[0] == (Coord{0, 0}));
    CHECK(cells[1] == (Coord{0, 1}));
    CHECK(cells[2] == (Coord{1, 1}));
    CHECK(cells[3] == (Coord{2, 1}));
}
