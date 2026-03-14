// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: empty_rectangle.h (Poziom 4)
// Opis: Algorytm wyszukujący tzw. Puste Prostokąty (Empty Rectangle).
//       Sprawdza bloki, w których dana cyfra występuje tylko w jednej
//       kolumnie i jednym rzędzie (kształt litery 'L' na bitboardzie).
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "../../config/bit_utils.h"
#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p4_hard {

inline ApplyResult apply_empty_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));

        std::fill_n(sp.fish_row_masks, n, 0ULL);
        std::fill_n(sp.fish_col_masks, n, 0ULL);

        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;

            const int rr = st.topo->cell_row[idx];
            const int cc = st.topo->cell_col[idx];
            sp.fish_row_masks[rr] |= (1ULL << cc);
            sp.fish_col_masks[cc] |= (1ULL << rr);
        }

        for (int b = 0; b < n; ++b) {
            sp.als_cell_count = 0;
            uint64_t box_rows_used = 0ULL;
            uint64_t box_cols_used = 0ULL;

            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.topo->cell_box[idx] != b) continue;
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;

                sp.als_cells[sp.als_cell_count++] = idx;
                box_rows_used |= (1ULL << st.topo->cell_row[idx]);
                box_cols_used |= (1ULL << st.topo->cell_col[idx]);
                if (sp.als_cell_count > 5) break;
            }

            if (sp.als_cell_count < 2 || sp.als_cell_count > 5) continue;
            if (std::popcount(box_rows_used) > 2 || std::popcount(box_cols_used) > 2) continue;

            int rows[2] = {-1, -1};
            int cols[2] = {-1, -1};
            int row_count = 0;
            int col_count = 0;

            uint64_t tmp_rows = box_rows_used;
            while (tmp_rows != 0ULL && row_count < 2) {
                rows[row_count++] = config::bit_ctz_u64(tmp_rows);
                tmp_rows &= (tmp_rows - 1ULL);
            }

            uint64_t tmp_cols = box_cols_used;
            while (tmp_cols != 0ULL && col_count < 2) {
                cols[col_count++] = config::bit_ctz_u64(tmp_cols);
                tmp_cols &= (tmp_cols - 1ULL);
            }

            if (row_count != 2 || col_count != 2) continue;

            bool present[2][2] = {{false, false}, {false, false}};
            for (int i = 0; i < sp.als_cell_count; ++i) {
                const int idx = sp.als_cells[i];
                const int rr = st.topo->cell_row[idx];
                const int cc = st.topo->cell_col[idx];
                const int ri = (rr == rows[0]) ? 0 : ((rr == rows[1]) ? 1 : -1);
                const int ci = (cc == cols[0]) ? 0 : ((cc == cols[1]) ? 1 : -1);
                if (ri >= 0 && ci >= 0) present[ri][ci] = true;
            }

            for (int ri = 0; ri < 2; ++ri) {
                for (int ci = 0; ci < 2; ++ci) {
                    if (present[ri][ci]) continue;

                    const int rr = rows[ri];
                    const int cc = cols[ci];
                    int row_cell = -1;
                    int col_cell = -1;
                    for (int i = 0; i < sp.als_cell_count; ++i) {
                        const int idx = sp.als_cells[i];
                        if (st.topo->cell_row[idx] == rr && st.topo->cell_col[idx] != cc) row_cell = idx;
                        if (st.topo->cell_col[idx] == cc && st.topo->cell_row[idx] != rr) col_cell = idx;
                    }
                    if (row_cell < 0 || col_cell < 0) continue;

                    const uint64_t row_m = sp.fish_row_masks[rr];
                    if (std::popcount(row_m) != 2 || (row_m & (1ULL << st.topo->cell_col[row_cell])) == 0ULL) continue;
                    const uint64_t row_other_mask = row_m & ~(1ULL << st.topo->cell_col[row_cell]);
                    if (row_other_mask == 0ULL) continue;
                    const int row_other_col = config::bit_ctz_u64(row_other_mask);
                    const int row_other = rr * n + row_other_col;
                    if (st.topo->cell_box[row_other] == b) continue;

                    const uint64_t col_m = sp.fish_col_masks[cc];
                    if (std::popcount(col_m) != 2 || (col_m & (1ULL << st.topo->cell_row[col_cell])) == 0ULL) continue;
                    const uint64_t col_other_mask = col_m & ~(1ULL << st.topo->cell_row[col_cell]);
                    if (col_other_mask == 0ULL) continue;
                    const int col_other_row = config::bit_ctz_u64(col_other_mask);
                    const int col_other = col_other_row * n + cc;
                    if (st.topo->cell_box[col_other] == b) continue;

                    if (row_other == col_other) continue;

                    const int p0 = st.topo->peer_offsets[row_other];
                    const int p1 = st.topo->peer_offsets[row_other + 1];
                    for (int pi = p0; pi < p1; ++pi) {
                        const int t = st.topo->peers_flat[pi];
                        if (t == row_other || t == col_other) continue;
                        if (!st.is_peer(t, col_other)) continue;

                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) {
                            s.elapsed_ns += st.now_ns() - t0;
                            return er;
                        }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_empty_rectangle = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p4_hard
