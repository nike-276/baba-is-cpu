#include "loader.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string_view>

namespace baba::core {

namespace {

ParseError err(std::string const& path, int line, std::string msg) {
    return ParseError{path, line, std::move(msg)};
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' || s.back()  == '\r')) s.remove_suffix(1);
    return s;
}

// Tokenize on whitespace, honoring "double-quoted" tokens with \" and \\.
std::vector<std::string> tokenize(std::string_view line) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size()) break;
        if (line[i] == '"') {
            ++i;
            std::string acc;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    char nxt = line[i + 1];
                    if (nxt == '"' || nxt == '\\') { acc.push_back(nxt); i += 2; continue; }
                }
                acc.push_back(line[i]);
                ++i;
            }
            if (i < line.size()) ++i;  // consume closing "
            out.push_back(std::move(acc));
        } else {
            std::size_t start = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
            out.emplace_back(line.substr(start, i - start));
        }
    }
    return out;
}

bool parse_int32(std::string const& s, std::int32_t& out) {
    auto first = s.data();
    auto last  = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}

bool parse_int(std::string const& s, int& out) {
    auto first = s.data();
    auto last  = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}

std::optional<Direction> parse_facing(std::string const& s) {
    if (s == "right") return Direction::Right;
    if (s == "up")    return Direction::Up;
    if (s == "left")  return Direction::Left;
    if (s == "down")  return Direction::Down;
    return std::nullopt;
}

// Try to apply one body record (object/text/version/name/meta) into `level`.
// Returns true if the keyword was recognized.
//
// `seen_version` and `seen_name` track required-once headers.
// On error, sets `error` and returns true (consumed the line).
bool try_body_record(std::vector<std::string> const& tok,
                     LoadedLevel& level,
                     bool& seen_version,
                     bool& seen_name,
                     std::string const& path,
                     int line_no,
                     std::optional<ParseError>& error)
{
    auto const& kw = tok[0];

    if (kw == "version") {
        if (seen_version) { error = err(path, line_no, "record version: duplicate header"); return true; }
        seen_version = true;
        if (tok.size() != 2) { error = err(path, line_no, "record version: expected `version <int>`"); return true; }
        int v;
        if (!parse_int(tok[1], v) || v != 1) { error = err(path, line_no, "field version: only version 1 is supported"); return true; }
        return true;
    }

    if (kw == "name") {
        if (seen_name) { error = err(path, line_no, "record name: duplicate header"); return true; }
        seen_name = true;
        if (tok.size() != 2) { error = err(path, line_no, "record name: expected `name \"<string>\"`"); return true; }
        level.name = tok[1];
        return true;
    }

    if (kw == "meta") {
        if (tok.size() != 3) { error = err(path, line_no, "record meta: expected `meta <key> <value>`"); return true; }
        level.meta[tok[1]] = tok[2];
        return true;
    }

    if (kw == "object") {
        if (tok.size() != 5) { error = err(path, line_no, "record object: expected `object <x> <y> <kind> <facing>`"); return true; }
        std::int32_t x, y;
        if (!parse_int32(tok[1], x)) { error = err(path, line_no, "coord x: not an integer"); return true; }
        if (!parse_int32(tok[2], y)) { error = err(path, line_no, "coord y: not an integer"); return true; }
        auto k = kind_from_name(tok[3]);
        if (!k || !is_noun(*k))   { error = err(path, line_no, "kind: object record requires a noun name"); return true; }
        auto f = parse_facing(tok[4]);
        if (!f)                   { error = err(path, line_no, "facing: expected right|up|left|down"); return true; }
        level.world.spawn({x, y}, *k, /*text=*/false, *f);
        return true;
    }

    if (kw == "text") {
        if (tok.size() != 4) { error = err(path, line_no, "record text: expected `text <x> <y> <kind>`"); return true; }
        std::int32_t x, y;
        if (!parse_int32(tok[1], x)) { error = err(path, line_no, "coord x: not an integer"); return true; }
        if (!parse_int32(tok[2], y)) { error = err(path, line_no, "coord y: not an integer"); return true; }
        auto k = kind_from_name(tok[3]);
        if (!k)                   { error = err(path, line_no, "kind: unknown text token"); return true; }
        // Coexistence: at most one text per tile.
        for (ObjectId id : level.world.at({x, y})) {
            Object const* o = level.world.get(id);
            if (o && o->text) {
                error = err(path, line_no, "tile: at most one text object per tile");
                return true;
            }
        }
        level.world.spawn({x, y}, *k, /*text=*/true, Direction::Right);
        return true;
    }

    return false;  // not a body record
}

}  // namespace

