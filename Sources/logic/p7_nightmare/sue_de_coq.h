// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: sue_de_coq.h (Level 7 - Nightmare)
// Description: Full Sue de Coq implementation for row-box and col-box
// intersections. Zero-allocation, fixed-size local buffers only.
// Updated for N=64 and asymmetric geometries with 5-cell combo support.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

// Zaktualizowany silnik kombinatoryczny obsługujący do 5 elementów
template <class Fn>
inline void for_each_combo_up_to5(int count, int choose, const Fn& fn) {
    if (choose == 1) {
        for (int a = 0; a < count; ++a) {
            fn(a, -1, -1, -1, -1);
        }
        return;
    }
    if (choose == 2) {
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                fn(a, b, -1, -1, -1);
            }
        }
        return;
    }
    if (choose == 3) {
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                for (int c = b + 1; c < count; ++c) {
                    fn(a, b, c, -1, -1);
                }
            }
        }
        return;
    }
    if (choose == 4) {
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                for (int c = b + 1; c < count; ++c) {
                    for (int d = c + 1; d < count; ++d) {
                        fn(a, b, c, d, -1);
                    }
                }
            }
        }
        return;
    }
    if (choose == 5) {
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                for (int c = b + 1; c < count; ++c) {
                    for (int d = c + 1; d < count; ++d) {
                        for (int e = d + 1; e < count; ++e) {
                            fn(a, b, c, d, e);
                        }
                    }
                }
            }
        }
        return;
    }
}

inline uint64_t pool_mask_from_selection(
    const CandidateState& st,
    const int* pool,
    int i1, int i2, int i3, int i4, int i5) {
    uint64_t m = 0ULL;
    if (i1 >= 0) m |= st.cands[pool[i1]];
    if (i2 >= 0) m |= st.cands[pool[i2]];
    if (i3 >= 0) m |= st.cands[pool[i3]];
    if (i4 >= 0) m |= st.cands[pool[i4]];
    if (i5 >= 0) m |= st.cands[pool[i5]];
    return m;
}

inline bool idx_equals_any_selected(int idx, const int* pool, int i1, int i2, int i3, int i4, int i5) {
    return (i1 >= 0 && idx == pool[i1]) ||
           (i2 >= 0 && idx == pool[i2]) ||
           (i3 >= 0 && idx == pool[i3]) ||
           (i4 >= 0 && idx == pool[i4]) ||
           (i5 >= 0 && idx == pool[i5]);
}

inline int sue_de_coq_selection_cap(const CandidateState& st) {
    const int box_area = st.topo->box_rows * st.topo->box_cols;
    // Obejmuje warianty SDC do 5 wykluczeń dla mniejszych / rzadkich plansz
    if (st.topo->n <= 16 || box_area <= 12) return 5;
    return 3;
}

