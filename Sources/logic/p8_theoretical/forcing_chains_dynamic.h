// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: forcing_chains_dynamic.h (Poziom 8 - Theoretical)
// Opis: Algorytmy Forcing Chains oraz Dynamic Forcing Chains.
//       Eksploracja zaĹ‚oĹĽeĹ„ z wymuszaniem i zagnieĹĽdĹĽonym Ĺ›rodowiskiem symulacji.
//       RozwiÄ…zanie Zero-Allocation operujÄ…ce na tablicach "zrzutĂłw" 
//       znajdujÄ…cych siÄ™ we wspĂłĹ‚dzielonym Scratchpadzie (do gĹ‚Ä™bokoĹ›ci N).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"
#include "../shared/required_strategy_gate.h"
#include "../shared/state_probe.h"

// DoĹ‚Ä…czamy komponent wykorzystywany do silnej weryfikacji powiÄ…zaĹ„ w DFC
#include "../p7_nightmare/aic_grouped_aic.h"

namespace sudoku_hpc::logic::p8_theoretical {

enum class ForcingAssertionKind : uint8_t {
    CellValue = 0,
    HouseDigit = 1
};

struct ForcingAssertionGroup {
    ForcingAssertionKind kind = ForcingAssertionKind::CellValue;
    int digit = 0;
    int house = -1;
    int option_count = 0;
    int options[8]{};
};

inline bool forcing_group_has_option(
    const ForcingAssertionGroup& group,
    int cell) {
    for (int i = 0; i < group.option_count; ++i) {
        if (group.options[i] == cell) return true;
    }
    return false;
}

inline bool forcing_groups_adjacent(
    const CandidateState& st,
    const ForcingAssertionGroup& a,
    const ForcingAssertionGroup& b) {
    if (a.digit == b.digit) {
        if (a.kind == ForcingAssertionKind::CellValue && b.kind == ForcingAssertionKind::HouseDigit) {
            return forcing_group_has_option(b, a.options[0]) || forcing_group_has_option(b, a.options[1]);
        }
        if (a.kind == ForcingAssertionKind::HouseDigit && b.kind == ForcingAssertionKind::CellValue) {
            return forcing_group_has_option(a, b.options[0]) || forcing_group_has_option(a, b.options[1]);
        }
        if (a.kind == ForcingAssertionKind::HouseDigit && b.kind == ForcingAssertionKind::HouseDigit) {
            if (a.house == b.house) return true;
            for (int i = 0; i < a.option_count; ++i) {
                if (forcing_group_has_option(b, a.options[i])) return true;
            }
        }
    }

    for (int i = 0; i < a.option_count; ++i) {
        const int ca = a.options[i];
        for (int j = 0; j < b.option_count; ++j) {
            const int cb = b.options[j];
            if (ca == cb) return true;
            if (st.topo->cell_row[ca] == st.topo->cell_row[cb]) return true;
            if (st.topo->cell_col[ca] == st.topo->cell_col[cb]) return true;
            if (st.topo->cell_box[ca] == st.topo->cell_box[cb]) return true;
        }
    }
    return false;
}

inline void forcing_build_assertion_adjacency(
    const CandidateState& st,
    const ForcingAssertionGroup* groups,
    int group_count,
    int* offsets,
    int* degree,
    int* adj,
    int adj_cap) {
    for (int i = 0; i < group_count; ++i) degree[i] = 0;
    for (int i = 0; i < group_count; ++i) {
        for (int j = i + 1; j < group_count; ++j) {
            if (!forcing_groups_adjacent(st, groups[i], groups[j])) continue;
            ++degree[i];
            ++degree[j];
        }
    }

    int total = 0;
    for (int i = 0; i < group_count; ++i) {
        offsets[i] = total;
        total += degree[i];
    }
    offsets[group_count] = total;
    if (total > adj_cap) {
        for (int i = 0; i <= group_count; ++i) offsets[i] = 0;
        for (int i = 0; i < group_count; ++i) degree[i] = 0;
        return;
    }

    int cursor[128]{};
    for (int i = 0; i < group_count; ++i) {
        cursor[i] = offsets[i];
        degree[i] = 0;
    }
    for (int i = 0; i < group_count; ++i) {
        for (int j = i + 1; j < group_count; ++j) {
            if (!forcing_groups_adjacent(st, groups[i], groups[j])) continue;
            adj[cursor[i]++] = j;
            adj[cursor[j]++] = i;
            ++degree[i];
            ++degree[j];
        }
    }
}

inline ApplyResult forcing_apply_intersection_elims(
    CandidateState& st,
    const uint64_t* inter_cands);

inline bool forcing_assert_candidate_true(
    CandidateState& st,
    int cell,
    int digit,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    shared::restore_state(st, sp);
    if (!st.place(cell, digit)) return false;
    return shared::propagate_singles(st, max_steps);
}

inline void forcing_capture_intersection(
    CandidateState& st,
    uint64_t* inter_cands) {
    const int nn = st.topo->nn;
    for (int i = 0; i < nn; ++i) {
        uint64_t cur = st.cands[i];
        if (st.board->values[i] != 0) cur = (1ULL << (st.board->values[i] - 1));
        inter_cands[i] &= cur;
    }
}

inline ApplyResult forcing_run_assertion_group(
    CandidateState& st,
    const ForcingAssertionGroup& group,
    int max_steps,
    bool& used_flag) {
    const int nn = st.topo->nn;
    auto& sp = shared::exact_pattern_scratchpad();
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
    for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;

    const uint64_t bit = (1ULL << (group.digit - 1));
    int contradiction_count = 0;
    int contradiction_cells[8]{};
    int valid_branches = 0;

    shared::snapshot_state(st, sp);
    for (int i = 0; i < group.option_count; ++i) {
        const int cell = group.options[i];
        shared::restore_state(st, sp);
        const bool valid = forcing_assert_candidate_true(st, cell, group.digit, max_steps, sp);
        if (!valid) {
            contradiction_cells[contradiction_count++] = cell;
            continue;
        }
        ++valid_branches;
        forcing_capture_intersection(st, inter_cands);
    }
    shared::restore_state(st, sp);

    for (int i = 0; i < contradiction_count; ++i) {
        const ApplyResult er = st.eliminate(contradiction_cells[i], bit);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) {
            used_flag = true;
            return er;
        }
    }