// ----- load_level -----

std::variant<LoadedLevel, ParseError> load_level(std::istream& in, std::string const& path) {
    LoadedLevel level;
    bool seen_version{false}, seen_name{false};
    std::string raw;
    int line_no = 0;
    std::optional<ParseError> error;

    while (std::getline(in, raw)) {
        ++line_no;
        std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') continue;

        auto tok = tokenize(line);
        if (tok.empty()) continue;

        if (try_body_record(tok, level, seen_version, seen_name, path, line_no, error)) {
            if (error) return *error;
            continue;
        }
        return err(path, line_no, "record " + tok[0] + ": unknown keyword");
    }

    if (!seen_version) return err(path, line_no, "record version: required header missing");
    if (!seen_name)    return err(path, line_no, "record name: required header missing");
    return level;
}

std::variant<LoadedLevel, ParseError> load_level_file(std::string const& path) {
    std::ifstream f(path);
    if (!f) return err(path, 0, "file: cannot open");
    return load_level(f, path);
}

// ----- load_test -----

namespace {

bool parse_action(std::vector<std::string> const& tok, TestAction& out) {
    if (tok.size() == 1 && tok[0] == "wait") { out.kind = TestActionKind::Wait; return true; }
    if (tok.size() == 2 && tok[0] == "input") {
        if (tok[1] == "right") { out.kind = TestActionKind::MoveRight; return true; }
        if (tok[1] == "up")    { out.kind = TestActionKind::MoveUp;    return true; }
        if (tok[1] == "left")  { out.kind = TestActionKind::MoveLeft;  return true; }
        if (tok[1] == "down")  { out.kind = TestActionKind::MoveDown;  return true; }
        if (tok[1] == "wait")  { out.kind = TestActionKind::Wait;      return true; }
        if (tok[1] == "undo")  { out.kind = TestActionKind::Undo;      return true; }
    }
    return false;
}

bool parse_assertion(std::vector<std::string> const& tok, Assertion& out, std::string& msg) {
    auto& kw = tok[0];
    auto need_xy_kind = [&](AssertionKind ak) -> bool {
        if (tok.size() != 4) { msg = "expected `" + kw + " <x> <y> <kind>`"; return false; }
        std::int32_t x, y;
        if (!parse_int32(tok[1], x)) { msg = "coord x: not an integer"; return false; }
        if (!parse_int32(tok[2], y)) { msg = "coord y: not an integer"; return false; }
        auto k = kind_from_name(tok[3]);
        if (!k) { msg = "kind: unknown name"; return false; }
        out.kind = ak; out.pos = {x, y}; out.kind_arg = *k;
        return true;
    };
    auto need_kind_int = [&](AssertionKind ak) -> bool {
        if (tok.size() != 3) { msg = "expected `" + kw + " <kind> <int>`"; return false; }
        auto k = kind_from_name(tok[1]);
        if (!k) { msg = "kind: unknown name"; return false; }
        int n;
        if (!parse_int(tok[2], n)) { msg = "int: not an integer"; return false; }
        out.kind = ak; out.kind_arg = *k; out.int_arg = n;
        return true;
    };

    if (kw == "at")         return need_xy_kind(AssertionKind::At);
    if (kw == "not_at")     return need_xy_kind(AssertionKind::NotAt);
    if (kw == "text_at")    return need_xy_kind(AssertionKind::TextAt);
    if (kw == "count")      return need_kind_int(AssertionKind::Count);
    if (kw == "text_count") return need_kind_int(AssertionKind::TextCount);
    if (kw == "won")        { if (tok.size() != 1) { msg = "won takes no args"; return false; } out.kind = AssertionKind::Won;    return true; }
    if (kw == "not_won")    { if (tok.size() != 1) { msg = "not_won takes no args"; return false; } out.kind = AssertionKind::NotWon; return true; }
    if (kw == "tick") {
        if (tok.size() != 2) { msg = "expected `tick <int>`"; return false; }
        int n;
        if (!parse_int(tok[1], n)) { msg = "int: not an integer"; return false; }
        out.kind = AssertionKind::Tick; out.int_arg = n; return true;
    }
    if (kw == "sound_count") {
        if (tok.size() != 2) { msg = "expected `sound_count <int>`"; return false; }
        int n;
        if (!parse_int(tok[1], n)) { msg = "int: not an integer"; return false; }
        out.kind = AssertionKind::SoundCount; out.int_arg = n; return true;
    }
    msg = "record " + kw + ": unknown assertion";
    return false;
}

}  // namespace

