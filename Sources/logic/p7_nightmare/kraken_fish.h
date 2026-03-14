// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: kraken_fish.h (Level 7 - Nightmare)
// Description: Kraken Fish as a combination of finned fish body detection
// and deep alternating tentacle chains (zero-allocation).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/state_probe.h"
#include "../p6_diabolical/finned_jelly_sword.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool kraken_digit_chain_connects(
    shared::ExactPatternScratchpad& sp,
    int start_node,
    int end_node,
    int depth_cap) {
    if (start_node < 0 || end_node < 0 || start_node == end_node) return false;

    int neighbors[256]{};
    int* const vis_even = sp.visited;
    int* const vis_odd = sp.bfs_depth;
    int* const queue_state = sp.bfs_queue;
    int* const queue_depth = sp.bfs_parent;

    for (int first_type = 1; first_type >= 0; --first_type) {
        std::fill_n(vis_even, sp.dyn_node_count, 0);
        std::fill_n(vis_odd, sp.dyn_node_count, 0);

        int qh = 0;
        int qt = 0;
        const int first_cnt = aic_collect_neighbors(sp, start_node, first_type, neighbors);
        for (int i = 0; i < first_cnt; ++i) {
            const int v = neighbors[i];
            if (v == end_node) return true;
            if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
            queue_state[qt] = (v << 1) | first_type;
            queue_depth[qt] = 1;
            if (first_type == 0) vis_even[v] = 1;
            else vis_odd[v] = 1;
            ++qt;
        }

        while (qh < qt) {
            const int state = queue_state[qh];
            const int dep = queue_depth[qh];
            ++qh;

            const int u = (state >> 1);
            const int last_type = (state & 1);
            const int next_type = 1 - last_type;
            if (dep >= depth_cap) continue;

            const int next_cnt = aic_collect_neighbors(sp, u, next_type, neighbors);
            for (int i = 0; i < next_cnt; ++i) {
                const int v = neighbors[i];
                const int nd = dep + 1;
                if (v == end_node && (nd & 1) == 1) return true;

                int* const vis = (next_type == 0) ? vis_even : vis_odd;
                if (vis[v] != 0) continue;
                vis[v] = 1;
                if (qt >= shared::ExactPatternScratchpad::MAX_BFS) continue;
                queue_state[qt] = (v << 1) | next_type;
                queue_depth[qt] = nd;
                ++qt;
            }
        }
    }
    return false;
}

inline bool kraken_target_reached_by_all_tentacles(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int target_idx,
    const int* fin_cells,
    int fin_count,
    int depth_cap) {
    if (!shared::build_grouped_link_graph_for_digit(st, config::bit_ctz_u64(bit) + 1, sp)) return false;
    if (sp.dyn_node_count < 2 || sp.dyn_strong_edge_count == 0) return false;

    const int target_node = sp.dyn_cell_to_node[target_idx];
    if (target_node < 0) return false;

    for (int i = 0; i < fin_count; ++i) {
        const int fin_node = sp.dyn_cell_to_node[fin_cells[i]];
        if (fin_node < 0) return false;
        if (!kraken_digit_chain_connects(sp, fin_node, target_node, depth_cap)) return false;
    }
    return true;
}

