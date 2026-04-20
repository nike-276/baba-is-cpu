#include "ruleset.hpp"
#include "direction.hpp"

#include <algorithm>
#include <set>
#include <unordered_set>

namespace baba::core {

namespace {

// Read a single text kind at coord, or std::nullopt if the cell has no text.
std::optional<Kind> text_kind_at(World const& world, Coord c) {
    auto const& cell = world.at(c);
    for (ObjectId id : cell) {
        Object const* obj = world.get(id);
        if (obj && obj->text) return obj->kind;
    }
    return std::nullopt;
}

// Try to parse a rule strip starting at `start`, stepping by `step_dir`.
// Handles:
//   NOUN [AND NOUN]* IS [NOT] PROPERTY [AND [NOT] PROPERTY]*
//   NOUN [AND NOUN]* IS NOUN   (single-target transform; AND-noun predicates deferred)
//   NOT NOUN IS PROPERTY       (NOT-as-subject: applies P to everything except NOUN)
//
// Each parsed PropertyRule carries a `negated` flag for "X IS NOT P" forms.
// NOT-as-subject ("NOT X IS P") produces PropertyRules with negated=false but
// a special N_Not_Subject encoding — handled post-parse (deferred to full NOT pass).
// v1: we implement predicate-NOT cancellation only.
void scan_strip(World const& world, Coord start, Coord step_dir,
                std::vector<PropertyRule>& prop_out,
                std::vector<TransformRule>& xform_out,
                std::vector<MakeRule>& make_out,
                std::vector<EatRule>& eat_out,
                std::vector<ConditionalPropertyRule>& cond_out,
                std::vector<ConditionalTransformRule>& cond_xform_out,
                std::vector<ConditionalMakeRule>& cond_make_out,
                std::vector<FacingPropertyRule>& facing_out,
                std::vector<FacingTransformRule>& facing_xform_out,
                std::vector<GlobalConditionPropertyRule>& global_cond_out,
                std::vector<HasRule>& has_out,
                std::vector<FollowRule>& follow_out,
                std::vector<FearRule>& fear_out,
                std::vector<PlayRule>& play_out) {
    auto at = [&](Coord c) { return text_kind_at(world, c); };
    auto adv = [&](Coord c) -> Coord { return {c.x + step_dir.x, c.y + step_dir.y}; };
    auto bak = [&](Coord c) -> Coord { return {c.x - step_dir.x, c.y - step_dir.y}; };

    // ── [NOT] POWEREDx [AND [NOT] POWEREDy]* NOUN IS PROPERTY ───────────────
    // Detect global prefix condition (with optional AND-chained channels) before
    // the usual noun-subject guard.
    {
        auto is_pow_op = [](Kind k) {
            return k == Kind::O_Powered || k == Kind::O_Powered2 || k == Kind::O_Powered3;
        };
        auto pow_prop = [](Kind k) -> Kind {
            if (k == Kind::O_Powered2) return Kind::P_Power2;
            if (k == Kind::O_Powered3) return Kind::P_Power3;
            return Kind::P_Power;
        };

        auto try_parse_powered = [&]() -> bool {
            Coord pw = start;
            auto tok0 = at(pw);
            if (!tok0) return false;

            // Must start with NOT or a POWEREDx operator.
            if (*tok0 != Kind::O_Not && !is_pow_op(*tok0)) return false;

            // Skip if preceded by AND (mid-chain, handled from the real start).
            {
                auto prev = at(bak(start));
                if (prev && *prev == Kind::O_And) return false;
                // Skip if we are a POWEREDx preceded by NOT (handled from NOT position).
                if (is_pow_op(*tok0) && prev && *prev == Kind::O_Not) return false;
            }

            using Cond = GlobalConditionPropertyRule::Condition;
            std::vector<Cond> conditions;

            auto parse_one_cond = [&]() -> bool {
                auto t = at(pw);
                if (!t) return false;
                if (*t == Kind::O_Not) {
                    auto t2 = at(adv(pw));
                    if (!t2 || !is_pow_op(*t2)) return false;
                    conditions.push_back({pow_prop(*t2), true});
                    pw = adv(adv(pw));  // skip NOT + POWEREDx
                    return true;
                }
                if (is_pow_op(*t)) {
                    conditions.push_back({pow_prop(*t), false});
                    pw = adv(pw);  // skip POWEREDx
                    return true;
                }
                return false;
            };

            if (!parse_one_cond()) return false;

            // AND loop: continue only if AND is followed by [NOT] POWEREDx.
            while (true) {
                auto t = at(pw);
                if (!t || *t != Kind::O_And) break;
                Coord after_and = adv(pw);
                auto peek = at(after_and);
                if (!peek) break;
                bool next_is_pow_cond;
                if (is_pow_op(*peek)) {
                    next_is_pow_cond = true;
                } else if (*peek == Kind::O_Not) {
                    auto p2 = at(adv(after_and));
                    next_is_pow_cond = p2 && is_pow_op(*p2);
                } else {
                    next_is_pow_cond = false;
                }
                if (!next_is_pow_cond) break;
                pw = after_and;  // skip AND
                if (!parse_one_cond()) break;
            }

            // Expect NOUN IS PROPERTY.
            auto subj = at(pw);
            if (!subj || !is_noun(*subj)) return false;
            pw = adv(pw);
            auto is_tok = at(pw);
            if (!is_tok || *is_tok != Kind::O_Is) return false;
            pw = adv(pw);
            auto prop_tok = at(pw);
            if (!prop_tok || !is_property(*prop_tok)) return false;

            global_cond_out.push_back({*subj, *prop_tok, std::move(conditions)});
            return true;
        };
        if (try_parse_powered()) return;
    }

    // Guard: don't start a rule from inside an AND-chained noun list.
    // This covers several cases:
    //   subject chains: "BABA AND KEKE IS P" — don't start from KEKE
    //   condition chains: "X ON A AND B IS P" — don't start from B
    //   mixed-polarity condition chains: "X ON A AND NOT ON B AND C IS P" —
    //     don't start from C (which follows "AND" but is a condition noun,
    //     not a new subject)
    // Walk backwards through (noun AND)* chains; if O_On/O_Facing is reached
    // we're in a condition list; if we walked any AND-noun pair without finding
    // O_On we're in a subject list. Either way, skip — the rule was already
    // parsed starting from the first noun.
    {
        auto is_pow_op_g = [](Kind k) {
            return k == Kind::O_Powered || k == Kind::O_Powered2 || k == Kind::O_Powered3;
        };
        Coord cur = bak(start);
        bool walked_and_chain = false;
        while (true) {
            auto k = at(cur);
            if (!k) break;
            if (*k == Kind::O_On || *k == Kind::O_Facing || is_pow_op_g(*k) ||
                *k == Kind::O_Follow || *k == Kind::O_Fear) return;
            // [NOT] POWEREDx acts as a prefix condition chain-starter
            if (*k == Kind::O_Not) {
                auto after_not = at(adv(cur));
                if (after_not && is_pow_op_g(*after_not)) return;
                break;
            }
            if (*k != Kind::O_And) break;
            Coord noun_pos = bak(cur);
            auto nk = at(noun_pos);
            if (!nk) break;
            // AND followed by a POWEREDx operator — part of a powered prefix chain
            if (is_pow_op_g(*nk)) return;
            if (!is_noun(*nk)) {
                // Check for AND NOT ON pattern — we're inside a condition clause list.
                if (*nk == Kind::O_Not) {
                    Coord on_pos = bak(noun_pos);
                    auto ok = at(on_pos);
                    if (ok && *ok == Kind::O_On) return;  // in condition clause list
                }
                break;
            }
            walked_and_chain = true;
            cur = bak(noun_pos);
        }
        if (walked_and_chain) return;
    }

    Coord cursor = start;
    auto first = at(cursor);
    if (!first || !is_noun(*first)) return;

    std::vector<Kind> subjects;
    subjects.push_back(*first);
    cursor = adv(cursor);

    // Optional AND NOUN repetition (subjects).
    while (true) {
        auto k = at(cursor);
        if (!k || *k != Kind::O_And) break;
        Coord next = adv(cursor);
        auto noun = at(next);
        if (!noun || !is_noun(*noun)) return;
        subjects.push_back(*noun);
        cursor = adv(next);
    }

    // Operator: IS, ON, or MAKE.
    auto op_tok = at(cursor);
    if (!op_tok) return;

    // ── NOUN MAKE NOUN [AND NOUN]* ──────────────────────────────────────────
    if (*op_tok == Kind::O_Make) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    make_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN EAT NOUN [AND NOUN]* ───────────────────────────────────────────
    if (*op_tok == Kind::O_Eat) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    eat_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN FACING <cond> IS NOUN|PROPERTY ──────────────────────────────
    // <cond> is either a noun (adjacent tile) or P_Left/Right/Up/Down (own facing).
    if (*op_tok == Kind::O_Facing) {
        cursor = adv(cursor);
        auto cond_k = at(cursor);
        if (!cond_k) return;
        bool cond_is_dir = (*cond_k == Kind::P_Left || *cond_k == Kind::P_Right ||
                            *cond_k == Kind::P_Up   || *cond_k == Kind::P_Down);
        if (!is_noun(*cond_k) && !cond_is_dir) return;
        Kind cond = *cond_k;
        cursor = adv(cursor);
        if (auto verb = at(cursor); !verb || *verb != Kind::O_Is) return;
        cursor = adv(cursor);
        auto pk = at(cursor);
        if (!pk) return;
        bool neg = false;
        if (*pk == Kind::O_Not) { neg = true; cursor = adv(cursor); pk = at(cursor); }
        if (!pk) return;
        if (is_noun(*pk)) {
            if (!neg) for (Kind n : subjects)
                if (n != *pk) facing_xform_out.push_back({n, cond, *pk, false});
        } else if (is_property(*pk)) {
            for (Kind n : subjects)
                facing_out.push_back({n, cond, *pk, neg});
        }
        return;
    }

    // ── NOUN HAS NOUN [AND NOUN]* ───────────────────────────────────────────
    if (*op_tok == Kind::O_Has) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    has_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN FOLLOW NOUN [AND NOUN]* ───────────────────────────────────────
    if (*op_tok == Kind::O_Follow) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    follow_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN FEAR NOUN [AND NOUN]* ─────────────────────────────────────────
    if (*op_tok == Kind::O_Fear) {
        cursor = adv(cursor);
        auto target = at(cursor);
        if (target && is_noun(*target)) {
            std::vector<Kind> targets;
            targets.push_back(*target);
            cursor = adv(cursor);
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                Coord next = adv(cursor);
                auto nt = at(next);
                if (!nt || !is_noun(*nt)) break;
                targets.push_back(*nt);
                cursor = adv(next);
            }
            for (Kind n : subjects)
                for (Kind t : targets)
                    fear_out.push_back({n, t});
        }
        return;
    }

