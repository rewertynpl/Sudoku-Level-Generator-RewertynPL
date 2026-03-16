// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: empty_rectangle.h (Poziom 4)
// Opis: Algorytm wyszukujący tzw. Puste Prostokąty (Empty Rectangle).
//       Zrewidowana, poprawna matematycznie detekcja kształtu "L" lub "+" 
//       wewnątrz bloku. Zero-allocation, zoptymalizowane O(N).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

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
    // ER wymaga siatki z poprawnymi dwuwymiarowymi blokami
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));

        // Czyszczenie bitboardów dla cyfry d
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

            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.topo->cell_box[idx] != b) continue;
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;

                if (sp.als_cell_count < shared::ExactPatternScratchpad::MAX_NN) {
                    sp.als_cells[sp.als_cell_count++] = idx;
                }
            }

            // Pusty prostokąt wymaga co najmniej 2 kandydatów. 
            // Odcinamy też przepełnione bloki (powyżej 9 z reguły nie tworzy ER)
            if (sp.als_cell_count < 2 || sp.als_cell_count > 9) continue;

            // Szukamy punktu przecięcia (er_r, er_c) tworzącego kształt "L" lub "+"
            bool is_er = false;
            int er_r = -1, er_c = -1;

            const int box_r_start = (b / st.topo->box_cols_count) * st.topo->box_rows;
            const int box_c_start = (b % st.topo->box_cols_count) * st.topo->box_cols;

            for (int r = 0; r < st.topo->box_rows; ++r) {
                for (int c = 0; c < st.topo->box_cols; ++c) {
                    const int base_r = box_r_start + r;
                    const int base_c = box_c_start + c;

                    bool all_covered = true;
                    bool has_r_only = false;
                    bool has_c_only = false;

                    for (int i = 0; i < sp.als_cell_count; ++i) {
                        const int cell = sp.als_cells[i];
                        const int cr = st.topo->cell_row[cell];
                        const int cc = st.topo->cell_col[cell];

                        if (cr != base_r && cc != base_c) {
                            all_covered = false;
                            break;
                        }
                        if (cr == base_r && cc != base_c) has_r_only = true;
                        if (cc == base_c && cr != base_r) has_c_only = true;
                    }

                    // Pusty Prostokąt istnieje tylko wtedy, gdy kandydaci wchodzą
                    // OBA w ramiona przecięcia (nie są po prostu parą Pointing Row/Col)
                    if (all_covered && has_r_only && has_c_only) {
                        is_er = true;
                        er_r = base_r;
                        er_c = base_c;
                        break;
                    }
                }
                if (is_er) break;
            }

            if (!is_er) continue;

            // 1. Sprawdzanie sprzężeń na zewnątrz przez rzędy (Conjugate Rows)
            for (int rr = 0; rr < n; ++rr) {
                // Szukany rząd-łącznik (Strong Link) musi leżeć poza testowanym blokiem
                if (rr >= box_r_start && rr < box_r_start + st.topo->box_rows) continue;
                
                const uint64_t rm = sp.fish_row_masks[rr];
                if (std::popcount(rm) != 2) continue; // Wymagamy Strong Link
                if ((rm & (1ULL << er_c)) == 0ULL) continue; // Musi przecinać kolumnę z naszego ER

                const uint64_t target_col_mask = rm & ~(1ULL << er_c);
                const int target_c = config::bit_ctz_u64(target_col_mask);

                const int target_idx = er_r * n + target_c;
                if (st.topo->cell_box[target_idx] == b) continue; // Eliminujemy poza blokiem ER
                if (st.board->values[target_idx] != 0) continue;
                if ((st.cands[target_idx] & bit) == 0ULL) continue;

                const ApplyResult er = st.eliminate(target_idx, bit);
                if (er == ApplyResult::Contradiction) { 
                    s.elapsed_ns += st.now_ns() - t0; 
                    return er; 
                }
                if (er == ApplyResult::Progress) {
                    progress = true;
                }
            }

            // 2. Sprawdzanie sprzężeń na zewnątrz przez kolumny (Conjugate Cols)
            for (int cc = 0; cc < n; ++cc) {
                // Kolumna-łącznik musi leżeć poza testowanym blokiem
                if (cc >= box_c_start && cc < box_c_start + st.topo->box_cols) continue;

                const uint64_t cm = sp.fish_col_masks[cc];
                if (std::popcount(cm) != 2) continue; // Wymagamy Strong Link
                if ((cm & (1ULL << er_r)) == 0ULL) continue; // Musi przecinać rząd z naszego ER

                const uint64_t target_row_mask = cm & ~(1ULL << er_r);
                const int target_r = config::bit_ctz_u64(target_row_mask);

                const int target_idx = target_r * n + er_c;
                if (st.topo->cell_box[target_idx] == b) continue; // Eliminujemy poza blokiem ER
                if (st.board->values[target_idx] != 0) continue;
                if ((st.cands[target_idx] & bit) == 0ULL) continue;

                const ApplyResult er = st.eliminate(target_idx, bit);
                if (er == ApplyResult::Contradiction) { 
                    s.elapsed_ns += st.now_ns() - t0; 
                    return er; 
                }
                if (er == ApplyResult::Progress) {
                    progress = true;
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