std::variant<TestScenario, ParseError> load_test(std::istream& in, std::string const& path) {
    TestScenario sc;
    enum class Section { None, Setup, Inputs, Expected } section = Section::None;
    bool seen_version{false}, seen_name{false};
    std::string raw;
    int line_no = 0;
    std::optional<ParseError> error;

    while (std::getline(in, raw)) {
        ++line_no;
        std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') continue;

        if (line.front() == '[') {
            std::string_view s = line;
            if (s == "[setup]")    { section = Section::Setup;    continue; }
            if (s == "[inputs]")   { section = Section::Inputs;   continue; }
            if (s == "[expected]") { section = Section::Expected; continue; }
            return err(path, line_no, "section: expected [setup], [inputs], or [expected]");
        }

        auto tok = tokenize(line);
        if (tok.empty()) continue;

        switch (section) {
            case Section::None:
                return err(path, line_no, "section: data before [setup]");
            case Section::Setup: {
                if (!try_body_record(tok, sc.setup, seen_version, seen_name, path, line_no, error)) {
                    return err(path, line_no, "record " + tok[0] + ": unknown setup keyword");
                }
                if (error) return *error;
                break;
            }
            case Section::Inputs: {
                TestAction a; a.line = line_no;
                if (!parse_action(tok, a)) {
                    return err(path, line_no, "record " + tok[0] + ": unknown input action");
                }
                sc.inputs.push_back(a);
                break;
            }
            case Section::Expected: {
                Assertion a; a.line = line_no;
                std::string msg;
                if (!parse_assertion(tok, a, msg)) {
                    return err(path, line_no, msg);
                }
                sc.expected.push_back(a);
                break;
            }
        }
    }

    if (!seen_version) return err(path, line_no, "record version: required in [setup]");
    if (!seen_name)    return err(path, line_no, "record name: required in [setup]");
    return sc;
}

std::variant<TestScenario, ParseError> load_test_file(std::string const& path) {
    std::ifstream f(path);
    if (!f) return err(path, 0, "file: cannot open");
    return load_test(f, path);
}

// ----- serialize_level -----

std::string serialize_level(LoadedLevel const& level) {
    std::ostringstream os;
    os << "version 1\n";
    os << "name \"" << level.name << "\"\n";
    for (auto const& [k, v] : level.meta) {
        os << "meta " << k << " " << v << "\n";
    }
    if (level.world.object_count() > 0) os << "\n";

    // Emit in ascending id order so the file is stable across loads.
    for (ObjectId id : level.world.all_ids()) {
        Object const* o = level.world.get(id);
        if (!o) continue;
        if (o->text) {
            os << "text " << o->pos.x << " " << o->pos.y << " " << kind_name(o->kind) << "\n";
        } else {
            os << "object " << o->pos.x << " " << o->pos.y << " "
               << kind_name(o->kind) << " " << direction_name(o->facing) << "\n";
        }
    }
    return os.str();
}

// ----- load_schematic -----