    // ── NOUN [NOT] ON ... [AND [NOT] ON ...]* IS/MAKE PREDICATE ───────────
    // Unified handler for compound ON conditions with mixed polarity.
    // op_tok is O_On for "NOUN ON ..." or O_Not for "NOUN NOT ON ..."
    // (the O_Not case for FACING is handled separately below).
    {
        bool is_cond_on = (*op_tok == Kind::O_On);
        bool is_cond_not_on = false;
        if (*op_tok == Kind::O_Not) {
            // Peek: NOT must be followed by ON (not FACING, not IS, etc.)
            Coord peek = adv(cursor);
            auto pk = at(peek);
            is_cond_not_on = (pk && *pk == Kind::O_On);
        }
        if (is_cond_on || is_cond_not_on) {
            // Parse one or more condition clauses.
            std::vector<CondClause> clauses;
            Coord clause_cursor = cursor;
            while (true) {
                auto clause_tok = at(clause_cursor);
                if (!clause_tok) break;
                bool clause_neg = false;
                if (*clause_tok == Kind::O_Not) {
                    clause_neg = true;
                    clause_cursor = adv(clause_cursor);
                    clause_tok = at(clause_cursor);
                    if (!clause_tok || *clause_tok != Kind::O_On) break; // malformed
                }
                if (*clause_tok != Kind::O_On) break;
                clause_cursor = adv(clause_cursor);
                auto cn = at(clause_cursor);
                if (!cn || !is_noun(*cn)) break;
                CondClause cl;
                cl.negated = clause_neg;
                cl.nouns.push_back(*cn);
                clause_cursor = adv(clause_cursor);
                // AND NOUN* within this clause (nouns only, not another clause)
                while (true) {
                    auto ak = at(clause_cursor);
                    if (!ak || *ak != Kind::O_And) break;
                    Coord after_and = adv(clause_cursor);
                    auto nt = at(after_and);
                    if (!nt || !is_noun(*nt)) break; // AND NOT or AND IS etc — end inner loop
                    cl.nouns.push_back(*nt);
                    clause_cursor = adv(after_and);
                }
                clauses.push_back(std::move(cl));
                // Check if next tokens are AND [NOT] ON (another clause)
                auto ak = at(clause_cursor);
                if (!ak || *ak != Kind::O_And) break;
                Coord after_and = adv(clause_cursor);
                auto after_and_tok = at(after_and);
                if (!after_and_tok) break;
                if (*after_and_tok == Kind::O_On) {
                    clause_cursor = after_and; // consume AND, next iter starts at ON
                    continue;
                }
                if (*after_and_tok == Kind::O_Not) {
                    Coord after_not = adv(after_and);
                    auto after_not_tok = at(after_not);
                    if (after_not_tok && *after_not_tok == Kind::O_On) {
                        clause_cursor = after_and; // consume AND, next iter starts at NOT ON
                        continue;
                    }
                }
                break; // AND is not followed by [NOT] ON → end of condition clauses
            }
            if (clauses.empty()) return; // no valid clause parsed

            cursor = clause_cursor;
            auto verb2 = at(cursor);
            if (!verb2) return;

            // NOUN ON … MAKE NOUN [AND NOUN]*
            if (*verb2 == Kind::O_Make) {
                cursor = adv(cursor);
                auto t0 = at(cursor);
                if (!t0 || !is_noun(*t0)) return;
                std::vector<Kind> targets;
                targets.push_back(*t0);
                cursor = adv(cursor);
                while (true) {
                    auto k = at(cursor);
                    if (!k || *k != Kind::O_And) break;
                    Coord next = adv(cursor);
                    auto nt = at(next);
                    if (!nt || !is_noun(*nt)) break;
                    targets.push_back(*nt);
                    cursor = adv(next);
                }
                for (Kind n : subjects)
                    for (Kind t : targets)
                        cond_make_out.push_back({n, clauses, t});
                return;
            }

            if (*verb2 != Kind::O_Is) return;
            cursor = adv(cursor);

            // Conditional transform: NOUN ON … IS NOUN [AND NOUN]*
            {
                auto pred = at(cursor);
                if (pred && is_noun(*pred)) {
                    std::vector<Kind> targets;
                    targets.push_back(*pred);
                    cursor = adv(cursor);
                    while (true) {
                        auto k = at(cursor);
                        if (!k || *k != Kind::O_And) break;
                        Coord next = adv(cursor);
                        auto nt = at(next);
                        if (!nt || !is_noun(*nt)) break;
                        targets.push_back(*nt);
                        cursor = adv(next);
                    }
                    for (Kind n : subjects)
                        for (Kind t : targets)
                            if (n != t)
                                cond_xform_out.push_back({n, clauses, t});
                    return;
                }
            }

            // Conditional property: NOUN ON … IS [NOT] PROPERTY [AND …]
            struct PropEntry { Kind prop; bool neg; };
            std::vector<PropEntry> props;
            auto parse_one_prop_local = [&]() -> bool {
                auto k = at(cursor);
                if (!k) return false;
                bool neg = false;
                if (*k == Kind::O_Not) {
                    neg = true;
                    cursor = adv(cursor);
                    k = at(cursor);
                    if (!k || !is_property(*k)) return false;
                }
                if (!is_property(*k)) return false;
                props.push_back({*k, neg});
                cursor = adv(cursor);
                return true;
            };
            if (!parse_one_prop_local()) return;
            while (true) {
                auto k = at(cursor);
                if (!k || *k != Kind::O_And) break;
                cursor = adv(cursor);
                if (!parse_one_prop_local()) { cursor = bak(cursor); break; }
            }
            for (Kind n : subjects)
                for (auto const& pe : props)
                    if (!pe.neg) cond_out.push_back({n, clauses, pe.prop});
            return;
        }
    }