    if (valid_branches >= 2) {
        const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) {
            used_flag = true;
            return er;
        }
    }

    return ApplyResult::NoProgress;
}

inline int forcing_collect_assertion_groups(
    CandidateState& st,
    int digit_cap,
    int link_cap_per_digit,
    int house_cap,
    int house_max_places,
    ForcingAssertionGroup* groups,
    int group_cap) {
    const int n = st.topo->n;
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    auto& sp = shared::exact_pattern_scratchpad();
    int tested_digits = 0;
    int count = 0;

    for (int d = 1; d <= n && count < group_cap; ++d) {
        if (tested_digits >= digit_cap) break;
        const uint64_t bit = (1ULL << (d - 1));
        bool any = false;

        if (shared::build_grouped_link_graph_for_digit(st, d, sp) && sp.dyn_strong_edge_count > 0) {
            ++tested_digits;
            any = true;
            int tested_links = 0;
            for (int e = 0; e < sp.dyn_strong_edge_count && count < group_cap; ++e) {
                if (tested_links >= link_cap_per_digit) break;
                const int a_cell = sp.dyn_node_to_cell[sp.dyn_strong_edge_u[e]];
                const int b_cell = sp.dyn_node_to_cell[sp.dyn_strong_edge_v[e]];
                if (st.board->values[a_cell] != 0 || st.board->values[b_cell] != 0) continue;
                if ((st.cands[a_cell] & bit) == 0ULL || (st.cands[b_cell] & bit) == 0ULL) continue;
                ForcingAssertionGroup& g = groups[count++];
                g.kind = ForcingAssertionKind::CellValue;
                g.digit = d;
                g.house = -1;
                g.option_count = 2;
                g.options[0] = a_cell;
                g.options[1] = b_cell;
                ++tested_links;
            }
        }

        int tested_houses = 0;
        for (int h = 0; h < house_count && count < group_cap; ++h) {
            if (tested_houses >= house_cap) break;
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int places[8]{};
            int place_count = 0;
            for (int p = p0; p < p1 && place_count < house_max_places + 1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                places[place_count++] = idx;
            }
            if (place_count < 2 || place_count > house_max_places) continue;
            if (!any) {
                ++tested_digits;
                any = true;
                if (tested_digits > digit_cap) break;
            }
            ForcingAssertionGroup& g = groups[count++];
            g.kind = ForcingAssertionKind::HouseDigit;
            g.digit = d;
            g.house = h;
            g.option_count = place_count;
            for (int i = 0; i < place_count; ++i) g.options[i] = places[i];
            ++tested_houses;
        }
    }

    return count;
}