std::variant<Schematic, ParseError> load_schematic(std::istream& in, std::string const& path) {
    LoadedLevel level_part;  // reuse try_body_record for object/text/version/name records
    Schematic   schem;
    bool seen_version{false}, seen_name{false}, seen_origin{false};
    std::string raw;
    int line_no = 0;
    std::optional<ParseError> error;

    while (std::getline(in, raw)) {
        ++line_no;
        std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') continue;

        auto tok = tokenize(line);
        if (tok.empty()) continue;

        auto const& kw = tok[0];

        if (kw == "origin") {
            if (seen_origin) return err(path, line_no, "record origin: duplicate");
            seen_origin = true;
            if (tok.size() != 3) return err(path, line_no, "record origin: expected `origin <x> <y>`");
            std::int32_t x, y;
            if (!parse_int32(tok[1], x)) return err(path, line_no, "coord x: not an integer");
            if (!parse_int32(tok[2], y)) return err(path, line_no, "coord y: not an integer");
            schem.origin = {x, y};
            continue;
        }

        if (kw == "tag") {
            if (tok.size() != 4) return err(path, line_no, "record tag: expected `tag <x> <y> input|output`");
            std::int32_t x, y;
            if (!parse_int32(tok[1], x)) return err(path, line_no, "coord x: not an integer");
            if (!parse_int32(tok[2], y)) return err(path, line_no, "coord y: not an integer");
            SchemTag::Type type;
            if      (tok[3] == "input")  type = SchemTag::Type::Input;
            else if (tok[3] == "output") type = SchemTag::Type::Output;
            else return err(path, line_no, "field tag: expected input or output");
            schem.tags.push_back({{x, y}, type});
            continue;
        }

        if (kw == "schem") {
            if (tok.size() != 4) return err(path, line_no, "record schem: expected `schem <x> <y> \"<path>\"`");
            std::int32_t x, y;
            if (!parse_int32(tok[1], x)) return err(path, line_no, "coord x: not an integer");
            if (!parse_int32(tok[2], y)) return err(path, line_no, "coord y: not an integer");
            schem.nested.push_back({{x, y}, tok[3]});
            continue;
        }

        if (try_body_record(tok, level_part, seen_version, seen_name, path, line_no, error)) {
            if (error) return *error;
            continue;
        }

        return err(path, line_no, "record " + tok[0] + ": unknown keyword");
    }

    if (!seen_version) return err(path, line_no, "record version: required header missing");
    if (!seen_name)    return err(path, line_no, "record name: required header missing");
    if (!seen_origin)  return err(path, line_no, "record origin: required header missing");

    schem.name  = std::move(level_part.name);
    schem.world = std::move(level_part.world);
    return schem;
}

std::variant<Schematic, ParseError> load_schematic_file(std::string const& path) {
    std::ifstream f(path);
    if (!f) return err(path, 0, "file: cannot open");
    return load_schematic(f, path);
}

std::string serialize_schematic(Schematic const& schem) {
    std::ostringstream os;
    os << "version 1\n";
    os << "name \"" << schem.name << "\"\n";
    os << "origin " << schem.origin.x << " " << schem.origin.y << "\n";
    if (schem.world.object_count() > 0 || !schem.tags.empty() || !schem.nested.empty())
        os << "\n";

    for (ObjectId id : schem.world.all_ids()) {
        Object const* o = schem.world.get(id);
        if (!o) continue;
        if (o->text) {
            os << "text " << o->pos.x << " " << o->pos.y << " " << kind_name(o->kind) << "\n";
        } else {
            os << "object " << o->pos.x << " " << o->pos.y << " "
               << kind_name(o->kind) << " " << direction_name(o->facing) << "\n";
        }
    }

    for (auto const& tag : schem.tags) {
        os << "tag " << tag.pos.x << " " << tag.pos.y << " "
           << (tag.type == SchemTag::Type::Input ? "input" : "output") << "\n";
    }

    for (auto const& ref : schem.nested) {
        os << "schem " << ref.pos.x << " " << ref.pos.y << " \"" << ref.path << "\"\n";
    }

    return os.str();
}

}  // namespace baba::core
