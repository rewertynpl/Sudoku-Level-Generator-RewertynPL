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

inline bool row_has_digit_outside_box(
    const CandidateState& st,
    int row,
    int box_index,
    uint64_t bit) {

    const int p0 = st.topo->house_offsets[row];
    const int p1 = st.topo->house_offsets[row + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_box[idx] == box_index) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) != 0ULL) {
            return true;
        }
    }
    return false;
}

inline bool col_has_digit_outside_box(
    const CandidateState& st,
    int col,
    int box_index,
    uint64_t bit) {

    const int n = st.topo->n;
    const int house = n + col;
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_box[idx] == box_index) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) != 0ULL) {
            return true;
        }
    }
    return false;
}

inline bool box_has_digit_outside_row(
    const CandidateState& st,
    int box_index,
    int keep_row,
    uint64_t bit) {

    const int n = st.topo->n;
    const int house = 2 * n + box_index;
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_row[idx] == keep_row) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) != 0ULL) {
            return true;
        }
    }
    return false;
}

inline bool box_has_digit_outside_col(
    const CandidateState& st,
    int box_index,
    int keep_col,
    uint64_t bit) {

    const int n = st.topo->n;
    const int house = 2 * n + box_index;
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_col[idx] == keep_col) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) != 0ULL) {
            return true;
        }
    }
    return false;
}

inline ApplyResult eliminate_from_row_outside_box(
    CandidateState& st,
    int row,
    int box_index,
    uint64_t bit,
    bool& progress) {

    const int p0 = st.topo->house_offsets[row];
    const int p1 = st.topo->house_offsets[row + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_box[idx] == box_index) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) == 0ULL) {
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

inline ApplyResult eliminate_from_col_outside_box(
    CandidateState& st,
    int col,
    int box_index,
    uint64_t bit,
    bool& progress) {

    const int n = st.topo->n;
    const int house = n + col;
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_box[idx] == box_index) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) == 0ULL) {
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

inline ApplyResult eliminate_from_box_outside_row(
    CandidateState& st,
    int box_index,
    int keep_row,
    uint64_t bit,
    bool& progress) {

    const int n = st.topo->n;
    const int house = 2 * n + box_index;
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_row[idx] == keep_row) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) == 0ULL) {
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

    const int n = st.topo->n;
    const int house = 2 * n + box_index;
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[p];
        if (st.topo->cell_col[idx] == keep_col) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        if ((st.cands[idx] & bit) == 0ULL) {
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
    bool progress = false;

    for (int box = 0; box < n; ++box) {
        const int house = 2 * n + box;
        const int p0 = st.topo->house_offsets[house];
        const int p1 = st.topo->house_offsets[house + 1];

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));

            int first_row = -1;
            int first_col = -1;
            int count = 0;
            bool same_row = true;
            bool same_col = true;

            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] != 0) {
                    continue;
                }
                if ((st.cands[idx] & bit) == 0ULL) {
                    continue;
                }

                const int row = st.topo->cell_row[idx];
                const int col = st.topo->cell_col[idx];
                if (count == 0) {
                    first_row = row;
                    first_col = col;
                } else {
                    same_row = same_row && (row == first_row);
                    same_col = same_col && (col == first_col);
                }
                ++count;
            }

            if (count < 2) {
                continue;
            }

            if (same_row && first_row >= 0 && detail::row_has_digit_outside_box(st, first_row, box, bit)) {
                const ApplyResult er = detail::eliminate_from_row_outside_box(
                    st, first_row, box, bit, progress);
                if (er == ApplyResult::Contradiction) {
                    s.elapsed_ns += st.now_ns() - t0;
                    return er;
                }
            }

            if (same_col && first_col >= 0 && detail::col_has_digit_outside_box(st, first_col, box, bit)) {
                const ApplyResult er = detail::eliminate_from_col_outside_box(
                    st, first_col, box, bit, progress);
                if (er == ApplyResult::Contradiction) {
                    s.elapsed_ns += st.now_ns() - t0;
                    return er;
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
        const int p0 = st.topo->house_offsets[row];
        const int p1 = st.topo->house_offsets[row + 1];

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));

            int target_box = -1;
            int count = 0;
            bool same_box = true;

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
            if (!detail::box_has_digit_outside_row(st, target_box, row, bit)) {
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
        const int house = n + col;
        const int p0 = st.topo->house_offsets[house];
        const int p1 = st.topo->house_offsets[house + 1];

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));

            int target_box = -1;
            int count = 0;
            bool same_box = true;

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
            if (!detail::box_has_digit_outside_col(st, target_box, col, bit)) {
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
