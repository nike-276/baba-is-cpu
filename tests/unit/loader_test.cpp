#include "check.hpp"
#include "core/loader.hpp"

#include <sstream>
#include <string>

using namespace baba::core;

namespace {

LoadedLevel parse_level_or_die(std::string const& src) {
    std::istringstream in(src);
    auto r = load_level(in, "<inline>");
    if (auto* e = std::get_if<ParseError>(&r)) {
        ::baba::testing::report("loader", "unexpected ParseError @line " +
                                              std::to_string(e->line) + ": " + e->message);
        return {};
    }
    return std::get<LoadedLevel>(std::move(r));
}

ParseError parse_level_expect_err(std::string const& src) {
    std::istringstream in(src);
    auto r = load_level(in, "<inline>");
    if (std::get_if<LoadedLevel>(&r)) {
        ::baba::testing::report("loader", "expected ParseError, got LoadedLevel");
        return {};
    }
    return std::get<ParseError>(std::move(r));
}

TestScenario parse_test_or_die(std::string const& src) {
    std::istringstream in(src);
    auto r = load_test(in, "<inline>");
    if (auto* e = std::get_if<ParseError>(&r)) {
        ::baba::testing::report("loader", "unexpected ParseError @line " +
                                              std::to_string(e->line) + ": " + e->message);
        return {};
    }
    return std::get<TestScenario>(std::move(r));
}

ParseError parse_test_expect_err(std::string const& src) {
    std::istringstream in(src);
    auto r = load_test(in, "<inline>");
    if (std::get_if<TestScenario>(&r)) {
        ::baba::testing::report("loader", "expected ParseError, got TestScenario");
        return {};
    }
    return std::get<ParseError>(std::move(r));
}

}  // namespace

// ---- load_level ----

TEST(LoadLevel, minimal_header_only) {
    auto lvl = parse_level_or_die("version 1\nname \"hello\"\n");
    CHECK(lvl.name == "hello");
    CHECK_EQ(lvl.world.object_count(), std::size_t{0});
}

TEST(LoadLevel, blank_lines_and_comments_ignored) {
    auto lvl = parse_level_or_die(
        "# comment\n"
        "\n"
        "version 1\n"
        "  # indented comment\n"
        "name \"x\"\n"
        "\n"
        "object 0 0 baba right\n"
    );
    CHECK_EQ(lvl.world.object_count(), std::size_t{1});
}

TEST(LoadLevel, missing_version_errors) {
    auto e = parse_level_expect_err("name \"x\"\n");
    CHECK(e.message.find("version") != std::string::npos);
}

TEST(LoadLevel, missing_name_errors) {
    auto e = parse_level_expect_err("version 1\n");
    CHECK(e.message.find("name") != std::string::npos);
}

TEST(LoadLevel, wrong_version_errors) {
    auto e = parse_level_expect_err("version 2\nname \"x\"\n");
    CHECK(e.message.find("version") != std::string::npos);
}

TEST(LoadLevel, duplicate_version_errors) {
    auto e = parse_level_expect_err("version 1\nversion 1\nname \"x\"\n");
    CHECK(e.message.find("duplicate") != std::string::npos);
}

TEST(LoadLevel, unknown_keyword_errors) {
    auto e = parse_level_expect_err("version 1\nname \"x\"\nbogus 1 2 3\n");
    CHECK(e.message.find("unknown") != std::string::npos);
    CHECK_EQ(e.line, 3);
}

TEST(LoadLevel, object_kind_must_be_a_noun) {
    auto e = parse_level_expect_err(
        "version 1\nname \"x\"\nobject 0 0 is right\n"
    );
    CHECK(e.message.find("noun") != std::string::npos);
}

TEST(LoadLevel, object_facing_must_be_valid) {
    auto e = parse_level_expect_err(
        "version 1\nname \"x\"\nobject 0 0 baba sideways\n"
    );
    CHECK(e.message.find("facing") != std::string::npos);
}

TEST(LoadLevel, object_coords_must_parse) {
    auto e = parse_level_expect_err(
        "version 1\nname \"x\"\nobject zero 0 baba right\n"
    );
    CHECK(e.message.find("integer") != std::string::npos);
}

TEST(LoadLevel, text_at_most_one_per_tile) {
    auto e = parse_level_expect_err(
        "version 1\n"
        "name \"x\"\n"
        "text 0 0 baba\n"
        "text 0 0 wall\n"
    );
    CHECK(e.message.find("at most one") != std::string::npos);
}

TEST(LoadLevel, text_and_object_can_coexist) {
    auto lvl = parse_level_or_die(
        "version 1\n"
        "name \"x\"\n"
        "text 0 0 baba\n"
        "object 0 0 baba right\n"
    );
    CHECK_EQ(lvl.world.object_count(), std::size_t{2});
}

TEST(LoadLevel, meta_records_collected) {
    auto lvl = parse_level_or_die(
        "version 1\nname \"x\"\nmeta author akaash\nmeta seed 42\n"
    );
    CHECK_EQ(lvl.meta.size(), std::size_t{2});
    CHECK(lvl.meta["author"] == "akaash");
    CHECK(lvl.meta["seed"] == "42");
}

TEST(LoadLevel, negative_coords_supported) {
    auto lvl = parse_level_or_die(
        "version 1\nname \"x\"\nobject -3 -7 baba right\n"
    );
    CHECK_EQ(lvl.world.object_count(), std::size_t{1});
    auto ids = lvl.world.all_ids();
    auto const* o = lvl.world.get(ids[0]);
    CHECK(o->pos == (Coord{-3, -7}));
}

// ---- serialize_level round-trip ----

TEST(SerializeLevel, round_trip_preserves_objects) {
    auto orig = parse_level_or_die(
        "version 1\n"
        "name \"round\"\n"
        "object 1 2 baba right\n"
        "object 3 4 wall up\n"
        "text 5 6 is\n"
    );
    auto serialized = serialize_level(orig);
    auto round = parse_level_or_die(serialized);

    CHECK(round.name == orig.name);
    CHECK_EQ(round.world.object_count(), orig.world.object_count());

    auto orig_ids  = orig.world.all_ids();
    auto round_ids = round.world.all_ids();
    CHECK_EQ(orig_ids.size(), round_ids.size());
    for (std::size_t i = 0; i < orig_ids.size(); ++i) {
        auto const* a = orig.world.get(orig_ids[i]);
        auto const* b = round.world.get(round_ids[i]);
        CHECK(a->pos == b->pos);
        CHECK(a->kind == b->kind);
        CHECK_EQ(a->text, b->text);
        CHECK(a->facing == b->facing);
    }
}

// ---- load_test ----

TEST(LoadTest, parses_three_sections) {
    auto sc = parse_test_or_die(
        "[setup]\n"
        "version 1\n"
        "name \"t\"\n"
        "object 0 0 baba right\n"
        "text 1 0 baba\n"
        "text 2 0 is\n"
        "text 3 0 you\n"
        "[inputs]\n"
        "input right\n"
        "wait\n"
        "[expected]\n"
        "at 1 0 baba\n"
        "won\n"
        "tick 2\n"
    );
    CHECK(sc.setup.name == "t");
    CHECK_EQ(sc.inputs.size(), std::size_t{2});
    CHECK(sc.inputs[0].kind == TestActionKind::MoveRight);
    CHECK(sc.inputs[1].kind == TestActionKind::Wait);
    CHECK_EQ(sc.expected.size(), std::size_t{3});
    CHECK(sc.expected[0].kind == AssertionKind::At);
    CHECK(sc.expected[0].pos == (Coord{1, 0}));
    CHECK(sc.expected[1].kind == AssertionKind::Won);
    CHECK(sc.expected[2].kind == AssertionKind::Tick);
    CHECK_EQ(sc.expected[2].int_arg, 2);
}

TEST(LoadTest, data_before_setup_errors) {
    auto e = parse_test_expect_err("version 1\n[setup]\nname \"x\"\n");
    CHECK(e.message.find("[setup]") != std::string::npos);
}

TEST(LoadTest, unknown_section_errors) {
    auto e = parse_test_expect_err("[bogus]\n");
    CHECK(e.message.find("section") != std::string::npos);
}

TEST(LoadTest, unknown_input_errors) {
    auto e = parse_test_expect_err(
        "[setup]\nversion 1\nname \"x\"\n"
        "[inputs]\nteleport\n"
    );
    CHECK(e.message.find("unknown") != std::string::npos);
}

TEST(LoadTest, unknown_assertion_errors) {
    auto e = parse_test_expect_err(
        "[setup]\nversion 1\nname \"x\"\n"
        "[expected]\nbogus\n"
    );
    CHECK(e.message.find("unknown") != std::string::npos);
}

TEST(LoadTest, count_assertion_parses) {
    auto sc = parse_test_or_die(
        "[setup]\nversion 1\nname \"x\"\n"
        "[expected]\ncount baba 3\ntext_count is 1\n"
    );
    CHECK_EQ(sc.expected.size(), std::size_t{2});
    CHECK(sc.expected[0].kind == AssertionKind::Count);
    CHECK(sc.expected[0].kind_arg == Kind::N_Baba);
    CHECK_EQ(sc.expected[0].int_arg, 3);
    CHECK(sc.expected[1].kind == AssertionKind::TextCount);
    CHECK(sc.expected[1].kind_arg == Kind::O_Is);
    CHECK_EQ(sc.expected[1].int_arg, 1);
}

TEST(LoadTest, all_input_directions_and_undo_parse) {
    auto sc = parse_test_or_die(
        "[setup]\nversion 1\nname \"x\"\n"
        "[inputs]\n"
        "input right\ninput up\ninput left\ninput down\n"
        "input wait\ninput undo\nwait\n"
    );
    CHECK_EQ(sc.inputs.size(), std::size_t{7});
    CHECK(sc.inputs[0].kind == TestActionKind::MoveRight);
    CHECK(sc.inputs[1].kind == TestActionKind::MoveUp);
    CHECK(sc.inputs[2].kind == TestActionKind::MoveLeft);
    CHECK(sc.inputs[3].kind == TestActionKind::MoveDown);
    CHECK(sc.inputs[4].kind == TestActionKind::Wait);
    CHECK(sc.inputs[5].kind == TestActionKind::Undo);
    CHECK(sc.inputs[6].kind == TestActionKind::Wait);
}