    // ── NOUN PLAY NOTE [OCTAVE] [ACCIDENTAL] ─────────────────────────────
    // Modifiers (octave number, sharp/flat) may appear in any order after the note.
    if (*op_tok == Kind::O_Play) {
        cursor = adv(cursor);
        auto note_k = at(cursor);
        if (note_k && is_note_token(*note_k)) {
            Kind note = *note_k;
            cursor = adv(cursor);
            int  octave = 5;
            bool sharp  = false;
            bool flat   = false;
            // Parse optional modifiers in any order (up to 2 passes).
            for (int pass = 0; pass < 2; ++pass) {
                auto mk = at(cursor);
                if (!mk) break;
                if (is_num_token(*mk)) {
                    octave = num_to_int(*mk);
                    cursor = adv(cursor);
                } else if (*mk == Kind::O_Sharp) {
                    sharp = true;
                    cursor = adv(cursor);
                } else if (*mk == Kind::O_Flat) {
                    flat = true;
                    cursor = adv(cursor);
                } else {
                    break;
                }
            }
            for (Kind n : subjects)
                play_out.push_back({n, note, octave, sharp, flat});
        }
        return;
    }

    // ── NOUN NOT FACING <cond> IS NOUN|PROPERTY ────────────────────────────
    if (*op_tok == Kind::O_Not) {
        cursor = adv(cursor);
        auto on_check = at(cursor);

        if (on_check && *on_check == Kind::O_Facing) {
            cursor = adv(cursor);
            auto cond_k = at(cursor);
            if (!cond_k) return;
            bool cond_is_dir = (*cond_k == Kind::P_Left || *cond_k == Kind::P_Right ||
                                *cond_k == Kind::P_Up   || *cond_k == Kind::P_Down);
            if (!is_noun(*cond_k) && !cond_is_dir) return;
            Kind cond = *cond_k;
            cursor = adv(cursor);
            if (auto verb = at(cursor); !verb || *verb != Kind::O_Is) return;
            cursor = adv(cursor);
            auto pk = at(cursor);
            if (!pk) return;
            bool neg = false;
            if (*pk == Kind::O_Not) { neg = true; cursor = adv(cursor); pk = at(cursor); }
            if (!pk) return;
            if (is_noun(*pk)) {
                if (!neg) for (Kind n : subjects)
                    if (n != *pk) facing_xform_out.push_back({n, cond, *pk, true}); // negated
            } else if (is_property(*pk)) {
                for (Kind n : subjects)
                    facing_out.push_back({n, cond, *pk, !neg}); // negated=true (NOT FACING)
            }
            return;
        }

        // Not O_On and not O_Facing after O_Not — fall through to IS handling below.
        // We need to un-advance cursor since we advanced past O_Not already.
        cursor = bak(cursor);
    }

    // Must be IS.
    if (*op_tok != Kind::O_Is) return;
    cursor = adv(cursor);

    // Predicate: optional leading NOT, then PROPERTY or NOUN.
    auto pred0 = at(cursor);
    if (!pred0) return;

    // Check for NOUN IS NOUN [AND NOUN]* (transform, possibly multi-target).
    if (is_noun(*pred0)) {
        std::vector<Kind> targets;
        targets.push_back(*pred0);
        cursor = adv(cursor);
        while (true) {
            auto k = at(cursor);
            if (!k || *k != Kind::O_And) break;
            Coord next = adv(cursor);
            auto nt = at(next);
            if (!nt || !is_noun(*nt)) break;
            targets.push_back(*nt);
            cursor = adv(next);
        }
        for (Kind n : subjects)
            for (Kind t : targets)
                xform_out.push_back({n, t});
        return;
    }

    // Parse property phrase: [NOT] PROPERTY [AND [NOT] PROPERTY]*
    if (!is_property(*pred0) && *pred0 != Kind::O_Not) return;

    struct PropEntry { Kind prop; bool neg; };
    std::vector<PropEntry> props;

    auto parse_one_prop = [&]() -> bool {
        auto k = at(cursor);
        if (!k) return false;
        bool neg = false;
        if (*k == Kind::O_Not) {
            neg = true;
            cursor = adv(cursor);
            k = at(cursor);
            if (!k || !is_property(*k)) return false;
        }
        if (!is_property(*k)) return false;
        props.push_back({*k, neg});
        cursor = adv(cursor);
        return true;
    };