inline ApplyResult forcing_assertion_graph_pass(
    CandidateState& st,
    int digit_cap,
    int link_cap_per_digit,
    int house_cap,
    int house_max_places,
    int max_steps,
    bool& used_flag) {
    ForcingAssertionGroup groups[128]{};
    int offsets[129]{};
    int degree[128]{};
    int adj[1024]{};
    const int group_count = forcing_collect_assertion_groups(
        st, digit_cap, link_cap_per_digit, house_cap, house_max_places, groups, 128);
    forcing_build_assertion_adjacency(st, groups, group_count, offsets, degree, adj, 1024);
    for (int i = 0; i < group_count; ++i) {
        const ApplyResult er = forcing_run_assertion_group(st, groups[i], max_steps, used_flag);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_strong_link_assertion_pass(
    CandidateState& st,
    int digit_cap,
    int link_cap_per_digit,
    int max_steps,
    bool& used_flag) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    auto& sp = shared::exact_pattern_scratchpad();
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
    int tested_digits = 0;

    for (int d = 1; d <= n; ++d) {
        if (tested_digits >= digit_cap) return ApplyResult::NoProgress;
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_strong_edge_count == 0) continue;
        ++tested_digits;

        const uint64_t bit = (1ULL << (d - 1));
        int tested_links = 0;
        for (int e = 0; e < sp.dyn_strong_edge_count; ++e) {
            if (tested_links >= link_cap_per_digit) break;
            ++tested_links;

            const int a_node = sp.dyn_strong_edge_u[e];
            const int b_node = sp.dyn_strong_edge_v[e];
            const int a_cell = sp.dyn_node_to_cell[a_node];
            const int b_cell = sp.dyn_node_to_cell[b_node];
            if (st.board->values[a_cell] != 0 || st.board->values[b_cell] != 0) continue;
            if ((st.cands[a_cell] & bit) == 0ULL || (st.cands[b_cell] & bit) == 0ULL) continue;

            shared::snapshot_state(st, sp);
            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;

            const bool a_valid = forcing_assert_candidate_true(st, a_cell, d, max_steps, sp);
            if (a_valid) forcing_capture_intersection(st, inter_cands);

            const bool b_valid = forcing_assert_candidate_true(st, b_cell, d, max_steps, sp);
            if (b_valid) forcing_capture_intersection(st, inter_cands);

            shared::restore_state(st, sp);

            if (!a_valid && !b_valid) {
                return ApplyResult::Contradiction;
            }
            if (!a_valid) {
                const ApplyResult er = st.eliminate(a_cell, bit);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }
            if (!b_valid) {
                const ApplyResult er = st.eliminate(b_cell, bit);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }
            if (a_valid && b_valid) {
                const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_house_digit_assertion_pass(
    CandidateState& st,
    int house_cap,
    int max_places,
    int max_steps,
    bool& used_flag) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    auto& sp = shared::exact_pattern_scratchpad();
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
    int tested_houses = 0;

    for (int h = 0; h < house_count; ++h) {
        if (tested_houses >= house_cap) return ApplyResult::NoProgress;
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
        bool house_used = false;

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int places[8]{};
            int place_count = 0;
            for (int p = p0; p < p1 && place_count < max_places + 1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                places[place_count++] = idx;
            }
            if (place_count < 2 || place_count > max_places) continue;
            if (!house_used) {
                ++tested_houses;
                house_used = true;
            }

            shared::snapshot_state(st, sp);
            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
            int contradiction_count = 0;
            int contradiction_cells[8]{};
            int valid_branches = 0;

            for (int i = 0; i < place_count; ++i) {
                const int cell = places[i];
                const bool valid = forcing_assert_candidate_true(st, cell, d, max_steps, sp);
                if (valid) {
                    ++valid_branches;
                    forcing_capture_intersection(st, inter_cands);
                } else {
                    contradiction_cells[contradiction_count++] = cell;
                }
            }

            shared::restore_state(st, sp);

            for (int i = 0; i < contradiction_count; ++i) {
                const ApplyResult er = st.eliminate(contradiction_cells[i], bit);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }

            if (valid_branches >= 2) {
                const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_dynamic_strong_link_pass(
    CandidateState& st,
    int digit_cap,
    int outer_link_cap,
    int inner_link_cap,
    int max_steps,
    bool& used_flag) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    auto& sp = shared::exact_pattern_scratchpad();
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
    uint64_t nested_inter[shared::ExactPatternScratchpad::MAX_NN]{};
    int tested_digits = 0;

    for (int d = 1; d <= n; ++d) {
        if (tested_digits >= digit_cap) return ApplyResult::NoProgress;
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_strong_edge_count == 0) continue;
        ++tested_digits;

        int outer_tested = 0;
        for (int e = 0; e < sp.dyn_strong_edge_count; ++e) {
            if (outer_tested >= outer_link_cap) break;
            ++outer_tested;

            const int a_node = sp.dyn_strong_edge_u[e];
            const int b_node = sp.dyn_strong_edge_v[e];
            const int a_cell = sp.dyn_node_to_cell[a_node];
            const int b_cell = sp.dyn_node_to_cell[b_node];
            const uint64_t bit = (1ULL << (d - 1));
            if ((st.cands[a_cell] & bit) == 0ULL || (st.cands[b_cell] & bit) == 0ULL) continue;

            shared::snapshot_state(st, sp);
            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
            uint64_t contradiction_mask = 0ULL;
            int valid_outer = 0;

            const int outer_cells[2] = {a_cell, b_cell};
            for (int oi = 0; oi < 2; ++oi) {
                const int outer_cell = outer_cells[oi];
                const bool outer_valid = forcing_assert_candidate_true(st, outer_cell, d, max_steps, sp);
                if (!outer_valid) {
                    contradiction_mask |= bit;
                    continue;
                }

                for (int i = 0; i < nn; ++i) nested_inter[i] = ~0ULL;
                bool nested_used = false;
                int nested_done = 0;

                for (int d2 = 1; d2 <= n && nested_done < inner_link_cap; ++d2) {
                    if (!shared::build_grouped_link_graph_for_digit(st, d2, sp)) continue;
                    if (sp.dyn_strong_edge_count == 0) continue;
                    const uint64_t bit2 = (1ULL << (d2 - 1));

                    for (int e2 = 0; e2 < sp.dyn_strong_edge_count && nested_done < inner_link_cap; ++e2) {
                        const int u_cell = sp.dyn_node_to_cell[sp.dyn_strong_edge_u[e2]];
                        const int v_cell = sp.dyn_node_to_cell[sp.dyn_strong_edge_v[e2]];
                        if ((st.cands[u_cell] & bit2) == 0ULL || (st.cands[v_cell] & bit2) == 0ULL) continue;
                        ++nested_done;

                        shared::snapshot_state(st, sp);
                        bool u_valid = forcing_assert_candidate_true(st, u_cell, d2, max_steps, sp);
                        if (u_valid) forcing_capture_intersection(st, nested_inter);
                        bool v_valid = forcing_assert_candidate_true(st, v_cell, d2, max_steps, sp);
                        if (v_valid) forcing_capture_intersection(st, nested_inter);
                        shared::restore_state(st, sp);

                        if (u_valid || v_valid) {
                            nested_used = true;
                            break;
                        }
                    }
                }

                shared::restore_state(st, sp);
                if (!forcing_assert_candidate_true(st, outer_cell, d, max_steps, sp)) continue;
                ++valid_outer;
                if (nested_used) {
                    for (int i = 0; i < nn; ++i) inter_cands[i] &= nested_inter[i];
                } else {
                    forcing_capture_intersection(st, inter_cands);
                }
            }

            shared::restore_state(st, sp);

            if (contradiction_mask != 0ULL) {
                const ApplyResult er = st.eliminate(a_cell, contradiction_mask);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
                const ApplyResult er2 = st.eliminate(b_cell, contradiction_mask);
                if (er2 == ApplyResult::Contradiction) return er2;
                if (er2 == ApplyResult::Progress) {
                    used_flag = true;
                    return er2;
                }
            }

            if (valid_outer >= 2) {
                const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_dynamic_house_digit_pass(
    CandidateState& st,
    int house_cap,
    int outer_max_places,
    int inner_max_places,
    int max_steps,
    bool& used_flag) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    auto& sp = shared::exact_pattern_scratchpad();
    auto* inter_cands = shared::intersection_slot(sp, 0);
    auto* nested_inter = shared::intersection_slot(sp, 1);
    auto* root_cands = sp.p8_cands_backup[0];
    auto* root_values = sp.p8_values_backup[0];
    auto* root_row_used = sp.p8_row_used_backup[0];
    auto* root_col_used = sp.p8_col_used_backup[0];
    auto* root_box_used = sp.p8_box_used_backup[0];
    auto* branch_cands = sp.p8_cands_backup[1];
    auto* branch_values = sp.p8_values_backup[1];
    auto* branch_row_used = sp.p8_row_used_backup[1];
    auto* branch_col_used = sp.p8_col_used_backup[1];
    auto* branch_box_used = sp.p8_box_used_backup[1];
    int tested_houses = 0;

    for (int h = 0; h < house_count; ++h) {
        if (tested_houses >= house_cap) return ApplyResult::NoProgress;
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
        bool house_used = false;

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int places[8]{};
            int place_count = 0;
            for (int p = p0; p < p1 && place_count < outer_max_places + 1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                places[place_count++] = idx;
            }
            if (place_count < 2 || place_count > outer_max_places) continue;
            if (!house_used) {
                ++tested_houses;
                house_used = true;
            }

            std::copy_n(st.cands, nn, root_cands);
            std::copy_n(st.board->values.data(), nn, root_values);
            std::copy_n(st.board->row_used.data(), n, root_row_used);
            std::copy_n(st.board->col_used.data(), n, root_col_used);
            std::copy_n(st.board->box_used.data(), n, root_box_used);
            const int root_empty = st.board->empty_cells;
            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
            int contradiction_count = 0;
            int contradiction_cells[8]{};
            int valid_outer = 0;

            for (int i = 0; i < place_count; ++i) {
                const int outer_cell = places[i];
                std::copy_n(root_cands, nn, st.cands);
                std::copy_n(root_values, nn, st.board->values.data());
                std::copy_n(root_row_used, n, st.board->row_used.data());
                std::copy_n(root_col_used, n, st.board->col_used.data());
                std::copy_n(root_box_used, n, st.board->box_used.data());
                st.board->empty_cells = root_empty;

                if (!st.place(outer_cell, d) || !shared::propagate_singles(st, max_steps)) {
                    contradiction_cells[contradiction_count++] = outer_cell;
                    continue;
                }

                std::copy_n(st.cands, nn, branch_cands);
                std::copy_n(st.board->values.data(), nn, branch_values);
                std::copy_n(st.board->row_used.data(), n, branch_row_used);
                std::copy_n(st.board->col_used.data(), n, branch_col_used);
                std::copy_n(st.board->box_used.data(), n, branch_box_used);
                const int branch_empty = st.board->empty_cells;

                for (int k = 0; k < nn; ++k) nested_inter[k] = ~0ULL;
                bool nested_used = false;

                const int nested_house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
                for (int h2 = 0; h2 < nested_house_count && !nested_used; ++h2) {
                    const int q0 = st.topo->house_offsets[static_cast<size_t>(h2)];
                    const int q1 = st.topo->house_offsets[static_cast<size_t>(h2 + 1)];
                    for (int d2 = 1; d2 <= n && !nested_used; ++d2) {
                        const uint64_t bit2 = (1ULL << (d2 - 1));
                        int inner_places[8]{};
                        int inner_count = 0;
                        for (int q = q0; q < q1 && inner_count < inner_max_places + 1; ++q) {
                            const int idx = st.topo->houses_flat[static_cast<size_t>(q)];
                            if (st.board->values[idx] != 0) continue;
                            if ((st.cands[idx] & bit2) == 0ULL) continue;
                            inner_places[inner_count++] = idx;
                        }
                        if (inner_count < 2 || inner_count > inner_max_places) continue;

                        int inner_valid = 0;
                        for (int ii = 0; ii < inner_count; ++ii) {
                            const int inner_cell = inner_places[ii];
                            std::copy_n(branch_cands, nn, st.cands);
                            std::copy_n(branch_values, nn, st.board->values.data());
                            std::copy_n(branch_row_used, n, st.board->row_used.data());
                            std::copy_n(branch_col_used, n, st.board->col_used.data());
                            std::copy_n(branch_box_used, n, st.board->box_used.data());
                            st.board->empty_cells = branch_empty;
                            const bool valid = st.place(inner_cell, d2) && shared::propagate_singles(st, max_steps);
                            if (!valid) continue;
                            ++inner_valid;
                            forcing_capture_intersection(st, nested_inter);
                        }
                        if (inner_valid >= 2) nested_used = true;
                    }
                }

                ++valid_outer;
                if (nested_used) {
                    for (int k = 0; k < nn; ++k) inter_cands[k] &= nested_inter[k];
                } else {
                    for (int k = 0; k < nn; ++k) {
                        uint64_t cur = branch_cands[k];
                        if (branch_values[k] != 0) cur = (1ULL << (branch_values[k] - 1));
                        inter_cands[k] &= cur;
                    }
                }
            }

            std::copy_n(root_cands, nn, st.cands);
            std::copy_n(root_values, nn, st.board->values.data());
            std::copy_n(root_row_used, n, st.board->row_used.data());
            std::copy_n(root_col_used, n, st.board->col_used.data());
            std::copy_n(root_box_used, n, st.board->box_used.data());
            st.board->empty_cells = root_empty;

            for (int i = 0; i < contradiction_count; ++i) {
                const ApplyResult er = st.eliminate(contradiction_cells[i], bit);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }

            if (valid_outer >= 2) {
                const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return er;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_dynamic_assertion_graph_pass(
    CandidateState& st,
    int digit_cap,
    int link_cap_per_digit,
    int house_cap,
    int house_max_places,
    int inner_house_cap,
    int max_steps,
    bool& used_flag) {
    const int nn = st.topo->nn;
    auto& sp = shared::exact_pattern_scratchpad();
    ForcingAssertionGroup outer_groups[128]{};
    ForcingAssertionGroup inner_groups[128]{};
    int offsets[129]{};
    int degree[128]{};
    int adj[1024]{};
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
    uint64_t nested_inter[shared::ExactPatternScratchpad::MAX_NN]{};

    const int outer_count = forcing_collect_assertion_groups(
        st, digit_cap, link_cap_per_digit, house_cap, house_max_places, outer_groups, 128);
    forcing_build_assertion_adjacency(st, outer_groups, outer_count, offsets, degree, adj, 1024);
    for (int gi = 0; gi < outer_count; ++gi) {
        const ForcingAssertionGroup& outer = outer_groups[gi];
        shared::snapshot_state(st, sp);
        for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
        int contradiction_count = 0;
        int contradiction_cells[8]{};
        int valid_outer = 0;

        for (int oi = 0; oi < outer.option_count; ++oi) {
            const int outer_cell = outer.options[oi];
            shared::restore_state(st, sp);
            const bool outer_valid = forcing_assert_candidate_true(st, outer_cell, outer.digit, max_steps, sp);
            if (!outer_valid) {
                contradiction_cells[contradiction_count++] = outer_cell;
                continue;
            }

            for (int i = 0; i < nn; ++i) nested_inter[i] = ~0ULL;
            bool nested_used = false;
            int nested_checked = 0;
            for (int p = offsets[gi]; p < offsets[gi + 1] && nested_checked < inner_house_cap; ++p) {
                const int ii = adj[p];
                const ForcingAssertionGroup& inner = outer_groups[ii];
                ++nested_checked;
                shared::snapshot_state(st, sp);
                int inner_valid = 0;
                for (int opt = 0; opt < inner.option_count; ++opt) {
                    const int inner_cell = inner.options[opt];
                    shared::restore_state(st, sp);
                    const bool valid = forcing_assert_candidate_true(st, inner_cell, inner.digit, max_steps, sp);
                    if (!valid) continue;
                    ++inner_valid;
                    forcing_capture_intersection(st, nested_inter);
                }
                shared::restore_state(st, sp);
                if (inner_valid >= 2) {
                    nested_used = true;
                    break;
                }
            }

            if (!nested_used) {
                const int inner_count = forcing_collect_assertion_groups(
                    st, std::max(2, digit_cap / 2), std::max(2, link_cap_per_digit / 2),
                    inner_house_cap, house_max_places, inner_groups, 128);
                for (int ii = 0; ii < inner_count; ++ii) {
                    const ForcingAssertionGroup& inner = inner_groups[ii];
                    if (inner.digit == outer.digit && inner.option_count == outer.option_count) {
                        bool same_group = true;
                        for (int k = 0; k < inner.option_count; ++k) {
                            if (inner.options[k] != outer.options[k]) {
                                same_group = false;
                                break;
                            }
                        }
                        if (same_group) continue;
                    }

                    shared::snapshot_state(st, sp);
                    int inner_valid = 0;
                    for (int opt = 0; opt < inner.option_count; ++opt) {
                        const int inner_cell = inner.options[opt];
                        shared::restore_state(st, sp);
                        const bool valid = forcing_assert_candidate_true(st, inner_cell, inner.digit, max_steps, sp);
                        if (!valid) continue;
                        ++inner_valid;
                        forcing_capture_intersection(st, nested_inter);
                    }
                    shared::restore_state(st, sp);
                    if (inner_valid >= 2) {
                        nested_used = true;
                        break;
                    }
                }
            }

            shared::restore_state(st, sp);
            if (!forcing_assert_candidate_true(st, outer_cell, outer.digit, max_steps, sp)) continue;
            ++valid_outer;
            if (nested_used) {
                for (int i = 0; i < nn; ++i) inter_cands[i] &= nested_inter[i];
            } else {
                forcing_capture_intersection(st, inter_cands);
            }
        }

        shared::restore_state(st, sp);

        const uint64_t bit = (1ULL << (outer.digit - 1));
        for (int i = 0; i < contradiction_count; ++i) {
            const ApplyResult er = st.eliminate(contradiction_cells[i], bit);
            if (er == ApplyResult::Contradiction) return er;
            if (er == ApplyResult::Progress) {
                used_flag = true;
                return er;
            }
        }

        if (valid_outer >= 2) {
            const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
            if (er == ApplyResult::Contradiction) return er;
            if (er == ApplyResult::Progress) {
                used_flag = true;
                return er;
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_apply_intersection_elims(
    CandidateState& st,
    const uint64_t* inter_cands) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;

    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        uint64_t rm = st.cands[idx] & ~inter_cands[idx];
        while (rm != 0ULL) {
            const uint64_t bit = config::bit_lsb(rm);
            rm = config::bit_clear_lsb_u64(rm);
            const ApplyResult er = st.eliminate(idx, bit);
            if (er != ApplyResult::NoProgress) return er;
        }
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    for (int h = 0; h < house_count; ++h) {
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int live_places = 0;
            int inter_places = 0;
            int only_idx = -1;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) != 0ULL) ++live_places;
                if ((inter_cands[idx] & bit) != 0ULL) {
                    ++inter_places;
                    only_idx = idx;
                }
            }
            if (inter_places == 1 && live_places > 1 && only_idx >= 0) {
                uint64_t drop = st.cands[only_idx] & ~bit;
                while (drop != 0ULL) {
                    const uint64_t rm = config::bit_lsb(drop);
                    drop = config::bit_clear_lsb_u64(drop);
                    const ApplyResult er = st.eliminate(only_idx, rm);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult forcing_convergence_pass(
    CandidateState& st,
    int pivot_budget,
    int max_steps,
    bool allow_trivalue,
    bool& used_flag) {
    const int nn = st.topo->nn;
    auto& sp = shared::exact_pattern_scratchpad();
    int tested_pivots = 0;
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};

    for (int pass_pc = 2; pass_pc <= (allow_trivalue ? 3 : 2); ++pass_pc) {
        for (int pivot = 0; pivot < nn; ++pivot) {
            if (tested_pivots >= pivot_budget) return ApplyResult::NoProgress;
            if (st.board->values[pivot] != 0) continue;
            const uint64_t mask = st.cands[pivot];
            const int pc = std::popcount(mask);
            if (pc != pass_pc) continue;
            ++tested_pivots;

            shared::snapshot_state(st, sp);
            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;

            int valid_branches = 0;
            uint64_t contradiction_mask = 0ULL;
            int digit_budget = (pc == 2) ? 2 : 3;
            int tested_digits = 0;

            for (uint64_t w = mask; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
                if (tested_digits >= digit_budget) break;
                ++tested_digits;
                const uint64_t bit = config::bit_lsb(w);
                const int digit = config::bit_ctz_u64(bit) + 1;

                shared::restore_state(st, sp);
                bool contradiction = false;
                if (!st.place(pivot, digit)) {
                    contradiction = true;
                } else if (!shared::propagate_singles(st, max_steps)) {
                    contradiction = true;
                }

                if (contradiction) {
                    contradiction_mask |= bit;
                    continue;
                }

                ++valid_branches;
                for (int i = 0; i < nn; ++i) {
                    uint64_t cur = st.cands[i];
                    if (st.board->values[i] != 0) cur = (1ULL << (st.board->values[i] - 1));
                    inter_cands[i] &= cur;
                }
            }

            shared::restore_state(st, sp);

            if (contradiction_mask != 0ULL) {
                const ApplyResult er = st.eliminate(pivot, contradiction_mask);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return ApplyResult::Progress;
                }
            }

            if (valid_branches >= 2) {
                const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return ApplyResult::Progress;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_dynamic_forcing_convergence(
    CandidateState& st,
    bool& used_flag) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    auto& sp = shared::exact_pattern_scratchpad();

    const int pivot_budget = std::clamp(6 + (n / 2), 8, 24);
    const int branch_steps = std::clamp(6 + (n / 3), 8, 16);
    const int nested_budget = std::clamp(3 + (n / 8), 4, 10);
    uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
    int tested_pivots = 0;

    for (int pivot = 0; pivot < nn; ++pivot) {
        if (tested_pivots >= pivot_budget) return ApplyResult::NoProgress;
        if (st.board->values[pivot] != 0) continue;
        const uint64_t mask = st.cands[pivot];
        const int pc = std::popcount(mask);
        if (pc < 2 || pc > 3) continue;
        ++tested_pivots;

        shared::snapshot_state(st, sp);
        for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
        int valid_outer = 0;
        uint64_t contradiction_mask = 0ULL;
        int outer_budget = 0;

        for (uint64_t w = mask; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
            if (outer_budget >= 3) break;
            ++outer_budget;
            const uint64_t bit = config::bit_lsb(w);
            const int digit = config::bit_ctz_u64(bit) + 1;

            shared::restore_state(st, sp);
            bool contradiction = false;
            if (!st.place(pivot, digit)) {
                contradiction = true;
            } else if (!shared::propagate_singles(st, branch_steps)) {
                contradiction = true;
            }
            if (contradiction) {
                contradiction_mask |= bit;
                continue;
            }

            int nested_found = 0;
            uint64_t nested_inter[shared::ExactPatternScratchpad::MAX_NN]{};
            for (int i = 0; i < nn; ++i) nested_inter[i] = ~0ULL;

            for (int inner = 0; inner < nn && nested_found < nested_budget; ++inner) {
                if (inner == pivot || st.board->values[inner] != 0) continue;
                const uint64_t inner_mask = st.cands[inner];
                if (std::popcount(inner_mask) != 2) continue;

                shared::snapshot_state(st, sp);
                int inner_valid = 0;
                for (uint64_t iw = inner_mask; iw != 0ULL; iw = config::bit_clear_lsb_u64(iw)) {
                    const uint64_t ibit = config::bit_lsb(iw);
                    const int id = config::bit_ctz_u64(ibit) + 1;
                    shared::restore_state(st, sp);
                    bool inner_contra = false;
                    if (!st.place(inner, id)) {
                        inner_contra = true;
                    } else if (!shared::propagate_singles(st, branch_steps)) {
                        inner_contra = true;
                    }
                    if (inner_contra) continue;
                    ++inner_valid;
                    for (int i = 0; i < nn; ++i) {
                        uint64_t cur = st.cands[i];
                        if (st.board->values[i] != 0) cur = (1ULL << (st.board->values[i] - 1));
                        nested_inter[i] &= cur;
                    }
                }
                shared::restore_state(st, sp);
                if (inner_valid >= 2) ++nested_found;
            }

            ++valid_outer;
            if (nested_found > 0) {
                for (int i = 0; i < nn; ++i) inter_cands[i] &= nested_inter[i];
            } else {
                for (int i = 0; i < nn; ++i) {
                    uint64_t cur = st.cands[i];
                    if (st.board->values[i] != 0) cur = (1ULL << (st.board->values[i] - 1));
                    inter_cands[i] &= cur;
                }
            }
        }

        shared::restore_state(st, sp);

        if (contradiction_mask != 0ULL) {
            const ApplyResult er = st.eliminate(pivot, contradiction_mask);
            if (er == ApplyResult::Contradiction) return er;
            if (er == ApplyResult::Progress) {
                used_flag = true;
                return ApplyResult::Progress;
            }
        }

        if (valid_outer >= 2) {
            const ApplyResult er = forcing_apply_intersection_elims(st, inter_cands);
            if (er == ApplyResult::Contradiction) return er;
            if (er == ApplyResult::Progress) {
                used_flag = true;
                return ApplyResult::Progress;
            }
        }
    }

    return ApplyResult::NoProgress;
}

// ============================================================================
// Wymuszenia Dynamiczne (Dynamic Forcing Assumption)
// NajciÄ™ĹĽsza metoda (P8). Dokonuje "Nishio Probing" na wÄ™zĹ‚ach bivalue/trivalue.
// Tworzy gaĹ‚Ä™zie i weryfikuje ich stabilnoĹ›Ä‡ (bez peĹ‚nego drzewa DLX).
// ============================================================================
inline ApplyResult apply_dynamic_forcing_assumption(CandidateState& st, bool& used_flag) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    auto& sp = shared::exact_pattern_scratchpad();

    // Ograniczamy budĹĽet hipotez: najpierw bivalue, potem trivalue.
    const int pivot_budget = std::clamp(8 + (n / 2), 8, 40);
    const int max_steps = std::clamp(6 + (n / 3), 8, 20);
    int tested_pivots = 0;

    for (int pass_pc = 2; pass_pc <= 3; ++pass_pc) {
        for (int pivot = 0; pivot < nn; ++pivot) {
            if (tested_pivots >= pivot_budget) return ApplyResult::NoProgress;
            if (st.board->values[pivot] != 0) continue;
            const uint64_t mask = st.cands[pivot];
            const int pc = std::popcount(mask);
            if (pc != pass_pc) continue;
            ++tested_pivots;

            const int digit_budget = (pc == 2) ? 2 : 2;
            int tested_digits = 0;

            for (uint64_t w = mask; w != 0ULL; w &= (w - 1ULL)) {
                if (tested_digits >= digit_budget) break;
                ++tested_digits;

                const uint64_t test_bit = config::bit_lsb(w);
                const int test_digit = config::bit_ctz_u64(test_bit) + 1;

                const bool contradiction =
                    shared::probe_candidate_contradiction(st, pivot, test_digit, max_steps, sp);

                if (!contradiction) continue;

                const ApplyResult er = st.eliminate(pivot, test_bit);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return ApplyResult::Progress;
                }
            }
        }
    }
    return ApplyResult::NoProgress;
}

// ============================================================================
// GĹĂ“WNY INTERFEJS Forcing Chains
// ============================================================================
inline ApplyResult apply_forcing_chains(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    // Heurystyka odcinajÄ…ca (Tylko w pĂłĹşnych fazach ma to sens)
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool used_dynamic = false;
    StrategyStats tmp{};

    const int convergence_steps = std::clamp(6 + (st.topo->n / 3), 8, 16);
    const int assertion_digit_cap = std::clamp(4 + (st.topo->n / 3), 6, 20);
    const int assertion_link_cap = std::clamp(4 + (st.topo->n / 4), 6, 18);
    const int house_cap = std::clamp(3 + (st.topo->n / 4), 4, 14);
    const int house_places = std::clamp(2 + (st.topo->n / 16), 3, 4);
    const ApplyResult ar_assert = forcing_assertion_graph_pass(
        st, assertion_digit_cap, assertion_link_cap, house_cap, house_places, convergence_steps, used_dynamic);
    if (ar_assert == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar_assert;
    }
    if (ar_assert == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    const int pivot_budget = std::clamp(6 + (st.topo->n / 2), 8, 24);
    const ApplyResult ar_conv = forcing_convergence_pass(st, pivot_budget, convergence_steps, false, used_dynamic);
    if (ar_conv == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar_conv;
    }
    if (ar_conv == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // Faza 1: Ograniczone hipotezy wymuszeĹ„ (Nishio-style).
    const ApplyResult ar_assumption = apply_dynamic_forcing_assumption(st, used_dynamic);
    
    if (ar_assumption == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar_assumption; 
    }
    if (ar_assumption == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }
    if (::sudoku_hpc::logic::shared::required_exact_strategy_active(RequiredStrategy::ForcingChains)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    // Faza 2: GĹ‚Ä™bokie zejĹ›cia AIC na grafie implikacji.
    const int depth_cap = std::clamp(14 + (st.board->empty_cells / std::max(1, st.topo->n)), 16, 28);
    
    const ApplyResult dyn = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used_dynamic);
    if (dyn == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn; 
    }
    if (dyn == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return dyn;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// DYNAMIC FORCING CHAINS
// Ewolucja zagnieĹĽdĹĽenia - wywoĹ‚anie jeszcze wiÄ™kszej iloĹ›ci wirtualnych BFS'Ăłw.
// ============================================================================
inline ApplyResult apply_dynamic_forcing_chains(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    bool used_dynamic = false;
    const int branch_steps = std::clamp(6 + (st.topo->n / 3), 8, 16);
    const int dynamic_digit_cap = std::clamp(4 + (st.topo->n / 3), 6, 18);
    const int outer_link_cap = std::clamp(4 + (st.topo->n / 4), 6, 16);
    const int inner_link_cap = std::clamp(2 + (st.topo->n / 8), 3, 8);
    const int house_cap = std::clamp(3 + (st.topo->n / 4), 4, 12);
    const int outer_places = std::clamp(2 + (st.topo->n / 16), 3, 4);
    const int inner_places = std::clamp(2 + (st.topo->n / 20), 3, 4);
    const ApplyResult direct_assert = forcing_dynamic_assertion_graph_pass(
        st, dynamic_digit_cap, outer_link_cap, house_cap, outer_places, std::max(2, house_cap / 2), branch_steps, used_dynamic);
    if (direct_assert == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return direct_assert;
    }
    if (direct_assert == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_dynamic_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    const ApplyResult direct = apply_dynamic_forcing_convergence(st, used_dynamic);
    if (direct == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return direct;
    }
    if (direct == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_dynamic_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }
    if (::sudoku_hpc::logic::shared::required_exact_strategy_active(RequiredStrategy::DynamicForcingChains)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    const ApplyResult dyn_exact = apply_forcing_chains(st, tmp, r);
    
    if (dyn_exact == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn_exact; 
    }
    if (dyn_exact == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return dyn_exact;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical
