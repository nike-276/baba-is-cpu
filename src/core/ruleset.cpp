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
                std::vector<ConditionalEatRule>& cond_eat_out,
                std::vector<FacingPropertyRule>& facing_out,
                std::vector<FacingTransformRule>& facing_xform_out,
                std::vector<GlobalConditionPropertyRule>& global_cond_out,
                std::vector<GlobalConditionEatRule>& global_cond_eat_out,
                std::vector<GlobalConditionMakeRule>& global_cond_make_out,
                std::vector<GlobalConditionTransformRule>& global_cond_xform_out,
                std::vector<HasRule>& has_out,
                std::vector<FearRule>& fear_out,
                std::vector<PlayRule>& play_out,
                std::vector<ConditionalPlayRule>& cond_play_out,
                std::unordered_map<Coord, Kind, CoordHash> const& word_at = {}) {
    auto at = [&](Coord c) -> std::optional<Kind> {
        // Real text objects take priority.
        if (auto tk = text_kind_at(world, c)) return tk;
        // Non-text objects that have the WORD property act as their own kind.
        auto it = word_at.find(c);
        if (it != word_at.end()) return it->second;
        return std::nullopt;
    };
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

            // Expect NOUN [AND NOUN]* [[NOT] ON NOUN]* (IS … | EAT … | MAKE …).
            auto subj = at(pw);
            if (!subj || !is_noun(*subj)) return false;
            std::vector<Kind> subjects;
            subjects.push_back(*subj);
            pw = adv(pw);
            // AND-chained additional subjects. Stop if the token after AND isn't
            // a noun (could be a condition continuation — leave those to the ON
            // clause loop below, though no ON-clause AND chain ever precedes the
            // first clause, so in practice this just short-circuits on AND ON).
            while (true) {
                auto ak = at(pw);
                if (!ak || *ak != Kind::O_And) break;
                Coord nk_pos = adv(pw);
                auto nk = at(nk_pos);
                if (!nk || !is_noun(*nk)) break;
                subjects.push_back(*nk);
                pw = adv(nk_pos);
            }

            // Optional ON/NOT ON clause chain between subject and verb.
            std::vector<CondClause> on_clauses;
            while (true) {
                auto t0 = at(pw);
                if (!t0) break;
                bool clause_neg = false;
                if (*t0 == Kind::O_Not) {
                    auto t1 = at(adv(pw));
                    if (!t1 || *t1 != Kind::O_On) break;
                    clause_neg = true;
                    pw = adv(pw); // skip NOT, now pw points at ON
                    t0 = t1;
                }
                if (*t0 != Kind::O_On) break;
                pw = adv(pw); // skip ON
                auto cn = at(pw);
                if (!cn || !is_noun(*cn)) break;
                CondClause cl;
                cl.negated = clause_neg;
                cl.nouns.push_back(*cn);
                pw = adv(pw);
                // AND NOUN* within this clause
                while (true) {
                    auto ak = at(pw);
                    if (!ak || *ak != Kind::O_And) break;
                    Coord after_and = adv(pw);
                    auto nt = at(after_and);
                    if (!nt || !is_noun(*nt)) break;
                    // Don't consume if the next AND is followed by NOT ON or ON (new clause)
                    auto peek2 = at(adv(after_and));
                    if (!peek2) { cl.nouns.push_back(*nt); pw = adv(after_and); continue; }
                    // If nt is a noun and next after it is another noun/AND-as-new-clause, ok
                    cl.nouns.push_back(*nt);
                    pw = adv(after_and);
                }
                on_clauses.push_back(std::move(cl));
                // Peek: if next is AND followed by [NOT] ON, consume AND and continue loop
                auto ak = at(pw);
                if (!ak || *ak != Kind::O_And) break;
                Coord after_and = adv(pw);
                auto peek = at(after_and);
                if (!peek) break;
                bool next_is_on_clause = (*peek == Kind::O_On);
                if (!next_is_on_clause && *peek == Kind::O_Not) {
                    auto p2 = at(adv(after_and));
                    next_is_on_clause = p2 && *p2 == Kind::O_On;
                }
                if (!next_is_on_clause) break;
                pw = after_and; // consume AND, next iteration handles [NOT] ON
            }

            auto op_tok = at(pw);
            if (!op_tok) return false;

            if (*op_tok == Kind::O_Is) {
                pw = adv(pw);
                auto pred_tok = at(pw);
                if (!pred_tok) return false;
                if (is_property(*pred_tok) && on_clauses.empty()) {
                    for (Kind s : subjects)
                        global_cond_out.push_back({s, *pred_tok, conditions});
                    return true;
                }
                if (is_noun(*pred_tok)) {
                    for (Kind s : subjects)
                        global_cond_xform_out.push_back({s, *pred_tok, conditions, on_clauses});
                    return true;
                }
                return false;
            }
            if (*op_tok == Kind::O_Eat) {
                pw = adv(pw);
                auto target_tok = at(pw);
                if (!target_tok || !is_noun(*target_tok)) return false;
                std::vector<Kind> targets;
                targets.push_back(*target_tok);
                pw = adv(pw);
                while (true) {
                    auto ak = at(pw);
                    if (!ak || *ak != Kind::O_And) break;
                    Coord next = adv(pw);
                    auto nt = at(next);
                    if (!nt || !is_noun(*nt)) break;
                    targets.push_back(*nt);
                    pw = adv(next);
                }
                for (Kind s : subjects)
                    for (Kind t : targets)
                        global_cond_eat_out.push_back({s, t, conditions, on_clauses});
                return true;
            }
            if (*op_tok == Kind::O_Make) {
                pw = adv(pw);
                auto target_tok = at(pw);
                if (!target_tok || !is_noun(*target_tok)) return false;
                std::vector<Kind> targets;
                targets.push_back(*target_tok);
                pw = adv(pw);
                while (true) {
                    auto ak = at(pw);
                    if (!ak || *ak != Kind::O_And) break;
                    Coord next = adv(pw);
                    auto nt = at(next);
                    if (!nt || !is_noun(*nt)) break;
                    targets.push_back(*nt);
                    pw = adv(next);
                }
                for (Kind s : subjects)
                    for (Kind t : targets)
                        global_cond_make_out.push_back({s, t, conditions, on_clauses});
                return true;
            }
            return false;
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
            if (*k == Kind::O_On || *k == Kind::O_Facing || *k == Kind::O_FacedBy ||
                is_pow_op_g(*k) || *k == Kind::O_Fear) return;
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
                // Check for AND NOT ON/FACEDBY pattern — we're inside a condition clause list.
                if (*nk == Kind::O_Not) {
                    Coord on_pos = bak(noun_pos);
                    auto ok = at(on_pos);
                    if (ok && (*ok == Kind::O_On || *ok == Kind::O_FacedBy)) return;
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

    // ── NOUN [NOT] ON/FACEDBY ... [AND [NOT] ON/FACEDBY ...]* IS/MAKE PREDICATE ─
    // Unified handler for compound ON and FACEDBY conditions with mixed polarity.
    // op_tok is O_On/O_FacedBy for positive or O_Not for "NOUN NOT ON/FACEDBY ..."
    // (the O_Not case for FACING is handled separately below).
    {
        bool is_cond_on      = (*op_tok == Kind::O_On);
        bool is_cond_facedby = (*op_tok == Kind::O_FacedBy);
        bool is_cond_not_on      = false;
        bool is_cond_not_facedby = false;
        if (*op_tok == Kind::O_Not) {
            // Peek: NOT must be followed by ON or FACEDBY (not FACING, not IS, etc.)
            Coord peek = adv(cursor);
            auto pk = at(peek);
            is_cond_not_on      = (pk && *pk == Kind::O_On);
            is_cond_not_facedby = (pk && *pk == Kind::O_FacedBy);
        }
        if (is_cond_on || is_cond_not_on || is_cond_facedby || is_cond_not_facedby) {
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
                    if (!clause_tok || (*clause_tok != Kind::O_On && *clause_tok != Kind::O_FacedBy)) break;
                }
                if (*clause_tok != Kind::O_On && *clause_tok != Kind::O_FacedBy) break;
                CondType clause_type = (*clause_tok == Kind::O_FacedBy) ? CondType::FacedBy : CondType::On;
                clause_cursor = adv(clause_cursor);
                auto cn = at(clause_cursor);
                if (!cn || !is_noun(*cn)) break;
                CondClause cl;
                cl.ctype   = clause_type;
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
                // Check if next tokens are AND [NOT] ON/FACEDBY (another clause)
                auto ak = at(clause_cursor);
                if (!ak || *ak != Kind::O_And) break;
                Coord after_and = adv(clause_cursor);
                auto after_and_tok = at(after_and);
                if (!after_and_tok) break;
                if (*after_and_tok == Kind::O_On || *after_and_tok == Kind::O_FacedBy) {
                    clause_cursor = after_and; // consume AND, next iter starts at ON/FACEDBY
                    continue;
                }
                if (*after_and_tok == Kind::O_Not) {
                    Coord after_not = adv(after_and);
                    auto after_not_tok = at(after_not);
                    if (after_not_tok && (*after_not_tok == Kind::O_On || *after_not_tok == Kind::O_FacedBy)) {
                        clause_cursor = after_and; // consume AND, next iter starts at NOT ON/FACEDBY
                        continue;
                    }
                }
                break; // AND is not followed by [NOT] ON/FACEDBY → end of condition clauses
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

            // NOUN ON … EAT NOUN [AND NOUN]*
            if (*verb2 == Kind::O_Eat) {
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
                        cond_eat_out.push_back({n, clauses, t});
                return;
            }

            // NOUN ON … PLAY NOTE [OCTAVE] [ACCIDENTAL]
            if (*verb2 == Kind::O_Play) {
                cursor = adv(cursor);
                auto note_k = at(cursor);
                if (!note_k || !is_note_token(*note_k)) return;
                Kind note = *note_k;
                cursor = adv(cursor);
                int  octave = 5;
                bool sharp  = false;
                bool flat   = false;
                for (int pass = 0; pass < 2; ++pass) {
                    auto mk = at(cursor);
                    if (!mk) break;
                    if (is_num_token(*mk)) { octave = num_to_int(*mk); cursor = adv(cursor); }
                    else if (*mk == Kind::O_Sharp) { sharp = true; cursor = adv(cursor); }
                    else if (*mk == Kind::O_Flat)  { flat  = true; cursor = adv(cursor); }
                    else break;
                }
                for (Kind n : subjects)
                    cond_play_out.push_back({n, clauses, note, octave, sharp, flat});
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
    // Pass 1: scan from cells containing real text objects.
    for (Coord c : cells) {
        auto const& ids = world.at(c);
        bool has_text = false;
        for (ObjectId id : ids) {
            Object const* o = world.get(id);
            if (o && o->text) { has_text = true; break; }
        }
        if (!has_text) continue;
        scan_strip(world, c, {1, 0}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.cond_eats_, rs.facing_rules_, rs.facing_transforms_, rs.global_cond_rules_, rs.global_cond_eat_rules_, rs.global_cond_make_rules_, rs.global_cond_transforms_, rs.has_rules_, rs.fear_rules_, rs.play_rules_, rs.cond_play_rules_);
        scan_strip(world, c, {0, 1}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.cond_eats_, rs.facing_rules_, rs.facing_transforms_, rs.global_cond_rules_, rs.global_cond_eat_rules_, rs.global_cond_make_rules_, rs.global_cond_transforms_, rs.has_rules_, rs.fear_rules_, rs.play_rules_, rs.cond_play_rules_);
    }

    // Pass 2: WORD — objects that have P_Word (via any rule form) act as their own text tile.
    // TEXT IS WORD has no effect (wiki). Evaluate all rule forms per-object.
    std::unordered_map<Coord, Kind, CoordHash> word_at;
    {
        // Pre-index positive/negative unconditional P_Word rules for fast lookup.
        std::unordered_set<Kind> word_pos, word_neg;
        for (auto const& r : rs.rules_) {
            if (r.property != Kind::P_Word || r.subject == Kind::N_Text) continue;
            if (r.negated) word_neg.insert(r.subject); else word_pos.insert(r.subject);
        }

        // Check if any non-text object in the world has a given power property
        // (pre-index approximation: checks only unconditional positive rules).
        auto world_has_power = [&](Kind power_prop) -> bool {
            for (auto const& r : rs.rules_) {
                if (r.negated || r.property != power_prop) continue;
                for (ObjectId oid : world.all_ids()) {
                    Object const* ob = world.get(oid);
                    if (ob && !ob->text && ob->kind == r.subject) return true;
                }
            }
            return false;
        };

        for (ObjectId id : world.all_ids()) {
            Object const* o = world.get(id);
            if (!o || o->text || o->kind == Kind::N_Text) continue;
            Kind noun = o->kind;
            bool has_word = false;

            // 1. Unconditional X IS WORD (with tentative NOT cancellation).
            if (!has_word && word_pos.count(noun) && !word_neg.count(noun))
                has_word = true;

            // 2. Conditional X ON/NOT ON Y IS WORD.
            if (!has_word) {
                for (auto const& cr : rs.cond_rules_) {
                    if (cr.subject != noun || cr.property != Kind::P_Word) continue;
                    bool all_pass = true;
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
                        if (!(clause.negated ? !all_found : all_found)) { all_pass = false; break; }
                    }
                    if (all_pass) { has_word = true; break; }
                }
            }

            // 3. X [NOT] FACING Y IS WORD.
            if (!has_word) {
                for (auto const& fr : rs.facing_rules_) {
                    if (fr.subject != noun || fr.property != Kind::P_Word) continue;
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
                        Coord fp = {o->pos.x + step(o->facing).x, o->pos.y + step(o->facing).y};
                        matched = false;
                        for (ObjectId oid : world.at(fp)) {
                            Object const* ob = world.get(oid);
                            if (ob && !ob->text && ob->kind == fr.condition) { matched = true; break; }
                        }
                    }
                    if (fr.negated ? !matched : matched) { has_word = true; break; }
                }
            }

            // 4. [NOT] POWEREDx … X IS WORD.
            if (!has_word) {
                for (auto const& gcr : rs.global_cond_rules_) {
                    if (gcr.subject != noun || gcr.property != Kind::P_Word) continue;
                    bool all_met = true;
                    for (auto const& cond : gcr.conditions) {
                        bool pw = world_has_power(cond.power_kind);
                        if (cond.negated ? pw : !pw) { all_met = false; break; }
                    }
                    if (all_met) { has_word = true; break; }
                }
            }

            // Record first WORD object kind at this coord (text takes priority via text_kind_at).
            if (has_word) word_at.emplace(o->pos, noun);
        }
    }
    if (!word_at.empty()) {
        // Re-scan all cells that contain text OR a WORD object.
        // Duplicate rules from re-scanning text-only cells are removed in dedup below.
        for (Coord c : cells) {
            bool has_text_or_word = word_at.count(c) > 0;
            if (!has_text_or_word) {
                for (ObjectId id : world.at(c)) {
                    Object const* o = world.get(id);
                    if (o && o->text) { has_text_or_word = true; break; }
                }
            }
            if (!has_text_or_word) continue;
            scan_strip(world, c, {1, 0}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.cond_eats_, rs.facing_rules_, rs.facing_transforms_, rs.global_cond_rules_, rs.global_cond_eat_rules_, rs.global_cond_make_rules_, rs.global_cond_transforms_, rs.has_rules_, rs.fear_rules_, rs.play_rules_, rs.cond_play_rules_, word_at);
            scan_strip(world, c, {0, 1}, rs.rules_, rs.transforms_, rs.makes_, rs.eats_, rs.cond_rules_, rs.cond_transforms_, rs.cond_makes_, rs.cond_eats_, rs.facing_rules_, rs.facing_transforms_, rs.global_cond_rules_, rs.global_cond_eat_rules_, rs.global_cond_make_rules_, rs.global_cond_transforms_, rs.has_rules_, rs.fear_rules_, rs.play_rules_, rs.cond_play_rules_, word_at);
        }
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
        std::vector<ConditionalEatRule> deduped;
        for (auto const& er : rs.cond_eats_) {
            bool dup = false;
            for (auto const& ex : deduped) {
                if (ex.subject == er.subject && ex.target == er.target
                    && ex.clauses.size() == er.clauses.size()) {
                    bool same = true;
                    for (size_t i = 0; i < er.clauses.size(); ++i) {
                        if (er.clauses[i].negated != ex.clauses[i].negated ||
                            er.clauses[i].nouns   != ex.clauses[i].nouns)  { same = false; break; }
                    }
                    if (same) { dup = true; break; }
                }
            }
            if (!dup) deduped.push_back(er);
        }
        rs.cond_eats_ = std::move(deduped);
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
        std::vector<GlobalConditionEatRule> deduped;
        for (auto const& gcer : rs.global_cond_eat_rules_) {
            bool dup = false;
            for (auto const& ex : deduped) {
                if (ex.subject == gcer.subject && ex.target == gcer.target
                    && ex.conditions == gcer.conditions
                    && ex.on_clauses.size() == gcer.on_clauses.size()) {
                    bool same_clauses = true;
                    for (size_t i = 0; i < gcer.on_clauses.size(); ++i) {
                        if (gcer.on_clauses[i].negated != ex.on_clauses[i].negated ||
                            gcer.on_clauses[i].nouns   != ex.on_clauses[i].nouns)
                            { same_clauses = false; break; }
                    }
                    if (same_clauses) { dup = true; break; }
                }
            }
            if (!dup) deduped.push_back(gcer);
        }
        rs.global_cond_eat_rules_ = std::move(deduped);
    }
    {
        std::vector<GlobalConditionMakeRule> deduped;
        for (auto const& gcmr : rs.global_cond_make_rules_) {
            bool dup = false;
            for (auto const& ex : deduped) {
                if (ex.subject == gcmr.subject && ex.target == gcmr.target
                    && ex.conditions == gcmr.conditions
                    && ex.on_clauses.size() == gcmr.on_clauses.size()) {
                    bool same = true;
                    for (size_t i = 0; i < gcmr.on_clauses.size(); ++i) {
                        if (gcmr.on_clauses[i].negated != ex.on_clauses[i].negated ||
                            gcmr.on_clauses[i].nouns   != ex.on_clauses[i].nouns)
                            { same = false; break; }
                    }
                    if (same) { dup = true; break; }
                }
            }
            if (!dup) deduped.push_back(gcmr);
        }
        rs.global_cond_make_rules_ = std::move(deduped);
    }
    {
        std::vector<GlobalConditionTransformRule> deduped;
        for (auto const& gctr : rs.global_cond_transforms_) {
            bool dup = false;
            for (auto const& ex : deduped) {
                if (ex.subject == gctr.subject && ex.target == gctr.target
                    && ex.conditions == gctr.conditions
                    && ex.on_clauses.size() == gctr.on_clauses.size()) {
                    bool same = true;
                    for (size_t i = 0; i < gctr.on_clauses.size(); ++i) {
                        if (gctr.on_clauses[i].negated != ex.on_clauses[i].negated ||
                            gctr.on_clauses[i].nouns   != ex.on_clauses[i].nouns)
                            { same = false; break; }
                    }
                    if (same) { dup = true; break; }
                }
            }
            if (!dup) deduped.push_back(gctr);
        }
        rs.global_cond_transforms_ = std::move(deduped);
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
    {
        std::vector<ConditionalPlayRule> deduped;
        for (auto const& pr : rs.cond_play_rules_) {
            bool dup = false;
            for (auto const& ex : deduped) {
                if (ex.subject == pr.subject && ex.note == pr.note &&
                    ex.octave == pr.octave && ex.sharp == pr.sharp && ex.flat == pr.flat &&
                    ex.clauses.size() == pr.clauses.size()) {
                    bool same = true;
                    for (size_t i = 0; i < pr.clauses.size(); ++i) {
                        if (pr.clauses[i].negated != ex.clauses[i].negated ||
                            pr.clauses[i].nouns   != ex.clauses[i].nouns)  { same = false; break; }
                    }
                    if (same) { dup = true; break; }
                }
            }
            if (!dup) deduped.push_back(pr);
        }
        rs.cond_play_rules_ = std::move(deduped);
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

    // Build granted_props_: every property Kind potentially grantable by any rule.
    for (auto const& r   : rs.rules_)            rs.granted_props_.insert(r.property);
    for (auto const& cr  : rs.cond_rules_)        rs.granted_props_.insert(cr.property);
    for (auto const& fr  : rs.facing_rules_)      rs.granted_props_.insert(fr.property);
    for (auto const& gcr : rs.global_cond_rules_) rs.granted_props_.insert(gcr.property);

    // Subjects-per-property: for each property a rule could grant, union of
    // subject kinds across all rule forms. Ascending-sorted for deterministic
    // iteration.
    {
        std::unordered_map<Kind, std::unordered_set<Kind>> acc;
        for (auto const& r   : rs.rules_)            acc[r.property].insert(r.subject);
        for (auto const& cr  : rs.cond_rules_)        acc[cr.property].insert(cr.subject);
        for (auto const& fr  : rs.facing_rules_)      acc[fr.property].insert(fr.subject);
        for (auto const& gcr : rs.global_cond_rules_) acc[gcr.property].insert(gcr.subject);
        for (auto const& [p, s] : acc) {
            auto& v = rs.subjects_per_property_[p];
            v.assign(s.begin(), s.end());
            std::sort(v.begin(), v.end());
        }
    }

    // Hot-path indices for object_has_property: per-(subject, property) vec
    // of rule indices into cond_rules_ / facing_rules_ / global_cond_rules_.
    for (std::uint32_t i = 0; i < rs.cond_rules_.size(); ++i) {
        auto const& cr = rs.cond_rules_[i];
        auto k = key_(cr.subject, cr.property);
        rs.cond_any_.insert(k);
        rs.cond_idx_[k].push_back(i);
    }
    for (std::uint32_t i = 0; i < rs.facing_rules_.size(); ++i) {
        auto const& fr = rs.facing_rules_[i];
        auto k = key_(fr.subject, fr.property);
        rs.cond_any_.insert(k);
        rs.facing_idx_[k].push_back(i);
    }
    for (std::uint32_t i = 0; i < rs.global_cond_rules_.size(); ++i) {
        auto const& gcr = rs.global_cond_rules_[i];
        auto k = key_(gcr.subject, gcr.property);
        rs.cond_any_.insert(k);
        rs.global_idx_[k].push_back(i);
    }

    // Subjects-per-verb (Transform / Make / Eat / Has / Fear / Play).
    {
        auto build = [](RuleSet::Verb, std::unordered_set<Kind>& acc,
                        std::vector<Kind>& out) {
            out.assign(acc.begin(), acc.end());
            std::sort(out.begin(), out.end());
        };
        std::unordered_set<Kind> xform, make, eat, has, fear, play;
        for (auto const& r : rs.transforms_)             xform.insert(r.from);
        for (auto const& r : rs.cond_transforms_)        xform.insert(r.subject);
        for (auto const& r : rs.facing_transforms_)      xform.insert(r.subject);
        for (auto const& r : rs.global_cond_transforms_) xform.insert(r.subject);
        for (auto const& r : rs.makes_)                  make.insert(r.from);
        for (auto const& r : rs.cond_makes_)             make.insert(r.subject);
        for (auto const& r : rs.global_cond_make_rules_) make.insert(r.subject);
        for (auto const& r : rs.eats_)                   eat.insert(r.subject);
        for (auto const& r : rs.cond_eats_)              eat.insert(r.subject);
        for (auto const& r : rs.global_cond_eat_rules_)  eat.insert(r.subject);
        for (auto const& r : rs.has_rules_)              has.insert(r.subject);
        for (auto const& r : rs.fear_rules_)             fear.insert(r.subject);
        for (auto const& r : rs.play_rules_)             play.insert(r.subject);
        for (auto const& r : rs.cond_play_rules_)        play.insert(r.subject);
        build(RuleSet::Verb::Transform, xform, rs.subjects_per_verb_[0]);
        build(RuleSet::Verb::Make,      make,  rs.subjects_per_verb_[1]);
        build(RuleSet::Verb::Eat,       eat,   rs.subjects_per_verb_[2]);
        build(RuleSet::Verb::Has,       has,   rs.subjects_per_verb_[3]);
        build(RuleSet::Verb::Fear,      fear,  rs.subjects_per_verb_[4]);
        build(RuleSet::Verb::Play,      play,  rs.subjects_per_verb_[5]);
    }

    return rs;
}

std::vector<Kind> const& RuleSet::subjects_for_property(Kind property) const {
    static std::vector<Kind> const kEmpty;
    auto it = subjects_per_property_.find(property);
    return (it == subjects_per_property_.end()) ? kEmpty : it->second;
}

std::vector<Kind> const& RuleSet::subjects_for_verb(Verb v) const {
    return subjects_per_verb_[static_cast<std::size_t>(v)];
}

bool RuleSet::has_property(Kind noun, Kind property) const {
    if (index_.count(key_(noun, property))) return true;
    return false;
}

bool RuleSet::object_has_property(World const& world, ObjectId id, Kind property) const {
    Object const* o = world.get(id);
    if (!o) return false;
    Kind noun = o->text ? Kind::N_Text : o->kind;
    auto k = key_(noun, property);

    // Fast path 1: unconditional positive rule. O(1).
    if (index_.count(k)) return true;

    // Text objects only ever match TEXT IS <prop> unconditionally (already
    // handled above); all conditional/facing/global rules apply to nouns.
    if (o->text) return false;

    // Fast path 2: no conditional/facing/global rule with this (subject, prop).
    // Single O(1) probe rejects the overwhelming-majority "not applicable" case.
    if (!cond_any_.count(k)) return false;

    // Slow path: only rules matching (subject, property) are evaluated.
    if (auto it = cond_idx_.find(k); it != cond_idx_.end()) {
        for (std::uint32_t idx : it->second) {
            if (eval_cond_clauses(world, id, cond_rules_[idx].clauses)) return true;
        }
    }

    if (auto it = facing_idx_.find(k); it != facing_idx_.end()) {
        for (std::uint32_t idx : it->second) {
            auto const& fr = facing_rules_[idx];
            bool cond_is_dir = (fr.condition == Kind::P_Left || fr.condition == Kind::P_Right ||
                                fr.condition == Kind::P_Up   || fr.condition == Kind::P_Down);
            bool matched;
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
    }

    if (auto it = global_idx_.find(k); it != global_idx_.end()) {
        for (std::uint32_t idx : it->second) {
            auto const& gcr = global_cond_rules_[idx];
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
    // Only kinds that could ever grant this power (via any rule form) need
    // checking — comes from the parse-time subjects_for_property cache.
    auto const& candidates = subjects_for_property(power_prop);
    if (candidates.empty()) return false;

    for (Kind k : candidates) {
        // Unconditional X IS POWERx — a single index probe covers every
        // object of this kind at once.
        if (index_.count(key_(k, power_prop))) {
            if (!world.objects_of_kind(k).empty()) return true;
        }
        // Conditional via ON/FACEDBY. Evaluate per-object because it depends
        // on cell or neighbor contents. Never recurse into global_cond_rules_.
        bool kind_has_cond = false;
        for (auto const& cr : cond_rules_)
            if (cr.subject == k && cr.property == power_prop) { kind_has_cond = true; break; }
        if (!kind_has_cond) continue;

        for (ObjectId id : world.objects_of_kind(k)) {
            for (auto const& cr : cond_rules_) {
                if (cr.subject != k || cr.property != power_prop) continue;
                if (eval_cond_clauses(world, id, cr.clauses)) return true;
            }
        }
    }
    return false;
}

// static
bool RuleSet::eval_cond_clauses(World const& world, ObjectId id,
                                std::vector<CondClause> const& clauses) {
    Object const* o = world.get(id);
    if (!o) return false;
    for (auto const& clause : clauses) {
        bool all_found = true;
        if (clause.ctype == CondType::On) {
            // Co-location: ALL nouns must be on the same tile as the subject.
            // Duplicate nouns in the list (e.g. [BABA, BABA]) require that many
            // *distinct* objects of that kind are present — each object may only
            // satisfy one slot (wiki: "FLAG ON BABA AND BABA IS WIN fires only
            // when FLAG is on two babas").
            std::unordered_set<ObjectId> used;
            for (Kind cn : clause.nouns) {
                bool found = false;
                for (ObjectId other : world.at(o->pos)) {
                    if (other == id) continue;
                    if (used.count(other)) continue;
                    Object const* ob = world.get(other);
                    if (ob && !ob->text && ob->kind == cn) {
                        used.insert(other);
                        found = true;
                        break;
                    }
                }
                if (!found) { all_found = false; break; }
            }
        } else {
            // FacedBy: for each noun, some object of that kind on an adjacent tile
            // must be facing toward the subject.
            // Single objects_of_kind() probe per noun (one hash lookup vs 4 for
            // the 4-tile scan); aimed==o->pos enforces both adjacency and direction.
            std::unordered_set<ObjectId> used;
            for (Kind cn : clause.nouns) {
                bool found = false;
                for (ObjectId other : world.objects_of_kind(cn)) {
                    if (used.count(other)) continue;
                    Object const* ob = world.get(other);
                    if (!ob || ob->text) continue;
                    Coord aimed = {ob->pos.x + step(ob->facing).x,
                                   ob->pos.y + step(ob->facing).y};
                    if (aimed.x == o->pos.x && aimed.y == o->pos.y) {
                        used.insert(other);
                        found = true;
                        break;
                    }
                }
                if (!found) { all_found = false; break; }
            }
        }
        bool clause_pass = clause.negated ? !all_found : all_found;
        if (!clause_pass) return false;
    }
    return true;
}

}  // namespace baba::core
