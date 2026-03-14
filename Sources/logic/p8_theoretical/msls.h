// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: msls.h (Level 8 - Theoretical)
// Description: MSLS (Multi-Sector Locked Sets) with direct sector-cluster
// probing and bounded composite fallback, zero-allocation.
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

#include "../p7_nightmare/aic_grouped_aic.h"
#include "../p7_nightmare/grouped_x_cycle.h"
#include "../p7_nightmare/als_xy_wing_chain.h"
#include "../p7_nightmare/kraken_fish.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool msls_propagate_singles(CandidateState& st, int max_steps) {
    return shared::propagate_singles(st, max_steps);
}

inline bool msls_probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int d,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    return shared::probe_candidate_contradiction(st, idx, d, max_steps, sp);
}

inline int msls_relation_mask(const CandidateState& st, int a, int b) {
    int mask = 0;
    if (st.topo->cell_row[a] == st.topo->cell_row[b]) mask |= 1;
    if (st.topo->cell_col[a] == st.topo->cell_col[b]) mask |= 2;
    if (st.topo->cell_box[a] == st.topo->cell_box[b]) mask |= 4;
    return mask;
}

inline void msls_sort_sector_ids(
    const int* counts,
    int* ids,
    int id_count) {
    for (int i = 1; i < id_count; ++i) {
        const int id = ids[i];
        const int cnt = counts[id];
        int j = i - 1;
        while (j >= 0) {
            const int prev = ids[j];
            if (counts[prev] < cnt) break;
            if (counts[prev] == cnt && prev < id) break;
            ids[j + 1] = prev;
            --j;
        }
        ids[j + 1] = id;
    }
}

inline int msls_collect_active_sector_ids(
    const int* counts,
    int n,
    int min_count,
    int max_count,
    int cap,
    int* out_ids) {
    int out = 0;
    for (int i = 0; i < n; ++i) {
        const int cnt = counts[i];
        if (cnt < min_count || cnt > max_count) continue;
        out_ids[out++] = i;
    }
    msls_sort_sector_ids(counts, out_ids, out);
    return std::min(out, cap);
}

inline int msls_collect_certify_sector_ids(
    const int* counts,
    int n,
    int min_count,
    int cap,
    int* out_ids) {
    int out = 0;
    for (int i = 0; i < n; ++i) {
        if (counts[i] < min_count) continue;
        out_ids[out++] = i;
    }
    msls_sort_sector_ids(counts, out_ids, out);
    return std::min(out, cap);
}

inline ApplyResult msls_apply_closed_sector_eliminations(
    CandidateState& st,
    uint64_t bit,
    const int* in_cluster,
    const int* row_seen,
    const int* col_seen,
    const int* box_seen) {
    const int nn = st.topo->nn;
    int closed_rows[64]{};
    int closed_cols[64]{};
    int closed_boxes[64]{};

    for (int i = 0; i < st.topo->n; ++i) {
        closed_rows[i] = row_seen[i];
        closed_cols[i] = col_seen[i];
        closed_boxes[i] = box_seen[i];
    }

    for (int idx = 0; idx < nn; ++idx) {
        if (in_cluster[idx] || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
        closed_rows[st.topo->cell_row[idx]] = 0;
        closed_cols[st.topo->cell_col[idx]] = 0;
        closed_boxes[st.topo->cell_box[idx]] = 0;
    }

    int closed_total = 0;
    for (int i = 0; i < st.topo->n; ++i) {
        closed_total += closed_rows[i] + closed_cols[i] + closed_boxes[i];
    }
    if (closed_total < 2) return ApplyResult::NoProgress;

    for (int idx = 0; idx < nn; ++idx) {
        if (in_cluster[idx] || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
        if (!closed_rows[st.topo->cell_row[idx]] &&
            !closed_cols[st.topo->cell_col[idx]] &&
            !closed_boxes[st.topo->cell_box[idx]]) {
            continue;
        }
        const ApplyResult er = st.eliminate(idx, bit);
        if (er != ApplyResult::NoProgress) return er;
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult msls_probe_sector_targets(
    CandidateState& st,
    const int* cand_cells,
    int cc,
    const int* cluster,
    int cl,
    const int* in_cluster,
    int d,
    int probe_steps,
    int& probe_budget,
    shared::ExactPatternScratchpad& sp) {
    const uint64_t bit = (1ULL << (d - 1));
    for (int ti = 0; ti < cc && probe_budget > 0; ++ti) {
        const int t = cand_cells[ti];
        if (in_cluster[t]) continue;

        int seen_cells = 0;
        int rel_mask = 0;
        for (int ci = 0; ci < cl; ++ci) {
            const int cidx = cluster[ci];
            if (!st.is_peer(t, cidx)) continue;
            ++seen_cells;
            rel_mask |= msls_relation_mask(st, t, cidx);
            if (seen_cells >= 3 && std::popcount(static_cast<unsigned int>(rel_mask)) >= 2) break;
        }
        if (seen_cells < 2) continue;
        if (std::popcount(static_cast<unsigned int>(rel_mask)) < 2) continue;

        --probe_budget;
        if (!msls_probe_candidate_contradiction(st, t, d, probe_steps, sp)) continue;
        const ApplyResult er = st.eliminate(t, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult msls_try_sector_combo(
    CandidateState& st,
    uint64_t bit,
    const int* cand_cells,
    int cc,
    const int* row_sel,
    const int* col_sel,
    const int* box_sel,
    int min_membership,
    int probe_steps,
    int& probe_budget,
    shared::ExactPatternScratchpad& sp) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    int cluster[96]{};
    int in_cluster[4096]{};
    int row_seen[64]{}, col_seen[64]{}, box_seen[64]{};
    int cl = 0;
    int ru = 0, cu = 0, bu = 0;

    for (int i = 0; i < cc && cl < 96; ++i) {
        const int idx = cand_cells[i];
        int membership = 0;
        membership += row_sel[st.topo->cell_row[idx]];
        membership += col_sel[st.topo->cell_col[idx]];
        membership += box_sel[st.topo->cell_box[idx]];
        if (membership < min_membership) continue;
        in_cluster[idx] = 1;
        cluster[cl++] = idx;

        const int rr = st.topo->cell_row[idx];
        const int ccv = st.topo->cell_col[idx];
        const int bb = st.topo->cell_box[idx];
        if (!row_seen[rr]) {
            row_seen[rr] = 1;
            ++ru;
        }
        if (!col_seen[ccv]) {
            col_seen[ccv] = 1;
            ++cu;
        }
        if (!box_seen[bb]) {
            box_seen[bb] = 1;
            ++bu;
        }
    }

    if (cl < 5 || cl > std::min(nn, 24)) return ApplyResult::NoProgress;
    if (ru + cu + bu < 5) return ApplyResult::NoProgress;
    if ((ru == 0) + (cu == 0) + (bu == 0) > 1) return ApplyResult::NoProgress;
    if (ru < 2 && cu < 2 && bu < 2) return ApplyResult::NoProgress;

    const ApplyResult direct_er = msls_apply_closed_sector_eliminations(
        st, bit, in_cluster, row_seen, col_seen, box_seen);
    if (direct_er != ApplyResult::NoProgress) return direct_er;

    return msls_probe_sector_targets(st, cand_cells, cc, cluster, cl, in_cluster, static_cast<int>(std::countr_zero(bit)) + 1, probe_steps, probe_budget, sp);
}

inline bool msls_certify_mode_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n > 25) return false;
    return st.board->empty_cells <= (nn - 7 * n);
}

inline bool msls_anchor_fallback_allowed(const CandidateState& st) {
    if (msls_certify_mode_allowed(st)) return false;
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    return st.board->empty_cells <= (nn - 6 * n);
}

inline ApplyResult msls_sector_pass(
    CandidateState& st,
    bool certify_mode) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n < 6 || n > 64) return ApplyResult::NoProgress;
    if (st.board->empty_cells > (nn - 6 * n)) return ApplyResult::NoProgress;

    auto& sp = shared::exact_pattern_scratchpad();
    int cand_cells[512]{};
    int row_counts[64]{}, col_counts[64]{}, box_counts[64]{};
    int active_rows[64]{}, active_cols[64]{}, active_boxes[64]{};
    int row_sel[64]{}, col_sel[64]{}, box_sel[64]{};

    const int sector_cap = certify_mode
        ? std::clamp(8 + n / 5, 10, 18)
        : std::clamp(4 + n / 8, 5, 10);
    const int sector_max_count = certify_mode
        ? std::clamp(5 + n / 5, 6, 14)
        : std::clamp(3 + n / 8, 4, 10);
    const int pattern_cap = certify_mode
        ? std::clamp(24 + 2 * n, 30, 120)
        : std::clamp(10 + n / 2, 12, 36);
    const int probe_steps = certify_mode
        ? std::clamp(8 + n / 4, 10, 18)
        : std::clamp(6 + n / 4, 8, 14);
    int pattern_count = 0;
    int probe_budget = certify_mode
        ? std::clamp(24 + 2 * n, 28, 96)
        : std::clamp(10 + n / 2, 12, 30);

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int cc = 0;
        std::fill_n(row_counts, n, 0);
        std::fill_n(col_counts, n, 0);
        std::fill_n(box_counts, n, 0);
        for (int idx = 0; idx < nn && cc < 512; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            cand_cells[cc++] = idx;
            ++row_counts[st.topo->cell_row[idx]];
            ++col_counts[st.topo->cell_col[idx]];
            ++box_counts[st.topo->cell_box[idx]];
        }
        if (cc < 8) continue;

        const int rows = certify_mode
            ? msls_collect_certify_sector_ids(row_counts, n, 2, sector_cap, active_rows)
            : msls_collect_active_sector_ids(row_counts, n, 2, sector_max_count, sector_cap, active_rows);
        const int cols = certify_mode
            ? msls_collect_certify_sector_ids(col_counts, n, 2, sector_cap, active_cols)
            : msls_collect_active_sector_ids(col_counts, n, 2, sector_max_count, sector_cap, active_cols);
        const int boxes = certify_mode
            ? msls_collect_certify_sector_ids(box_counts, n, 2, sector_cap, active_boxes)
            : msls_collect_active_sector_ids(box_counts, n, 2, sector_max_count, sector_cap, active_boxes);

        for (int ri = 0; ri < rows; ++ri) {
            for (int ci = 0; ci < cols; ++ci) {
                if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                std::fill_n(row_sel, n, 0);
                std::fill_n(col_sel, n, 0);
                std::fill_n(box_sel, n, 0);
                row_sel[active_rows[ri]] = 1;
                col_sel[active_cols[ci]] = 1;
                ++pattern_count;
                const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                if (er != ApplyResult::NoProgress) return er;
            }
        }

        for (int ri = 0; ri < rows; ++ri) {
            for (int bi = 0; bi < boxes; ++bi) {
                if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                std::fill_n(row_sel, n, 0);
                std::fill_n(col_sel, n, 0);
                std::fill_n(box_sel, n, 0);
                row_sel[active_rows[ri]] = 1;
                box_sel[active_boxes[bi]] = 1;
                ++pattern_count;
                const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                if (er != ApplyResult::NoProgress) return er;
            }
        }

        for (int ci = 0; ci < cols; ++ci) {
            for (int bi = 0; bi < boxes; ++bi) {
                if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                std::fill_n(row_sel, n, 0);
                std::fill_n(col_sel, n, 0);
                std::fill_n(box_sel, n, 0);
                col_sel[active_cols[ci]] = 1;
                box_sel[active_boxes[bi]] = 1;
                ++pattern_count;
                const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                if (er != ApplyResult::NoProgress) return er;
            }
        }

        for (int ri = 0; ri < rows; ++ri) {
            for (int ci = 0; ci < cols; ++ci) {
                for (int bi = 0; bi < boxes; ++bi) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                    std::fill_n(row_sel, n, 0);
                    std::fill_n(col_sel, n, 0);
                    std::fill_n(box_sel, n, 0);
                    row_sel[active_rows[ri]] = 1;
                    col_sel[active_cols[ci]] = 1;
                    box_sel[active_boxes[bi]] = 1;
                    ++pattern_count;
                    const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }

        if (!certify_mode) continue;

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int bi = 0; bi < boxes; ++bi) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                    std::fill_n(row_sel, n, 0);
                    std::fill_n(col_sel, n, 0);
                    std::fill_n(box_sel, n, 0);
                    row_sel[active_rows[r1]] = 1;
                    row_sel[active_rows[r2]] = 1;
                    box_sel[active_boxes[bi]] = 1;
                    ++pattern_count;
                    const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }

        for (int c1 = 0; c1 < cols; ++c1) {
            for (int c2 = c1 + 1; c2 < cols; ++c2) {
                for (int bi = 0; bi < boxes; ++bi) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                    std::fill_n(row_sel, n, 0);
                    std::fill_n(col_sel, n, 0);
                    std::fill_n(box_sel, n, 0);
                    col_sel[active_cols[c1]] = 1;
                    col_sel[active_cols[c2]] = 1;
                    box_sel[active_boxes[bi]] = 1;
                    ++pattern_count;
                    const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }

        for (int ri = 0; ri < rows; ++ri) {
            for (int b1 = 0; b1 < boxes; ++b1) {
                for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                    std::fill_n(row_sel, n, 0);
                    std::fill_n(col_sel, n, 0);
                    std::fill_n(box_sel, n, 0);
                    row_sel[active_rows[ri]] = 1;
                    box_sel[active_boxes[b1]] = 1;
                    box_sel[active_boxes[b2]] = 1;
                    ++pattern_count;
                    const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }

        for (int ci = 0; ci < cols; ++ci) {
            for (int b1 = 0; b1 < boxes; ++b1) {
                for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                    std::fill_n(row_sel, n, 0);
                    std::fill_n(col_sel, n, 0);
                    std::fill_n(box_sel, n, 0);
                    col_sel[active_cols[ci]] = 1;
                    box_sel[active_boxes[b1]] = 1;
                    box_sel[active_boxes[b2]] = 1;
                    ++pattern_count;
                    const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int c1 = 0; c1 < cols; ++c1) {
                    for (int c2 = c1 + 1; c2 < cols; ++c2) {
                        if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        row_sel[active_rows[r1]] = 1;
                        row_sel[active_rows[r2]] = 1;
                        col_sel[active_cols[c1]] = 1;
                        col_sel[active_cols[c2]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int r3 = r2 + 1; r3 < rows; ++r3) {
                    for (int bi = 0; bi < boxes; ++bi) {
                        if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        row_sel[active_rows[r1]] = 1;
                        row_sel[active_rows[r2]] = 1;
                        row_sel[active_rows[r3]] = 1;
                        box_sel[active_boxes[bi]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int c1 = 0; c1 < cols; ++c1) {
            for (int c2 = c1 + 1; c2 < cols; ++c2) {
                for (int c3 = c2 + 1; c3 < cols; ++c3) {
                    for (int bi = 0; bi < boxes; ++bi) {
                        if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        col_sel[active_cols[c1]] = 1;
                        col_sel[active_cols[c2]] = 1;
                        col_sel[active_cols[c3]] = 1;
                        box_sel[active_boxes[bi]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int b1 = 0; b1 < boxes; ++b1) {
                    for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                        if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        row_sel[active_rows[r1]] = 1;
                        row_sel[active_rows[r2]] = 1;
                        box_sel[active_boxes[b1]] = 1;
                        box_sel[active_boxes[b2]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int ci = 0; ci < cols; ++ci) {
                    for (int bi = 0; bi < boxes; ++bi) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        row_sel[active_rows[r1]] = 1;
                        row_sel[active_rows[r2]] = 1;
                        col_sel[active_cols[ci]] = 1;
                        box_sel[active_boxes[bi]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int ri = 0; ri < rows; ++ri) {
            for (int c1 = 0; c1 < cols; ++c1) {
                for (int c2 = c1 + 1; c2 < cols; ++c2) {
                    for (int bi = 0; bi < boxes; ++bi) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        row_sel[active_rows[ri]] = 1;
                        col_sel[active_cols[c1]] = 1;
                        col_sel[active_cols[c2]] = 1;
                        box_sel[active_boxes[bi]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int c1 = 0; c1 < cols; ++c1) {
            for (int c2 = c1 + 1; c2 < cols; ++c2) {
                for (int b1 = 0; b1 < boxes; ++b1) {
                    for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        col_sel[active_cols[c1]] = 1;
                        col_sel[active_cols[c2]] = 1;
                        box_sel[active_boxes[b1]] = 1;
                        box_sel[active_boxes[b2]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int ri = 0; ri < rows; ++ri) {
            for (int ci = 0; ci < cols; ++ci) {
                for (int b1 = 0; b1 < boxes; ++b1) {
                    for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                    if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                        std::fill_n(row_sel, n, 0);
                        std::fill_n(col_sel, n, 0);
                        std::fill_n(box_sel, n, 0);
                        row_sel[active_rows[ri]] = 1;
                        col_sel[active_cols[ci]] = 1;
                        box_sel[active_boxes[b1]] = 1;
                        box_sel[active_boxes[b2]] = 1;
                        ++pattern_count;
                        const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int r3 = r2 + 1; r3 < rows; ++r3) {
                    for (int b1 = 0; b1 < boxes; ++b1) {
                        for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                            if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                            std::fill_n(row_sel, n, 0);
                            std::fill_n(col_sel, n, 0);
                            std::fill_n(box_sel, n, 0);
                            row_sel[active_rows[r1]] = 1;
                            row_sel[active_rows[r2]] = 1;
                            row_sel[active_rows[r3]] = 1;
                            box_sel[active_boxes[b1]] = 1;
                            box_sel[active_boxes[b2]] = 1;
                            ++pattern_count;
                            const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }

        for (int c1 = 0; c1 < cols; ++c1) {
            for (int c2 = c1 + 1; c2 < cols; ++c2) {
                for (int c3 = c2 + 1; c3 < cols; ++c3) {
                    for (int b1 = 0; b1 < boxes; ++b1) {
                        for (int b2 = b1 + 1; b2 < boxes; ++b2) {
                            if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                            std::fill_n(row_sel, n, 0);
                            std::fill_n(col_sel, n, 0);
                            std::fill_n(box_sel, n, 0);
                            col_sel[active_cols[c1]] = 1;
                            col_sel[active_cols[c2]] = 1;
                            col_sel[active_cols[c3]] = 1;
                            box_sel[active_boxes[b1]] = 1;
                            box_sel[active_boxes[b2]] = 1;
                            ++pattern_count;
                            const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }

        for (int r1 = 0; r1 < rows; ++r1) {
            for (int r2 = r1 + 1; r2 < rows; ++r2) {
                for (int c1 = 0; c1 < cols; ++c1) {
                    for (int c2 = c1 + 1; c2 < cols; ++c2) {
                        for (int bi = 0; bi < boxes; ++bi) {
                            if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
                            std::fill_n(row_sel, n, 0);
                            std::fill_n(col_sel, n, 0);
                            std::fill_n(box_sel, n, 0);
                            row_sel[active_rows[r1]] = 1;
                            row_sel[active_rows[r2]] = 1;
                            col_sel[active_cols[c1]] = 1;
                            col_sel[active_cols[c2]] = 1;
                            box_sel[active_boxes[bi]] = 1;
                            ++pattern_count;
                            const ApplyResult er = msls_try_sector_combo(st, bit, cand_cells, cc, row_sel, col_sel, box_sel, 2, probe_steps, probe_budget, sp);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }
next_digit:
        continue;
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_msls_sector_direct(CandidateState& st) {
    return msls_sector_pass(st, false);
}

inline ApplyResult apply_msls_sector_certify(CandidateState& st) {
    if (!msls_certify_mode_allowed(st)) return ApplyResult::NoProgress;
    return msls_sector_pass(st, true);
}

inline ApplyResult apply_msls_anchor_direct(CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n < 6 || n > 64) return ApplyResult::NoProgress;
    if (!msls_anchor_fallback_allowed(st)) return ApplyResult::NoProgress;

    auto& sp = shared::exact_pattern_scratchpad();
    int cand_cells[512]{};
    int cluster[64]{};
    int in_cluster[4096]{};

    const int anchor_cap = std::clamp(6 + n / 3, 8, 18);
    const int pattern_cap = std::clamp(8 + n / 2, 10, 28);
    const int probe_steps = std::clamp(6 + n / 4, 8, 14);
    int pattern_count = 0;
    int probe_budget = std::clamp(8 + n / 2, 10, 26);

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int cc = 0;
        for (int idx = 0; idx < nn && cc < 512; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            cand_cells[cc++] = idx;
        }
        if (cc < 8) continue;

        const int anchors = std::min(cc, anchor_cap);
        for (int ai = 0; ai < anchors; ++ai) {
            if (pattern_count >= pattern_cap || probe_budget <= 0) goto next_digit;
            const int anchor = cand_cells[ai];
            const int ar = st.topo->cell_row[anchor];
            const int ac = st.topo->cell_col[anchor];
            const int ab = st.topo->cell_box[anchor];

            std::fill_n(in_cluster, nn, 0);
            int cl = 0;
            for (int i = 0; i < cc && cl < 64; ++i) {
                const int idx = cand_cells[i];
                if (st.topo->cell_row[idx] == ar ||
                    st.topo->cell_col[idx] == ac ||
                    st.topo->cell_box[idx] == ab) {
                    in_cluster[idx] = 1;
                    cluster[cl++] = idx;
                }
            }
            if (cl < 5 || cl > 16) continue;

            int row_seen[64]{}, col_seen[64]{}, box_seen[64]{};
            int ru = 0, cu = 0, bu = 0;
            for (int i = 0; i < cl; ++i) {
                const int idx = cluster[i];
                const int rr = st.topo->cell_row[idx];
                const int ccv = st.topo->cell_col[idx];
                const int bb = st.topo->cell_box[idx];
                if (!row_seen[rr]) {
                    row_seen[rr] = 1;
                    ++ru;
                }
                if (!col_seen[ccv]) {
                    col_seen[ccv] = 1;
                    ++cu;
                }
                if (!box_seen[bb]) {
                    box_seen[bb] = 1;
                    ++bu;
                }
            }
            if (ru < 2 || cu < 2 || bu < 2) continue;
            ++pattern_count;

            const ApplyResult direct_er = msls_apply_closed_sector_eliminations(
                st, bit, in_cluster, row_seen, col_seen, box_seen);
            if (direct_er != ApplyResult::NoProgress) return direct_er;

            const ApplyResult probe_er = msls_probe_sector_targets(st, cand_cells, cc, cluster, cl, in_cluster, d, probe_steps, probe_budget, sp);
            if (probe_er != ApplyResult::NoProgress) return probe_er;
        }
next_digit:
        continue;
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_msls_direct(CandidateState& st) {
    ApplyResult ar = apply_msls_sector_direct(st);
    if (ar != ApplyResult::NoProgress) return ar;
    ar = apply_msls_sector_certify(st);
    if (ar != ApplyResult::NoProgress) return ar;
    if (msls_certify_mode_allowed(st)) return ApplyResult::NoProgress;
    return apply_msls_anchor_direct(st);
}

inline ApplyResult apply_msls(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 5 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};

    ApplyResult ar = apply_msls_direct(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_msls = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    const int depth_cap = std::clamp(8 + (st.board->empty_cells / std::max(1, n)), 10, 16);
    bool used_dynamic = false;
    ar = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used_dynamic);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = p7_nightmare::apply_grouped_x_cycle(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = p7_nightmare::apply_als_chain(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    // Kraken fallback only for very late boards.
    if (st.board->empty_cells <= (nn - 7 * n)) {
        ar = p7_nightmare::apply_kraken_fish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ar;
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical
