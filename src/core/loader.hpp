// core/loader.hpp — text-format parser for .level and .test files.
//
// Single-pass, line-oriented; no exceptions across the API boundary; first
// error stops parsing and is returned with 1-based line number.
#pragma once

#include "direction.hpp"
#include "kind.hpp"
#include "world.hpp"

#include <istream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace baba::core {

struct ParseError {
    std::string path;
    int         line{0};
    std::string message;
};

struct LoadedLevel {
    std::string                         name;
    World                               world;
    std::map<std::string, std::string>  meta;
};

// ---- .test format types ----

enum class TestActionKind { Wait, MoveRight, MoveUp, MoveLeft, MoveDown, Undo };

struct TestAction {
    TestActionKind kind;
    int            line{0};   // source line for diagnostics
};

enum class AssertionKind {
    At, NotAt, TextAt, Count, TextCount, Won, NotWon, Tick,
};

struct Assertion {
    AssertionKind kind;
    int           line{0};
    Coord         pos{};      // for At/NotAt/TextAt
    Kind          kind_arg{Kind::None};
    int           int_arg{0}; // for Count/TextCount/Tick
};

struct TestScenario {
    LoadedLevel             setup;
    std::vector<TestAction> inputs;
    std::vector<Assertion>  expected;
};

// ---- Parsers ----

std::variant<LoadedLevel, ParseError>  load_level(std::istream& in,  std::string const& path);
std::variant<TestScenario, ParseError> load_test (std::istream& in,  std::string const& path);

// File convenience wrappers.
std::variant<LoadedLevel, ParseError>  load_level_file(std::string const& path);
std::variant<TestScenario, ParseError> load_test_file (std::string const& path);

// ---- Serializer (round-trip for .level) ----

std::string serialize_level(LoadedLevel const& level);

}  // namespace baba::core