// ----------------------------------------------------------------------------
// SCAN ROW-BOX INTERSECTIONS
// ----------------------------------------------------------------------------
inline ApplyResult scan_row_box_intersections(CandidateState& st, bool& progress, StrategyStats& s, uint64_t t0) {
    const int n = st.topo->n;
    bool contradiction = false;

    // Przeszukujemy każdy blok na planszy
    for (int b = 0; b < n; ++b) {
        int rows[64];
        int row_cnt = 0;
        const int box_p0 = st.topo->house_offsets[2 * n + b];
        const int box_p1 = st.topo->house_offsets[2 * n + b + 1];

        // Wyznaczamy wszystkie rzędy przecinające ten blok (zabezpieczenie asymetrii)
        for (int p = box_p0; p < box_p1; ++p) {
            const int r = st.topo->cell_row[st.topo->houses_flat[p]];
            bool found = false;
            for (int i = 0; i < row_cnt; ++i) {
                if (rows[i] == r) { found = true; break; }
            }
            if (!found) rows[row_cnt++] = r;
        }

        // Testujemy każdy przecinający rząd jako punkt intersekcji SDC
        for (int i = 0; i < row_cnt; ++i) {
            const int r = rows[i];

            int a_cells[64]{};
            int a_cnt = 0;
            uint64_t ma = 0ULL;
            int c_pool[64]{};
            int c_pool_cnt = 0;

            // Zbiór A (Część wspólna) i Zbiór C (Tylko Blok)
            for (int p = box_p0; p < box_p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] == 0) {
                    if (st.topo->cell_row[idx] == r) {
                        a_cells[a_cnt++] = idx;
                        ma |= st.cands[idx];
                    } else {
                        c_pool[c_pool_cnt++] = idx;
                    }
                }
            }
            if (a_cnt < 2) continue; // SDC wymaga min 2 komórek intersekcji

            int b_pool[64]{};
            int b_pool_cnt = 0;
            const int row_p0 = st.topo->house_offsets[r];
            const int row_p1 = st.topo->house_offsets[r + 1];

            // Zbiór B (Tylko Rząd)
            for (int p = row_p0; p < row_p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] == 0 && st.topo->cell_box[idx] != b) {
                    b_pool[b_pool_cnt++] = idx;
                }
            }

            const int select_cap = sue_de_coq_selection_cap(st);
            const int b_limit = std::min(b_pool_cnt, select_cap);
            const int c_limit = std::min(c_pool_cnt, select_cap);
            if (b_limit <= 0 || c_limit <= 0) continue;

            for (int b_choose = 1; b_choose <= b_limit; ++b_choose) {
                for (int c_choose = 1; c_choose <= c_limit; ++c_choose) {
                    const int target_cands = a_cnt + b_choose + c_choose;
                    if (std::popcount(ma) > target_cands) continue;

                    for_each_combo_up_to5(b_pool_cnt, b_choose, [&](int bi1, int bi2, int bi3, int bi4, int bi5) {
                        if (contradiction) return;
                        const uint64_t mb = pool_mask_from_selection(st, b_pool, bi1, bi2, bi3, bi4, bi5);
                        const uint64_t row_union = ma | mb;
                        if (std::popcount(row_union) != (a_cnt + b_choose)) return;

                        for_each_combo_up_to5(c_pool_cnt, c_choose, [&](int ci1, int ci2, int ci3, int ci4, int ci5) {
                            if (contradiction) return;
                            const uint64_t mc = pool_mask_from_selection(st, c_pool, ci1, ci2, ci3, ci4, ci5);
                            const uint64_t box_union = ma | mc;
                            if (std::popcount(box_union) != (a_cnt + c_choose)) return;

                            const uint64_t union_mask = row_union | mc;
                            if (std::popcount(union_mask) != target_cands) return;
                            
                            const uint64_t row_only = row_union & ~box_union;
                            const uint64_t box_only = box_union & ~row_union;
                            
                            // Ścisła walidacja Sue de Coq: Cyfry wykluczone muszą być stricte odseparowane
                            if (row_only == 0ULL || box_only == 0ULL) return;

                            // Eliminacja w domenie Rzędu (Tylko część wspólna Rzędu - poza zbiorem A i B)
                            if (row_union != 0ULL) {
                                for (int p = row_p0; p < row_p1; ++p) {
                                    const int idx = st.topo->houses_flat[p];
                                    if (st.board->values[idx] != 0) continue;
                                    if (st.topo->cell_box[idx] == b) continue; // Węzeł A
                                    if (idx_equals_any_selected(idx, b_pool, bi1, bi2, bi3, bi4, bi5)) continue; // Węzeł B
                                    
                                    const ApplyResult er = st.eliminate(idx, row_union);
                                    if (er == ApplyResult::Contradiction) {
                                        contradiction = true;
                                        return;
                                    }
                                    progress = progress || (er == ApplyResult::Progress);
                                }
                            }

                            // Eliminacja w domenie Bloku (Tylko część wspólna Bloku - poza zbiorem A i C)
                            if (box_union != 0ULL) {
                                for (int p = box_p0; p < box_p1; ++p) {
                                    const int idx = st.topo->houses_flat[p];
                                    if (st.board->values[idx] != 0) continue;
                                    if (st.topo->cell_row[idx] == r) continue; // Węzeł A
                                    if (idx_equals_any_selected(idx, c_pool, ci1, ci2, ci3, ci4, ci5)) continue; // Węzeł C
                                    
                                    const ApplyResult er = st.eliminate(idx, box_union);
                                    if (er == ApplyResult::Contradiction) {
                                        contradiction = true;
                                        return;
                                    }
                                    progress = progress || (er == ApplyResult::Progress);
                                }
                            }
                        });
                    });
                    if (contradiction) return ApplyResult::Contradiction;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

// ----------------------------------------------------------------------------
// SCAN COL-BOX INTERSECTIONS
// ----------------------------------------------------------------------------
inline ApplyResult scan_col_box_intersections(CandidateState& st, bool& progress, StrategyStats& s, uint64_t t0) {
    const int n = st.topo->n;
    bool contradiction = false;

    // Przeszukujemy każdy blok na planszy
    for (int b = 0; b < n; ++b) {
        int cols[64];
        int col_cnt = 0;
        const int box_p0 = st.topo->house_offsets[2 * n + b];
        const int box_p1 = st.topo->house_offsets[2 * n + b + 1];

        // Wyznaczamy wszystkie kolumny przecinające ten blok (zabezpieczenie asymetrii)
        for (int p = box_p0; p < box_p1; ++p) {
            const int c = st.topo->cell_col[st.topo->houses_flat[p]];
            bool found = false;
            for (int i = 0; i < col_cnt; ++i) {
                if (cols[i] == c) { found = true; break; }
            }
            if (!found) cols[col_cnt++] = c;
        }

        // Testujemy każdą przecinającą kolumnę jako punkt intersekcji SDC
        for (int i = 0; i < col_cnt; ++i) {
            const int c = cols[i];

            int a_cells[64]{};
            int a_cnt = 0;
            uint64_t ma = 0ULL;
            int c_pool[64]{};
            int c_pool_cnt = 0;

            // Zbiór A (Część wspólna) i Zbiór C (Tylko Blok)
            for (int p = box_p0; p < box_p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] == 0) {
                    if (st.topo->cell_col[idx] == c) {
                        a_cells[a_cnt++] = idx;
                        ma |= st.cands[idx];
                    } else {
                        c_pool[c_pool_cnt++] = idx;
                    }
                }
            }
            if (a_cnt < 2) continue; // SDC wymaga min 2 komórek intersekcji

            int b_pool[64]{};
            int b_pool_cnt = 0;
            const int col_p0 = st.topo->house_offsets[n + c]; // Offsets dla kolumn zaczynają się od 'n'
            const int col_p1 = st.topo->house_offsets[n + c + 1];

            // Zbiór B (Tylko Kolumna)
            for (int p = col_p0; p < col_p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] == 0 && st.topo->cell_box[idx] != b) {
                    b_pool[b_pool_cnt++] = idx;
                }
            }

            const int select_cap = sue_de_coq_selection_cap(st);
            const int b_limit = std::min(b_pool_cnt, select_cap);
            const int c_limit = std::min(c_pool_cnt, select_cap);
            if (b_limit <= 0 || c_limit <= 0) continue;

            for (int b_choose = 1; b_choose <= b_limit; ++b_choose) {
                for (int c_choose = 1; c_choose <= c_limit; ++c_choose) {
                    const int target_cands = a_cnt + b_choose + c_choose;
                    if (std::popcount(ma) > target_cands) continue;

                    for_each_combo_up_to5(b_pool_cnt, b_choose, [&](int bi1, int bi2, int bi3, int bi4, int bi5) {
                        if (contradiction) return;
                        const uint64_t mb = pool_mask_from_selection(st, b_pool, bi1, bi2, bi3, bi4, bi5);
                        const uint64_t col_union = ma | mb;
                        if (std::popcount(col_union) != (a_cnt + b_choose)) return;

                        for_each_combo_up_to5(c_pool_cnt, c_choose, [&](int ci1, int ci2, int ci3, int ci4, int ci5) {
                            if (contradiction) return;
                            const uint64_t mc = pool_mask_from_selection(st, c_pool, ci1, ci2, ci3, ci4, ci5);
                            const uint64_t box_union = ma | mc;
                            if (std::popcount(box_union) != (a_cnt + c_choose)) return;

                            const uint64_t union_mask = col_union | mc;
                            if (std::popcount(union_mask) != target_cands) return;
                            
                            const uint64_t col_only = col_union & ~box_union;
                            const uint64_t box_only = box_union & ~col_union;
                            
                            // Ścisła walidacja Sue de Coq: Cyfry wykluczone muszą być stricte odseparowane
                            if (col_only == 0ULL || box_only == 0ULL) return;

                            // Eliminacja w domenie Kolumny (Tylko część wspólna Kolumny - poza zbiorem A i B)
                            if (col_union != 0ULL) {
                                for (int p = col_p0; p < col_p1; ++p) {
                                    const int idx = st.topo->houses_flat[p];
                                    if (st.board->values[idx] != 0) continue;
                                    if (st.topo->cell_box[idx] == b) continue; // Węzeł A
                                    if (idx_equals_any_selected(idx, b_pool, bi1, bi2, bi3, bi4, bi5)) continue; // Węzeł B
                                    
                                    const ApplyResult er = st.eliminate(idx, col_union);
                                    if (er == ApplyResult::Contradiction) {
                                        contradiction = true;
                                        return;
                                    }
                                    progress = progress || (er == ApplyResult::Progress);
                                }
                            }

                            // Eliminacja w domenie Bloku (Tylko część wspólna Bloku - poza zbiorem A i C)
                            if (box_union != 0ULL) {
                                for (int p = box_p0; p < box_p1; ++p) {
                                    const int idx = st.topo->houses_flat[p];
                                    if (st.board->values[idx] != 0) continue;
                                    if (st.topo->cell_col[idx] == c) continue; // Węzeł A
                                    if (idx_equals_any_selected(idx, c_pool, ci1, ci2, ci3, ci4, ci5)) continue; // Węzeł C
                                    
                                    const ApplyResult er = st.eliminate(idx, box_union);
                                    if (er == ApplyResult::Contradiction) {
                                        contradiction = true;
                                        return;
                                    }
                                    progress = progress || (er == ApplyResult::Progress);
                                }
                            }
                        });
                    });
                    if (contradiction) return ApplyResult::Contradiction;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

// ----------------------------------------------------------------------------
// GŁÓWNY PUNKT WEJŚCIA SUE DE COQ
// ----------------------------------------------------------------------------
inline ApplyResult apply_sue_de_coq(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool progress = false;
    {
        const ApplyResult rr = scan_row_box_intersections(st, progress, s, t0);
        if (rr == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return rr;
        }
        const ApplyResult rc = scan_col_box_intersections(st, progress, s, t0);
        if (rc == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return rc;
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_sue_de_coq = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