    if (!parse_one_prop()) return;

    // Optional AND [NOT] PROPERTY repetition.
    while (true) {
        auto k = at(cursor);
        if (!k || *k != Kind::O_And) break;
        cursor = adv(cursor);
        if (!parse_one_prop()) { cursor = {cursor.x - step_dir.x, cursor.y - step_dir.y}; break; }
    }

    for (Kind n : subjects) {
        for (auto const& pe : props) {
            prop_out.push_back({n, pe.prop, pe.neg});
        }
    }
}

}  // namespace

RuleSet RuleSet::parse(World const& world) {
    RuleSet rs;

    auto cells = world.all_cells();
    for (Coord c : cells) {
        auto const& ids = world.at(c);
        bool has_text = false;
        for (ObjectId id : ids) {
            Object const* o = world.get(id);
            if (o && o->text) { has_text = true; break; }
        }
        if (!has_text) continue;
        scan_strip(world, c, {1, 0}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.facing_rules_, rs.facing_transforms_, rs.global_cond_rules_, rs.has_rules_, rs.follow_rules_, rs.fear_rules_, rs.play_rules_);
        scan_strip(world, c, {0, 1}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.facing_rules_, rs.facing_transforms_, rs.global_cond_rules_, rs.has_rules_, rs.follow_rules_, rs.fear_rules_, rs.play_rules_);
    }

    // Deduplicate verb-operator rules before further processing.
    // Multiple scan starting positions can produce the same rule (e.g. scanning
    // from ROCK in "BABA AND ROCK IS FLAG" duplicates the ROCK IS FLAG entry).
    // Property-rule duplicates are harmless (index deduplicates lookups), but
    // transform duplicates would cause xmap to contain the same target twice,
    // leading to objects being spawned multiple times.
    {
        std::set<std::pair<uint16_t,uint16_t>> seen;
        std::vector<TransformRule> deduped;
        for (auto const& tr : rs.transforms_) {
            auto key2 = std::make_pair(static_cast<uint16_t>(tr.from),
                                       static_cast<uint16_t>(tr.to));
            if (seen.insert(key2).second) deduped.push_back(tr);
        }
        rs.transforms_ = std::move(deduped);
    }
    {
        std::set<std::pair<uint16_t,uint16_t>> seen;
        std::vector<EatRule> deduped;
        for (auto const& er : rs.eats_) {
            auto key2 = std::make_pair(static_cast<uint16_t>(er.subject),
                                       static_cast<uint16_t>(er.target));
            if (seen.insert(key2).second) deduped.push_back(er);
        }
        rs.eats_ = std::move(deduped);
    }
    {
        std::vector<GlobalConditionPropertyRule> deduped;
        for (auto const& gcr : rs.global_cond_rules_) {
            bool dup = false;
            for (auto const& ex : deduped) {
                if (ex.subject == gcr.subject && ex.property == gcr.property
                    && ex.conditions == gcr.conditions) {
                    dup = true;
                    break;
                }
            }
            if (!dup) deduped.push_back(gcr);
        }
        rs.global_cond_rules_ = std::move(deduped);
    }
    {
        std::set<std::pair<uint16_t,uint16_t>> seen;
        std::vector<HasRule> deduped;
        for (auto const& hr : rs.has_rules_) {
            auto key2 = std::make_pair(static_cast<uint16_t>(hr.subject),
                                       static_cast<uint16_t>(hr.target));
            if (seen.insert(key2).second) deduped.push_back(hr);
        }
        rs.has_rules_ = std::move(deduped);
    }
    {
        std::set<std::pair<uint16_t,uint16_t>> seen;
        std::vector<FollowRule> deduped;
        for (auto const& fr : rs.follow_rules_) {
            auto key2 = std::make_pair(static_cast<uint16_t>(fr.subject),
                                       static_cast<uint16_t>(fr.target));
            if (seen.insert(key2).second) deduped.push_back(fr);
        }
        rs.follow_rules_ = std::move(deduped);
    }
    // FearRule: no dedup — identical entries represent stacking (reserved for future use).
    {
        std::set<std::tuple<uint16_t,uint16_t,int,bool,bool>> seen;
        std::vector<PlayRule> deduped;
        for (auto const& pr : rs.play_rules_) {
            auto key5 = std::make_tuple(static_cast<uint16_t>(pr.subject),
                                        static_cast<uint16_t>(pr.note),
                                        pr.octave, pr.sharp, pr.flat);
            if (seen.insert(key5).second) deduped.push_back(pr);
        }
        rs.play_rules_ = std::move(deduped);
    }

    // Base rule: TEXT IS PUSH (always, treated as positive non-cancellable).
    rs.rules_.push_back({Kind::N_Text, Kind::P_Push, false});

    // NOT cancellation (spec §3.3 rule 1):
    // If both "X IS P" and "X IS NOT P" exist, neither applies.
    // Build a set of (subject, property) pairs that are contradicted.
    using Key = std::uint32_t;
    auto key = [](Kind n, Kind p) -> Key {
        return (static_cast<Key>(n) << 16) | static_cast<Key>(p);
    };

    std::unordered_set<Key> positive_keys, negative_keys;
    for (auto const& r : rs.rules_) {
        if (r.negated) negative_keys.insert(key(r.subject, r.property));
        else           positive_keys.insert(key(r.subject, r.property));
    }
    // Contradicted = intersection of positive and negative.
    std::unordered_set<Key> cancelled;
    for (Key k2 : positive_keys)
        if (negative_keys.count(k2)) cancelled.insert(k2);

    // Build final positive rules (exclude cancelled and negated).
    std::vector<PropertyRule> final_rules;
    for (auto const& r : rs.rules_) {
        if (r.negated) continue;  // never index negated rules directly
        if (cancelled.count(key(r.subject, r.property))) continue;  // cancelled by NOT
        final_rules.push_back(r);
    }
    rs.rules_ = std::move(final_rules);

    // Build lookup index from surviving positive rules.
    for (auto const& r : rs.rules_) {
        rs.index_.insert(key_(r.subject, r.property));
    }
    return rs;
}

