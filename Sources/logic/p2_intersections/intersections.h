// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: intersections.h (Poziom 1 / P1 Easy)
// Opis: Pointing Pairs/Triples oraz Box/Line Reduction (Intersection Removal)
//       Zero-allocation, odporne na asymetryczne geometrie boxów.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p2_intersections {

namespace detail {

inline ApplyResult eliminate_from_row_outside_box(
    CandidateState& st,
    int row,
    int box_col0,
    int box_cols,
    uint64_t bit,
    bool& progress) {

    const int n = st.topo->n;
    for (int c = 0; c < n; ++c) {
        if (c >= box_col0 && c < (box_col0 + box_cols)) {
            continue;
        }
        const int idx = row * n + c;
        const ApplyResult er = st.eliminate(idx, bit);
        if (er == ApplyResult::Contradiction) {
            return er;
        }
        progress = progress || (er == ApplyResult::Progress);
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult eliminate_from_col_outside_box(
    CandidateState& st,
    int col,
    int box_row0,
    int box_rows,
    uint64_t bit,
    bool& progress) {

    const int n = st.topo->n;
    for (int r = 0; r < n; ++r) {
        if (r >= box_row0 && r < (box_row0 + box_rows)) {
            continue;
        }
        const int idx = r * n + col;
        const ApplyResult er = st.eliminate(idx, bit);
        if (er == ApplyResult::Contradiction) {
            return er;
        }
        progress = progress || (er == ApplyResult::Progress);
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult eliminate_from_box_outside_row(
    CandidateState& st,
    int box_index,
    int keep_row,
    uint64_t bit,
    bool& progress) {

    const int p0 = st.topo->house_offsets[2 * st.topo->n + box_index];
    const int p1 = st.topo->house_offsets[2 * st.topo->n + box_index + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_row[idx] == keep_row) {
            continue;
        }
        const ApplyResult er = st.eliminate(idx, bit);
        if (er == ApplyResult::Contradiction) {
            return er;
        }
        progress = progress || (er == ApplyResult::Progress);
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult eliminate_from_box_outside_col(
    CandidateState& st,
    int box_index,
    int keep_col,
    uint64_t bit,
    bool& progress) {

    const int p0 = st.topo->house_offsets[2 * st.topo->n + box_index];
    const int p1 = st.topo->house_offsets[2 * st.topo->n + box_index + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_col[idx] == keep_col) {
            continue;
        }
        const ApplyResult er = st.eliminate(idx, bit);
        if (er == ApplyResult::Contradiction) {
            return er;
        }
        progress = progress || (er == ApplyResult::Progress);
    }
    return ApplyResult::NoProgress;
}

} // namespace detail

// ----------------------------------------------------------------------------
// POINTING PAIRS / TRIPLES
// Kandydat w obrębie boxa występuje tylko w jednym rzędzie lub jednej kolumnie,
// więc można go usunąć z reszty tego rzędu / kolumny poza boxem.
// ----------------------------------------------------------------------------
inline ApplyResult apply_pointing_pairs(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {

    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int box_rows = st.topo->box_rows;
    const int box_cols = st.topo->box_cols;
    const int box_rows_count = st.topo->box_rows_count;
    const int box_cols_count = st.topo->box_cols_count;

    bool progress = false;

    for (int brg = 0; brg < box_rows_count; ++brg) {
        const int r0 = brg * box_rows;
        for (int bcg = 0; bcg < box_cols_count; ++bcg) {
            const int c0 = bcg * box_cols;

            for (int d = 1; d <= n; ++d) {
                const uint64_t bit = (1ULL << (d - 1));

                int first_row = -1;
                int first_col = -1;
                int count = 0;
                bool same_row = true;
                bool same_col = true;

                for (int dr = 0; dr < box_rows; ++dr) {
                    const int rr = r0 + dr;
                    for (int dc = 0; dc < box_cols; ++dc) {
                        const int cc = c0 + dc;
                        const int idx = rr * n + cc;

                        if (st.board->values[idx] != 0) {
                            continue;
                        }
                        if ((st.cands[idx] & bit) == 0ULL) {
                            continue;
                        }

                        if (count == 0) {
                            first_row = rr;
                            first_col = cc;
                        } else {
                            same_row = same_row && (rr == first_row);
                            same_col = same_col && (cc == first_col);
                        }
                        ++count;
                    }
                }

                if (count < 2) {
                    continue;
                }

                if (same_row) {
                    const ApplyResult er = detail::eliminate_from_row_outside_box(
                        st, first_row, c0, box_cols, bit, progress);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return er;
                    }
                }
                if (same_col) {
                    const ApplyResult er = detail::eliminate_from_col_outside_box(
                        st, first_col, r0, box_rows, bit, progress);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return er;
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_pointing_pairs = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

// ----------------------------------------------------------------------------
// BOX / LINE REDUCTION
// Kandydat w całym rzędzie lub kolumnie występuje wyłącznie wewnątrz jednego boxa,
// więc można go usunąć z pozostałych komórek tego boxa poza tym rzędem/kolumną.
// ----------------------------------------------------------------------------
inline ApplyResult apply_box_line_reduction(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {

    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool progress = false;

    // Rzędy -> box
    for (int row = 0; row < n; ++row) {
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));

            int target_box = -1;
            int count = 0;
            bool same_box = true;

            const int p0 = st.topo->house_offsets[row];
            const int p1 = st.topo->house_offsets[row + 1];
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] != 0) {
                    continue;
                }
                if ((st.cands[idx] & bit) == 0ULL) {
                    continue;
                }

                const int box = st.topo->cell_box[idx];
                if (count == 0) {
                    target_box = box;
                } else {
                    same_box = same_box && (box == target_box);
                }
                ++count;
            }

            if (count < 2 || !same_box || target_box < 0) {
                continue;
            }

            const ApplyResult er = detail::eliminate_from_box_outside_row(
                st, target_box, row, bit, progress);
            if (er == ApplyResult::Contradiction) {
                s.elapsed_ns += st.now_ns() - t0;
                return er;
            }
        }
    }

    // Kolumny -> box
    for (int col = 0; col < n; ++col) {
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));

            int target_box = -1;
            int count = 0;
            bool same_box = true;

            const int p0 = st.topo->house_offsets[n + col];
            const int p1 = st.topo->house_offsets[n + col + 1];
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] != 0) {
                    continue;
                }
                if ((st.cands[idx] & bit) == 0ULL) {
                    continue;
                }

                const int box = st.topo->cell_box[idx];
                if (count == 0) {
                    target_box = box;
                } else {
                    same_box = same_box && (box == target_box);
                }
                ++count;
            }

            if (count < 2 || !same_box || target_box < 0) {
                continue;
            }

            const ApplyResult er = detail::eliminate_from_box_outside_col(
                st, target_box, col, bit, progress);
            if (er == ApplyResult::Contradiction) {
                s.elapsed_ns += st.now_ns() - t0;
                return er;
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_box_line = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

// ----------------------------------------------------------------------------
// Wspólny wrapper zachowany dla kompatybilności wstecznej.
// Najpierw Pointing, potem Box-Line, z oddzielnymi statystykami i flagami.
// ----------------------------------------------------------------------------
inline ApplyResult apply_pointing_and_boxline(
    CandidateState& st,
    StrategyStats& sp,
    StrategyStats& sb,
    GenericLogicCertifyResult& r) {

    const ApplyResult rp = apply_pointing_pairs(st, sp, r);
    if (rp != ApplyResult::NoProgress) {
        return rp;
    }
    return apply_box_line_reduction(st, sb, r);
}

} // namespace sudoku_hpc::logic::p2_intersections