inline ApplyResult kraken_try_targets(
    CandidateState& st,
    uint64_t bit,
    bool row_based,
    const int* base_lines,
    int base_count,
    uint64_t covers_union,
    const int* fin_cells,
    int fin_count,
    int probe_steps) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;

    for (uint64_t w = covers_union; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
        const int cover = config::bit_ctz_u64(w);
        for (int orth = 0; orth < n; ++orth) {
            const int idx = row_based ? (orth * n + cover) : (cover * n + orth);
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;

            const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
            bool in_base = false;
            for (int i = 0; i < base_count; ++i) {
                if (idx_line == base_lines[i]) {
                    in_base = true;
                    break;
                }
            }
            if (in_base) continue;

            bool sees_tentacles = true;
            for (int i = 0; i < fin_count; ++i) {
                if (!st.is_peer(idx, fin_cells[i])) {
                    sees_tentacles = false;
                    break;
                }
            }
            if (!sees_tentacles) continue;

            const int digit = config::bit_ctz_u64(bit) + 1;
            if (!shared::probe_candidate_contradiction(st, idx, digit, probe_steps, sp)) continue;
            return st.eliminate(idx, bit);
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult kraken_try_box_targets(
    CandidateState& st,
    uint64_t bit,
    bool row_based,
    const int* base_lines,
    int base_count,
    const int* fin_cells,
    int fin_count,
    int fin_box,
    int probe_steps) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;
    const int box_rows = st.topo->box_rows;
    const int box_cols = st.topo->box_cols;
    const int box_r = fin_box / (n / box_cols);
    const int box_c = fin_box % (n / box_cols);
    const int row0 = box_r * box_rows;
    const int col0 = box_c * box_cols;

    for (int dr = 0; dr < box_rows; ++dr) {
        for (int dc = 0; dc < box_cols; ++dc) {
            const int r = row0 + dr;
            const int c = col0 + dc;
            const int idx = r * n + c;
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;

            bool skip = false;
            for (int i = 0; i < fin_count; ++i) {
                if (idx == fin_cells[i]) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
            bool in_base = false;
            for (int i = 0; i < base_count; ++i) {
                if (idx_line == base_lines[i]) {
                    in_base = true;
                    break;
                }
            }
            if (in_base) continue;

            bool sees_some_fin = false;
            for (int i = 0; i < fin_count; ++i) {
                if (st.is_peer(idx, fin_cells[i])) {
                    sees_some_fin = true;
                    break;
                }
            }
            if (!sees_some_fin) continue;

            const int digit = config::bit_ctz_u64(bit) + 1;
            if (!shared::probe_candidate_contradiction(st, idx, digit, probe_steps, sp)) continue;
            return st.eliminate(idx, bit);
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult kraken_try_chain_targets(
    CandidateState& st,
    uint64_t bit,
    bool row_based,
    const int* base_lines,
    int base_count,
    uint64_t covers_union,
    const int* fin_cells,
    int fin_count,
    int probe_steps,
    int depth_cap) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;

    for (uint64_t w = covers_union; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
        const int cover = config::bit_ctz_u64(w);
        for (int orth = 0; orth < n; ++orth) {
            const int idx = row_based ? (orth * n + cover) : (cover * n + orth);
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;

            const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
            bool in_base = false;
            for (int i = 0; i < base_count; ++i) {
                if (idx_line == base_lines[i]) {
                    in_base = true;
                    break;
                }
            }
            if (in_base) continue;

            bool direct_seen = true;
            for (int i = 0; i < fin_count; ++i) {
                if (!st.is_peer(idx, fin_cells[i])) {
                    direct_seen = false;
                    break;
                }
            }
            if (direct_seen) continue;
            if (!kraken_target_reached_by_all_tentacles(st, sp, bit, idx, fin_cells, fin_count, depth_cap)) continue;

            const int digit = config::bit_ctz_u64(bit) + 1;
            if (!shared::probe_candidate_contradiction(st, idx, digit, probe_steps, sp)) continue;
            return st.eliminate(idx, bit);
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult kraken_try_chain_box_targets(
    CandidateState& st,
    uint64_t bit,
    bool row_based,
    const int* base_lines,
    int base_count,
    const int* fin_cells,
    int fin_count,
    int fin_box,
    int probe_steps,
    int depth_cap) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;
    const int box_rows = st.topo->box_rows;
    const int box_cols = st.topo->box_cols;
    const int box_r = fin_box / (n / box_cols);
    const int box_c = fin_box % (n / box_cols);
    const int row0 = box_r * box_rows;
    const int col0 = box_c * box_cols;

    for (int dr = 0; dr < box_rows; ++dr) {
        for (int dc = 0; dc < box_cols; ++dc) {
            const int r = row0 + dr;
            const int c = col0 + dc;
            const int idx = r * n + c;
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;

            bool skip = false;
            for (int i = 0; i < fin_count; ++i) {
                if (idx == fin_cells[i]) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
            bool in_base = false;
            for (int i = 0; i < base_count; ++i) {
                if (idx_line == base_lines[i]) {
                    in_base = true;
                    break;
                }
            }
            if (in_base) continue;

            bool direct_seen = false;
            for (int i = 0; i < fin_count; ++i) {
                if (st.is_peer(idx, fin_cells[i])) {
                    direct_seen = true;
                    break;
                }
            }
            if (direct_seen) continue;
            if (!kraken_target_reached_by_all_tentacles(st, sp, bit, idx, fin_cells, fin_count, depth_cap)) continue;

            const int digit = config::bit_ctz_u64(bit) + 1;
            if (!shared::probe_candidate_contradiction(st, idx, digit, probe_steps, sp)) continue;
            return st.eliminate(idx, bit);
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult kraken_direct_scan_for_digit(
    CandidateState& st,
    int digit,
    int fish_size,
    bool row_based,
    int combo_cap,
    int probe_steps,
    int depth_cap) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;
    const uint64_t bit = (1ULL << (digit - 1));

    int active_lines[64]{};
    uint64_t line_masks[64]{};
    int line_count = 0;
    for (int line = 0; line < n; ++line) {
        uint64_t mask = 0ULL;
        for (int orth = 0; orth < n; ++orth) {
            const int idx = row_based ? (line * n + orth) : (orth * n + line);
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            const int cover = row_based ? st.topo->cell_col[idx] : st.topo->cell_row[idx];
            mask |= (1ULL << cover);
        }
        const int pc = std::popcount(mask);
        if (pc >= 2 && pc <= fish_size + 1 && line_count < 64) {
            active_lines[line_count] = line;
            line_masks[line_count] = mask;
            ++line_count;
        }
    }
    if (line_count < fish_size) return ApplyResult::NoProgress;
    if (line_count > std::min(n, 24)) return ApplyResult::NoProgress;

    int combo_checks = 0;
    auto process_combo = [&](const int* ids, int count) -> ApplyResult {
        uint64_t covers_union = 0ULL;
        int base_lines[5]{};
        uint64_t masks[5]{};
        for (int i = 0; i < count; ++i) {
            base_lines[i] = active_lines[ids[i]];
            masks[i] = line_masks[ids[i]];
            covers_union |= masks[i];
        }
        const int cover_pc = std::popcount(covers_union);
        if (cover_pc < fish_size || cover_pc > fish_size + 1) return ApplyResult::NoProgress;

        int fin_cells[4]{};
        int fin_count = 0;
        for (uint64_t w = covers_union; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
            const int cover = config::bit_ctz_u64(w);
            int owner = -1;
            int cnt = 0;
            for (int i = 0; i < count; ++i) {
                if ((masks[i] & (1ULL << cover)) == 0ULL) continue;
                owner = i;
                ++cnt;
            }
            if (cnt == 1 && fin_count < 4) {
                fin_cells[fin_count++] =
                    row_based ? (base_lines[owner] * n + cover) : (cover * n + base_lines[owner]);
            }
        }
        if (fin_count <= 0 || fin_count > 2) return ApplyResult::NoProgress;

        int common_box = st.topo->cell_box[fin_cells[0]];
        bool same_box = true;
        for (int i = 1; i < fin_count; ++i) {
            if (st.topo->cell_box[fin_cells[i]] != common_box) {
                same_box = false;
                break;
            }
        }
        if (!same_box) return ApplyResult::NoProgress;

        ApplyResult ar = kraken_try_targets(st, bit, row_based, base_lines, count, covers_union, fin_cells, fin_count, probe_steps);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_try_box_targets(st, bit, row_based, base_lines, count, fin_cells, fin_count, common_box, probe_steps);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_try_chain_targets(st, bit, row_based, base_lines, count, covers_union, fin_cells, fin_count, probe_steps, depth_cap);
        if (ar != ApplyResult::NoProgress) return ar;
        return kraken_try_chain_box_targets(st, bit, row_based, base_lines, count, fin_cells, fin_count, common_box, probe_steps, depth_cap);
    };

    if (fish_size == 3) {
        for (int i0 = 0; i0 < line_count; ++i0) {
            for (int i1 = i0 + 1; i1 < line_count; ++i1) {
                for (int i2 = i1 + 1; i2 < line_count; ++i2) {
                    if (++combo_checks > combo_cap) return ApplyResult::NoProgress;
                    const int ids[3] = {i0, i1, i2};
                    const ApplyResult ar = process_combo(ids, 3);
                    if (ar != ApplyResult::NoProgress) return ar;
                }
            }
        }
    } else {
        for (int i0 = 0; i0 < line_count; ++i0) {
            for (int i1 = i0 + 1; i1 < line_count; ++i1) {
                for (int i2 = i1 + 1; i2 < line_count; ++i2) {
                    for (int i3 = i2 + 1; i3 < line_count; ++i3) {
                        if (++combo_checks > combo_cap) return ApplyResult::NoProgress;
                        const int ids[4] = {i0, i1, i2, i3};
                        const ApplyResult ar = process_combo(ids, 4);
                        if (ar != ApplyResult::NoProgress) return ar;
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_kraken_fish_direct(CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n < 6 || n > 25) return ApplyResult::NoProgress;
    if (st.board->empty_cells > (nn - 2 * n)) return ApplyResult::NoProgress;

    const int probe_steps = std::clamp(6 + n / 4, 8, 14);
    const int depth_cap = std::clamp(8 + n / 3, 10, 18);
    const int combo_cap = (n <= 12) ? 4000 : ((n <= 16) ? 2600 : 1800);
    for (int d = 1; d <= n; ++d) {
        ApplyResult ar = kraken_direct_scan_for_digit(st, d, 3, true, combo_cap, probe_steps, depth_cap);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_direct_scan_for_digit(st, d, 3, false, combo_cap, probe_steps, depth_cap);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_direct_scan_for_digit(st, d, 4, true, combo_cap / 2, probe_steps, depth_cap);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_direct_scan_for_digit(st, d, 4, false, combo_cap / 2, probe_steps, depth_cap);
        if (ar != ApplyResult::NoProgress) return ar;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_kraken_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    // Kraken patterns become useful in late game where fish body + tentacles
    // are constrained enough to produce deterministic eliminations.
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    ApplyResult ar = apply_kraken_fish_direct(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_kraken_fish = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }

    // Step 1: fallback fish body
    StrategyStats tmp{};
    ar = p6_diabolical::apply_finned_swordfish_jellyfish(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_kraken_fish = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }

    // Step 2: deep tentacles
    bool used = false;
    const int depth_cap = std::clamp(14 + (st.board->empty_cells / std::max(1, st.topo->n)), 16, 26);
    ar = alternating_chain_core(st, depth_cap, false, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_kraken_fish = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
