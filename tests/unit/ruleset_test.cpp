// Unit tests for RuleSet::any_grants invariants.
// These document the correctness guarantees that phase-guard optimizations rely on:
// if any_grants(P) returns false, no object can have property P at runtime, so
// phases guarded by !any_grants(P) may safely short-circuit to a no-op result.
#include "check.hpp"
#include "core/ruleset.hpp"
#include "core/world.hpp"

using namespace baba::core;

// Helper: spawn a horizontal "NOUN IS PROP" text strip starting at (col, row).
static void spawn_rule(World& w, int row, Kind noun, Kind prop) {
    w.spawn({0, row}, noun, /*text=*/true);
    w.spawn({1, row}, Kind::O_Is, /*text=*/true);
    w.spawn({2, row}, prop, /*text=*/true);
}

// ── any_grants for P_You ──────────────────────────────────────────────────

TEST(RuleSet_AnyGrants, you_false_when_no_rules) {
    World w;
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_You));
}

TEST(RuleSet_AnyGrants, you_false_when_only_push_rule) {
    World w;
    spawn_rule(w, 0, Kind::N_Rock, Kind::P_Push);
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_You));
}

TEST(RuleSet_AnyGrants, you_true_when_baba_is_you) {
    World w;
    spawn_rule(w, 0, Kind::N_Baba, Kind::P_You);
    RuleSet rs = RuleSet::parse(w);
    CHECK(rs.any_grants(Kind::P_You));
}

TEST(RuleSet_AnyGrants, you_false_after_not_cancellation) {
    // BABA IS YOU and BABA IS NOT YOU cancel each other → P_You must not be granted.
    World w;
    spawn_rule(w, 0, Kind::N_Baba, Kind::P_You);
    // BABA IS NOT YOU: spawn NOT between IS and YOU
    w.spawn({0, 1}, Kind::N_Baba, /*text=*/true);
    w.spawn({1, 1}, Kind::O_Is, /*text=*/true);
    w.spawn({2, 1}, Kind::O_Not, /*text=*/true);
    w.spawn({3, 1}, Kind::P_You, /*text=*/true);
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_You));
}

// ── any_grants for P_Win ──────────────────────────────────────────────────

TEST(RuleSet_AnyGrants, win_false_when_no_rules) {
    World w;
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_Win));
}

TEST(RuleSet_AnyGrants, win_true_when_flag_is_win) {
    World w;
    spawn_rule(w, 0, Kind::N_Flag, Kind::P_Win);
    RuleSet rs = RuleSet::parse(w);
    CHECK(rs.any_grants(Kind::P_Win));
}

TEST(RuleSet_AnyGrants, win_false_when_only_you_rule) {
    World w;
    spawn_rule(w, 0, Kind::N_Baba, Kind::P_You);
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_Win));
}

// ── any_grants for P_Revert ───────────────────────────────────────────────

TEST(RuleSet_AnyGrants, revert_false_when_no_rules) {
    World w;
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_Revert));
}

TEST(RuleSet_AnyGrants, revert_true_when_rock_is_revert) {
    World w;
    spawn_rule(w, 0, Kind::N_Rock, Kind::P_Revert);
    RuleSet rs = RuleSet::parse(w);
    CHECK(rs.any_grants(Kind::P_Revert));
}

TEST(RuleSet_AnyGrants, revert_false_when_only_win_and_you) {
    World w;
    spawn_rule(w, 0, Kind::N_Baba, Kind::P_You);
    spawn_rule(w, 1, Kind::N_Flag, Kind::P_Win);
    RuleSet rs = RuleSet::parse(w);
    CHECK_FALSE(rs.any_grants(Kind::P_Revert));
}

// ── any_grants must not be confused by unrelated properties ──────────────

TEST(RuleSet_AnyGrants, multiple_properties_tracked_independently) {
    World w;
    spawn_rule(w, 0, Kind::N_Baba, Kind::P_You);
    spawn_rule(w, 1, Kind::N_Flag, Kind::P_Win);
    spawn_rule(w, 2, Kind::N_Rock, Kind::P_Push);
    RuleSet rs = RuleSet::parse(w);
    CHECK(rs.any_grants(Kind::P_You));
    CHECK(rs.any_grants(Kind::P_Win));
    CHECK(rs.any_grants(Kind::P_Push));
    CHECK_FALSE(rs.any_grants(Kind::P_Revert));
    CHECK_FALSE(rs.any_grants(Kind::P_Stop));
}