bool RuleSet::has_property(Kind noun, Kind property) const {
    if (index_.count(key_(noun, property))) return true;
    return false;
}

bool RuleSet::object_has_property(World const& world, ObjectId id, Kind property) const {
    Object const* o = world.get(id);
    if (!o) return false;
    Kind noun = o->text ? Kind::N_Text : o->kind;
    if (index_.count(key_(noun, property))) return true;

    // Check conditional rules (NOUN ON/NOT ON NOUN [AND NOUN]* IS PROPERTY).
    if (!o->text) {
        for (auto const& cr : cond_rules_) {
            if (cr.subject != o->kind || cr.property != property) continue;
            // ALL clauses must pass. Each clause: ON → all nouns present; NOT ON → not all present.
            bool all_clauses_pass = true;
            for (auto const& clause : cr.clauses) {
                bool all_found = true;
                for (Kind cn : clause.nouns) {
                    bool found = false;
                    for (ObjectId other : world.at(o->pos)) {
                        if (other == id) continue;
                        Object const* ob = world.get(other);
                        if (ob && !ob->text && ob->kind == cn) { found = true; break; }
                    }
                    if (!found) { all_found = false; break; }
                }
                bool clause_pass = clause.negated ? !all_found : all_found;
                if (!clause_pass) { all_clauses_pass = false; break; }
            }
            if (all_clauses_pass) return true;
        }

        // Check FACING rules (NOUN [NOT] FACING <cond> IS PROPERTY).
        for (auto const& fr : facing_rules_) {
            if (fr.subject != o->kind || fr.property != property) continue;
            bool matched;
            bool cond_is_dir = (fr.condition == Kind::P_Left || fr.condition == Kind::P_Right ||
                                fr.condition == Kind::P_Up   || fr.condition == Kind::P_Down);
            if (cond_is_dir) {
                Direction req = fr.condition == Kind::P_Left  ? Direction::Left  :
                                fr.condition == Kind::P_Right ? Direction::Right :
                                fr.condition == Kind::P_Up    ? Direction::Up    :
                                                                Direction::Down;
                matched = (o->facing == req);
            } else {
                Coord face_pos = {o->pos.x + step(o->facing).x, o->pos.y + step(o->facing).y};
                matched = false;
                for (ObjectId other : world.at(face_pos)) {
                    Object const* ob = world.get(other);
                    if (ob && !ob->text && ob->kind == fr.condition) { matched = true; break; }
                }
            }
            if (fr.negated ? !matched : matched) return true;
        }

        // Check global conditional rules ([NOT] POWEREDx [AND …] NOUN IS PROPERTY).
        for (auto const& gcr : global_cond_rules_) {
            if (gcr.subject != o->kind || gcr.property != property) continue;
            bool all_met = true;
            for (auto const& cond : gcr.conditions) {
                bool pw_exists = any_has_power_kind(world, cond.power_kind);
                if (cond.negated ? pw_exists : !pw_exists) { all_met = false; break; }
            }
            if (all_met) return true;
        }
    }
    return false;
}

bool RuleSet::any_has_power_kind(World const& world, Kind power_prop) const {
    for (ObjectId id : world.all_ids()) {
        Object const* o = world.get(id);
        if (!o || o->text) continue;
        // Unconditional (X IS POWERx → in index)
        if (index_.count(key_(o->kind, power_prop))) return true;
        // Conditional via ON/NOT ON (no recursion into global_cond_rules_)
        for (auto const& cr : cond_rules_) {
            if (cr.subject != o->kind || cr.property != power_prop) continue;
            bool all_clauses_pass = true;
            for (auto const& clause : cr.clauses) {
                bool all_found = true;
                for (Kind cn : clause.nouns) {
                    bool found = false;
                    for (ObjectId oid : world.at(o->pos)) {
                        if (oid == id) continue;
                        Object const* ob = world.get(oid);
                        if (ob && !ob->text && ob->kind == cn) { found = true; break; }
                    }
                    if (!found) { all_found = false; break; }
                }
                bool clause_pass = clause.negated ? !all_found : all_found;
                if (!clause_pass) { all_clauses_pass = false; break; }
            }
            if (all_clauses_pass) return true;
        }
    }
    return false;
}

}  // namespace baba::core
