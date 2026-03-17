//Author copyright Marcin Matysek (Rewertyn)
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <random>

#include "pattern_planter_exact_templates.h"

namespace sudoku_hpc::pattern_forcing {

namespace detail {

inline bool pf_is_valid_idx(const GenericTopology& topo, int idx) {
    return idx >= 0 && idx < topo.nn;
}

inline bool pf_cells_see_each_other(const GenericTopology& topo, int a, int b) {
    if (!pf_is_valid_idx(topo, a) || !pf_is_valid_idx(topo, b) || a == b) {
        return false;
    }
    return topo.cell_row[static_cast<size_t>(a)] == topo.cell_row[static_cast<size_t>(b)] ||
           topo.cell_col[static_cast<size_t>(a)] == topo.cell_col[static_cast<size_t>(b)] ||
           topo.cell_box[static_cast<size_t>(a)] == topo.cell_box[static_cast<size_t>(b)];
}

inline bool pf_add_anchor_protected(PatternScratch& sc, int idx) {
    if (!sc.add_anchor(idx)) {
        return false;
    }
    if (idx >= 0 && idx < sc.prepared_nn) {
        sc.protected_cells[static_cast<size_t>(idx)] = static_cast<uint8_t>(1);
    }
    return true;
}

inline int pf_rand_mod(std::mt19937_64& rng, int bound) {
    if (bound <= 0) {
        return 0;
    }
    return static_cast<int>(rng() % static_cast<uint64_t>(bound));
}

inline int pf_pick_distinct_index(std::mt19937_64& rng, int bound, int avoid) {
    if (bound <= 1) {
        return avoid;
    }
    int v = avoid;
    for (int t = 0; t < 128 && v == avoid; ++t) {
        v = pf_rand_mod(rng, bound);
    }
    return v;
}

inline int pf_pick_third_distinct_index(std::mt19937_64& rng, int bound, int a, int b) {
    if (bound <= 2) {
        return a;
    }
    int v = a;
    for (int t = 0; t < 128 && (v == a || v == b); ++t) {
        v = pf_rand_mod(rng, bound);
    }
    return v;
}

inline int pf_house_size(const GenericTopology& topo, int house) {
    return topo.house_offsets[static_cast<size_t>(house + 1)] - topo.house_offsets[static_cast<size_t>(house)];
}

inline int pf_house_pick_member(const GenericTopology& topo, int house, std::mt19937_64& rng) {
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    if (end <= begin) {
        return -1;
    }
    const int off = begin + pf_rand_mod(rng, end - begin);
    return topo.houses_flat[static_cast<size_t>(off)];
}

inline int pf_box_house(const GenericTopology& topo, int box) {
    return 2 * topo.n + box;
}

inline bool pf_add_house_members(PatternScratch& sc,
                                 const GenericTopology& topo,
                                 int house,
                                 int limit,
                                 int exclude_a = -1,
                                 int exclude_b = -1,
                                 int exclude_c = -1) {
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end && sc.anchor_count < limit; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (idx == exclude_a || idx == exclude_b || idx == exclude_c) {
            continue;
        }
        pf_add_anchor_protected(sc, idx);
    }
    return sc.anchor_count > 0;
}

inline bool pf_find_peer_in_box(const GenericTopology& topo, int base, int box, int& out_idx) {
    const int house = pf_box_house(topo, box);
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (idx != base && pf_cells_see_each_other(topo, base, idx)) {
            out_idx = idx;
            return true;
        }
    }
    return false;
}

inline bool pf_find_row_cells_in_box(const GenericTopology& topo, int box, int row, int& out_a, int& out_b) {
    out_a = -1;
    out_b = -1;
    const int house = pf_box_house(topo, box);
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (topo.cell_row[static_cast<size_t>(idx)] != row) {
            continue;
        }
        if (out_a < 0) {
            out_a = idx;
        } else if (idx != out_a) {
            out_b = idx;
            return true;
        }
    }
    return false;
}

inline bool pf_find_col_cells_in_box(const GenericTopology& topo, int box, int col, int& out_a, int& out_b) {
    out_a = -1;
    out_b = -1;
    const int house = pf_box_house(topo, box);
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (topo.cell_col[static_cast<size_t>(idx)] != col) {
            continue;
        }
        if (out_a < 0) {
            out_a = idx;
        } else if (idx != out_a) {
            out_b = idx;
            return true;
        }
    }
    return false;
}

inline int pf_pick_box_with_min_house_intersection(const GenericTopology& topo,
                                                   bool by_row,
                                                   int min_count,
                                                   std::mt19937_64& rng,
                                                   int* out_house_index = nullptr) {
    if (topo.box_rows <= 0 || topo.box_cols <= 0) {
        return -1;
    }
    const int box_count = topo.box_rows_count * topo.box_cols_count;
    const int start = pf_rand_mod(rng, std::max(1, box_count));
    for (int probe = 0; probe < box_count; ++probe) {
        const int box = (start + probe) % box_count;
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        if (by_row) {
            const int r0 = br * topo.box_rows;
            for (int dr = 0; dr < topo.box_rows; ++dr) {
                int a = -1, b = -1;
                if (pf_find_row_cells_in_box(topo, box, r0 + dr, a, b)) {
                    if (out_house_index) {
                        *out_house_index = r0 + dr;
                    }
                    return box;
                }
            }
        } else {
            const int c0 = bc * topo.box_cols;
            for (int dc = 0; dc < topo.box_cols; ++dc) {
                int a = -1, b = -1;
                if (pf_find_col_cells_in_box(topo, box, c0 + dc, a, b)) {
                    if (out_house_index) {
                        *out_house_index = c0 + dc;
                    }
                    return box;
                }
            }
        }
        (void)min_count;
    }
    return -1;
}

inline void pf_fill_anchor_masks(PatternScratch& sc, uint64_t mask) {
    for (int i = 0; i < sc.anchor_count; ++i) {
        sc.anchor_masks[static_cast<size_t>(i)] = mask;
        sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = mask;
    }
}

} // namespace detail

// --- Szablony luźne / pół-kanoniczne (fallback gdy brakuje Exact Template) ---
// Zmiany względem starej wersji:
//  - mocniejsze geometrie dla intersection/fish/remote-pairs,
//  - brak ukrytego założenia box_rows == box_cols,
//  - anchor builders starają się budować kanoniczny szkielet wzorca,
//  - anchors są od razu oznaczane jako protected, żeby digger nie zjadał rdzenia wzorca.

inline bool build_chain_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 2) {
        return false;
    }
    const int n = topo.n;
    const int r1 = detail::pf_rand_mod(rng, n);
    const int r2 = detail::pf_pick_distinct_index(rng, n, r1);
    const int c1 = detail::pf_rand_mod(rng, n);
    const int c2 = detail::pf_pick_distinct_index(rng, n, c1);
    if (r1 == r2 || c1 == c2) {
        return false;
    }
    detail::pf_add_anchor_protected(sc, r1 * n + c1);
    detail::pf_add_anchor_protected(sc, r1 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c1);
    return sc.anchor_count >= 4;
}

inline bool build_forcing_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng) || sc.anchor_count < 4) {
        return false;
    }
    const int n = topo.n;
    const int p = sc.anchors[0];
    const int a = sc.anchors[1];
    const int b = sc.anchors[2];

    int row_support = -1;
    const int row_a = topo.cell_row[static_cast<size_t>(a)];
    const int col_p = topo.cell_col[static_cast<size_t>(p)];
    for (int cc = 0; cc < n && row_support < 0; ++cc) {
        if (cc == topo.cell_col[static_cast<size_t>(p)] || cc == topo.cell_col[static_cast<size_t>(a)]) {
            continue;
        }
        row_support = row_a * n + cc;
    }

    int col_support = -1;
    const int col_b = topo.cell_col[static_cast<size_t>(b)];
    for (int rr = 0; rr < n && col_support < 0; ++rr) {
        if (rr == topo.cell_row[static_cast<size_t>(a)] || rr == topo.cell_row[static_cast<size_t>(b)]) {
            continue;
        }
        col_support = rr * n + col_b;
    }

    int cross_support = -1;
    for (int rr = 0; rr < n && cross_support < 0; ++rr) {
        if (rr == topo.cell_row[static_cast<size_t>(p)] || rr == topo.cell_row[static_cast<size_t>(b)]) {
            continue;
        }
        for (int cc = 0; cc < n; ++cc) {
            if (cc == col_p || cc == col_b) {
                continue;
            }
            const int idx = rr * n + cc;
            if (detail::pf_cells_see_each_other(topo, idx, a) || detail::pf_cells_see_each_other(topo, idx, b)) {
                cross_support = idx;
                break;
            }
        }
    }

    if (row_support >= 0) detail::pf_add_anchor_protected(sc, row_support);
    if (col_support >= 0) detail::pf_add_anchor_protected(sc, col_support);
    if (cross_support >= 0) detail::pf_add_anchor_protected(sc, cross_support);
    return sc.anchor_count >= 6;
}

inline bool build_exocet_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.box_rows <= 1 || topo.box_cols <= 1) {
        return false;
    }
    const int box_count = topo.box_rows_count * topo.box_cols_count;
    const int box = detail::pf_rand_mod(rng, box_count);
    const int house = detail::pf_box_house(topo, box);
    if (detail::pf_house_size(topo, house) < 2) {
        return false;
    }

    const int b1 = detail::pf_house_pick_member(topo, house, rng);
    if (b1 < 0) {
        return false;
    }

    int b2 = -1;
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (idx == b1) {
            continue;
        }
        if (topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(b1)]) {
            continue;
        }
        if (topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(b1)]) {
            continue;
        }
        b2 = idx;
        break;
    }
    if (b2 < 0) {
        return false;
    }

    detail::pf_add_anchor_protected(sc, b1);
    detail::pf_add_anchor_protected(sc, b2);

    const int base_br = box / topo.box_cols_count;
    const int base_bc = box % topo.box_cols_count;
    const int target_bc = detail::pf_pick_distinct_index(rng, topo.box_cols_count, base_bc);
    if (target_bc == base_bc) {
        return false;
    }

    const int r1 = topo.cell_row[static_cast<size_t>(b1)];
    const int r2 = topo.cell_row[static_cast<size_t>(b2)];
    const int c0 = target_bc * topo.box_cols;
    if (topo.box_cols < 2) {
        return false;
    }

    const int t1 = r1 * topo.n + c0;
    const int t2 = r2 * topo.n + std::min(c0 + 1, topo.n - 1);
    const int target_box = base_br * topo.box_cols_count + target_bc;
    if (topo.cell_box[static_cast<size_t>(t1)] != target_box ||
        topo.cell_box[static_cast<size_t>(t2)] != target_box ||
        t1 == t2) {
        return false;
    }

    detail::pf_add_anchor_protected(sc, t1);
    detail::pf_add_anchor_protected(sc, t2);
    return sc.anchor_count >= 4;
}

inline bool build_loop_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) {
        return false;
    }
    const int n = topo.n;
    const int r3 = detail::pf_rand_mod(rng, n);
    const int c3 = detail::pf_rand_mod(rng, n);
    const int r4 = detail::pf_pick_distinct_index(rng, n, r3);
    const int c4 = detail::pf_pick_distinct_index(rng, n, c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c3);
    detail::pf_add_anchor_protected(sc, r4 * n + c4);
    return sc.anchor_count >= 6;
}

inline bool build_color_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) {
        return false;
    }
    const int pivot = sc.anchors[0];
    const int begin = topo.peer_offsets[static_cast<size_t>(pivot)];
    const int end = topo.peer_offsets[static_cast<size_t>(pivot + 1)];
    for (int p = begin; p < end && sc.anchor_count < 6; ++p) {
        detail::pf_add_anchor_protected(sc, topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 5;
}

inline bool build_petal_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    const int pivot = detail::pf_rand_mod(rng, topo.nn);
    detail::pf_add_anchor_protected(sc, pivot);
    const int begin = topo.peer_offsets[static_cast<size_t>(pivot)];
    const int end = topo.peer_offsets[static_cast<size_t>(pivot + 1)];
    for (int p = begin; p < end && sc.anchor_count < 6; ++p) {
        detail::pf_add_anchor_protected(sc, topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 4;
}

inline bool build_intersection_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.box_rows <= 1 || topo.box_cols <= 1) {
        return false;
    }

    int house_index = -1;
    const bool by_row = ((rng() & 1ULL) == 0ULL);
    const int box = detail::pf_pick_box_with_min_house_intersection(topo, by_row, 2, rng, &house_index);
    if (box < 0 || house_index < 0) {
        return false;
    }

    int a = -1;
    int b = -1;
    if (by_row) {
        if (!detail::pf_find_row_cells_in_box(topo, box, house_index, a, b)) {
            return false;
        }
    } else {
        if (!detail::pf_find_col_cells_in_box(topo, box, house_index, a, b)) {
            return false;
        }
    }

    detail::pf_add_anchor_protected(sc, a);
    detail::pf_add_anchor_protected(sc, b);

    // Dodaj komórki wspierające w tej samej linii poza boksem.
    const int line_house = by_row ? house_index : (topo.n + house_index);
    const int begin = topo.house_offsets[static_cast<size_t>(line_house)];
    const int end = topo.house_offsets[static_cast<size_t>(line_house + 1)];
    for (int p = begin; p < end && sc.anchor_count < 5; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (topo.cell_box[static_cast<size_t>(idx)] == box) {
            continue;
        }
        detail::pf_add_anchor_protected(sc, idx);
    }

    // I resztę boxa dla szkieletu Sue de Coq / intersection.
    detail::pf_add_house_members(sc, topo, detail::pf_box_house(topo, box), 7, a, b);
    return sc.anchor_count >= 4;
}

inline bool build_fish_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 4) {
        return false;
    }
    const int n = topo.n;
    const int r1 = detail::pf_rand_mod(rng, n);
    const int r2 = detail::pf_pick_distinct_index(rng, n, r1);
    const int c1 = detail::pf_rand_mod(rng, n);
    const int c2 = detail::pf_pick_distinct_index(rng, n, c1);
    if (r1 == r2 || c1 == c2) {
        return false;
    }
    detail::pf_add_anchor_protected(sc, r1 * n + c1);
    detail::pf_add_anchor_protected(sc, r1 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c1);
    detail::pf_add_anchor_protected(sc, r2 * n + c2);

    // lekki fin / trzeci cover dla większej stabilności rodziny fish
    const int c3 = detail::pf_pick_third_distinct_index(rng, n, c1, c2);
    if (c3 != c1 && c3 != c2) {
        detail::pf_add_anchor_protected(sc, r1 * n + c3);
        detail::pf_add_anchor_protected(sc, r2 * n + c3);
    }
    return sc.anchor_count >= 4;
}

inline bool build_franken_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_fish_like_anchors(topo, sc, rng) || topo.box_rows <= 1 || topo.box_cols <= 1) {
        return false;
    }
    const int seed = sc.anchors[0];
    const int box = topo.cell_box[static_cast<size_t>(seed)];
    detail::pf_add_house_members(sc, topo, detail::pf_box_house(topo, box), 8, seed);
    return sc.anchor_count >= 5;
}

inline bool build_mutant_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_franken_like_anchors(topo, sc, rng)) {
        return false;
    }
    detail::pf_add_anchor_protected(sc, detail::pf_rand_mod(rng, topo.nn));
    detail::pf_add_anchor_protected(sc, detail::pf_rand_mod(rng, topo.nn));
    return sc.anchor_count >= 6;
}

inline bool build_squirm_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 5) {
        return false;
    }
    const int n = topo.n;
    int rows[5] = { detail::pf_rand_mod(rng, n), -1, -1, -1, -1 };
    int cols[5] = { detail::pf_rand_mod(rng, n), -1, -1, -1, -1 };

    for (int i = 1; i < 5; ++i) {
        rows[i] = detail::pf_pick_distinct_index(rng, n, rows[0]);
        for (int j = 0; j < i; ++j) {
            if (rows[i] == rows[j]) {
                rows[i] = detail::pf_pick_distinct_index(rng, n, rows[j]);
            }
        }
    }
    for (int i = 1; i < 5; ++i) {
        cols[i] = detail::pf_pick_distinct_index(rng, n, cols[0]);
        for (int j = 0; j < i; ++j) {
            if (cols[i] == cols[j]) {
                cols[i] = detail::pf_pick_distinct_index(rng, n, cols[j]);
            }
        }
    }
    for (int ri = 0; ri < 5; ++ri) {
        for (int ci = 0; ci < 5; ++ci) {
            detail::pf_add_anchor_protected(sc, rows[ri] * n + cols[ci]);
        }
    }
    return sc.anchor_count >= 25;
}

inline bool build_als_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 4) {
        return false;
    }
    const int pivot = detail::pf_rand_mod(rng, topo.nn);
    detail::pf_add_anchor_protected(sc, pivot);
    const int begin = topo.peer_offsets[static_cast<size_t>(pivot)];
    const int end = topo.peer_offsets[static_cast<size_t>(pivot + 1)];
    for (int p = begin; p < end && sc.anchor_count < 6; ++p) {
        detail::pf_add_anchor_protected(sc, topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 3;
}

inline bool build_exclusion_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    const int a = detail::pf_rand_mod(rng, topo.nn);
    detail::pf_add_anchor_protected(sc, a);
    const int begin = topo.peer_offsets[static_cast<size_t>(a)];
    const int end = topo.peer_offsets[static_cast<size_t>(a + 1)];
    for (int p = begin; p < end && sc.anchor_count < 4; ++p) {
        detail::pf_add_anchor_protected(sc, topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 2;
}

inline bool build_aic_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    return build_chain_anchors(topo, sc, rng);
}

inline bool build_grouped_aic_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) {
        return false;
    }
    const int seed = sc.anchors[0];
    const int box = topo.cell_box[static_cast<size_t>(seed)];
    detail::pf_add_house_members(sc, topo, detail::pf_box_house(topo, box), 7, seed);
    return sc.anchor_count >= 5;
}

inline bool build_grouped_cycle_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    return build_loop_like_anchors(topo, sc, rng);
}

inline bool build_niceloop_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    return build_loop_like_anchors(topo, sc, rng);
}

inline bool build_empty_rectangle_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_intersection_like_anchors(topo, sc, rng) || sc.anchor_count <= 0) {
        return false;
    }
    const int pivot = sc.anchors[0];
    const int row = topo.cell_row[static_cast<size_t>(pivot)];
    const int col = topo.cell_col[static_cast<size_t>(pivot)];
    for (int p = topo.peer_offsets[static_cast<size_t>(pivot)];
         p < topo.peer_offsets[static_cast<size_t>(pivot + 1)] && sc.anchor_count < 7; ++p) {
        const int idx = topo.peers_flat[static_cast<size_t>(p)];
        if (topo.cell_row[static_cast<size_t>(idx)] == row || topo.cell_col[static_cast<size_t>(idx)] == col) {
            detail::pf_add_anchor_protected(sc, idx);
        }
    }
    return sc.anchor_count >= 4;
}

inline bool build_remote_pairs_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 4) {
        return false;
    }
    const int n = topo.n;
    const int r1 = detail::pf_rand_mod(rng, n);
    const int r2 = detail::pf_pick_distinct_index(rng, n, r1);
    const int r3 = detail::pf_pick_third_distinct_index(rng, n, r1, r2);
    const int c1 = detail::pf_rand_mod(rng, n);
    const int c2 = detail::pf_pick_distinct_index(rng, n, c1);
    const int c3 = detail::pf_pick_third_distinct_index(rng, n, c1, c2);
    if (r1 == r2 || c1 == c2) {
        return false;
    }

    // Budujemy 6-komórkową ścieżkę o naprzemiennej widoczności, zamiast starego „prawie prostokąta”.
    detail::pf_add_anchor_protected(sc, r1 * n + c1);
    detail::pf_add_anchor_protected(sc, r1 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c1);
    return sc.anchor_count >= 6;
}

inline int default_anchor_count(const GenericTopology& topo, PatternKind kind) {
    if (topo.n >= 36) {
        switch (kind) {
        case PatternKind::ExocetLike: return 18;
        case PatternKind::LoopLike: return 20;
        case PatternKind::ForcingLike: return 20;
        case PatternKind::ColorLike: return 18;
        case PatternKind::PetalLike: return 18;
        case PatternKind::IntersectionLike: return 18;
        case PatternKind::FishLike: return 18;
        case PatternKind::FrankenLike: return 18;
        case PatternKind::MutantLike: return 18;
        case PatternKind::SquirmLike: return 20;
        case PatternKind::AlsLike: return 18;
        case PatternKind::ExclusionLike: return 16;
        case PatternKind::AicLike: return 18;
        case PatternKind::GroupedAicLike: return 18;
        case PatternKind::GroupedCycleLike: return 18;
        case PatternKind::NiceLoopLike: return 18;
        case PatternKind::XChainLike: return 18;
        case PatternKind::XYChainLike: return 18;
        case PatternKind::EmptyRectangleLike: return 16;
        case PatternKind::RemotePairsLike: return 18;
        case PatternKind::SwordfishLike: return 18;
        case PatternKind::JellyfishLike: return 20;
        case PatternKind::FinnedFishLike: return 18;
        case PatternKind::Chain: return 16;
        default: return 0;
        }
    }
    if (topo.n >= 25) {
        switch (kind) {
        case PatternKind::ExocetLike: return 14;
        case PatternKind::LoopLike: return 16;
        case PatternKind::ForcingLike: return 16;
        case PatternKind::ColorLike: return 14;
        case PatternKind::PetalLike: return 14;
        case PatternKind::IntersectionLike: return 14;
        case PatternKind::FishLike: return 14;
        case PatternKind::FrankenLike: return 14;
        case PatternKind::MutantLike: return 14;
        case PatternKind::SquirmLike: return 16;
        case PatternKind::AlsLike: return 14;
        case PatternKind::ExclusionLike: return 12;
        case PatternKind::AicLike: return 14;
        case PatternKind::GroupedAicLike: return 14;
        case PatternKind::GroupedCycleLike: return 14;
        case PatternKind::NiceLoopLike: return 14;
        case PatternKind::XChainLike: return 14;
        case PatternKind::XYChainLike: return 14;
        case PatternKind::EmptyRectangleLike: return 12;
        case PatternKind::RemotePairsLike: return 14;
        case PatternKind::SwordfishLike: return 14;
        case PatternKind::JellyfishLike: return 16;
        case PatternKind::FinnedFishLike: return 14;
        case PatternKind::Chain: return 12;
        default: return 0;
        }
    }
    switch (kind) {
    case PatternKind::ExocetLike: return std::clamp(topo.n / 2, 4, 10);
    case PatternKind::LoopLike: return std::clamp(topo.n / 2 + 2, 6, 12);
    case PatternKind::ForcingLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::ColorLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::PetalLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::IntersectionLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::FishLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::FrankenLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::MutantLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::SquirmLike: return std::clamp(topo.n / 2 + 2, 8, 14);
    case PatternKind::AlsLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::ExclusionLike: return std::clamp(topo.n / 3 + 4, 4, 10);
    case PatternKind::AicLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::GroupedAicLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::GroupedCycleLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::NiceLoopLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::XChainLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::XYChainLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::EmptyRectangleLike: return std::clamp(topo.n / 3 + 4, 4, 10);
    case PatternKind::RemotePairsLike: return std::clamp(topo.n / 2, 7, 12);
    case PatternKind::SwordfishLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::JellyfishLike: return std::clamp(topo.n / 2 + 2, 8, 14);
    case PatternKind::FinnedFishLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::Chain: return std::clamp(topo.n / 3 + 3, 4, 10);
    default: return 0;
    }
}

inline void apply_anchor_masks(const GenericTopology& topo, PatternScratch& sc, PatternKind kind, std::mt19937_64& rng) {
    if (sc.anchor_count <= 0) {
        return;
    }
    const uint64_t full = pf_full_mask_for_n(topo.n);
    const bool large_geom = topo.n >= 25;
    const int want_a = large_geom ? 3 : 2;
    const int want_b = large_geom ? 4 : 2;
    const int want_c = large_geom ? 5 : 3;
    uint64_t mask_a = random_digit_mask(topo.n, want_a, rng);
    uint64_t mask_b = random_digit_mask(topo.n, want_b, rng);
    uint64_t mask_c = random_digit_mask(topo.n, want_c, rng);

    if (kind == PatternKind::ExocetLike) {
        const uint64_t shared = random_digit_mask(topo.n, large_geom ? 4 : 3, rng);
        if (sc.anchor_count >= 1) sc.allowed_masks[static_cast<size_t>(sc.anchors[0])] = shared;
        if (sc.anchor_count >= 2) sc.allowed_masks[static_cast<size_t>(sc.anchors[1])] = shared;
        for (int i = 2; i < sc.anchor_count; ++i) {
            const int idx = sc.anchors[static_cast<size_t>(i)];
            sc.allowed_masks[static_cast<size_t>(idx)] =
                large_geom ? ((mask_b | random_extra_digit(full, mask_b, rng)) & full) : (mask_c & full);
        }
        return;
    }

    if (kind == PatternKind::ForcingLike) {
        const uint64_t d1 = random_digit_mask(topo.n, 1, rng);
        const uint64_t d2 = random_extra_digit(full, d1, rng);
        const uint64_t d3 = random_extra_digit(full, d1 | d2, rng);
        const uint64_t d4 = random_extra_digit(full, d1 | d2 | d3, rng);
        const uint64_t d5 = random_extra_digit(full, d1 | d2 | d3 | d4, rng);
        const uint64_t m12 = (d1 | d2) & full;
        const uint64_t m23 = (d2 | d3) & full;
        const uint64_t m13 = (d1 | d3) & full;
        const uint64_t pivot_mask = (m12 | d4) & full;
        const uint64_t branch_a_mask = (m23 | d4) & full;
        const uint64_t branch_b_mask = (m13 | d5) & full;
        const uint64_t target_mask = (m12 | d5) & full;
        uint64_t row_support_mask = (m12 | d3 | d5) & full;
        uint64_t col_support_mask = (m23 | d1 | d5) & full;
        uint64_t cross_support_mask = (m13 | d2 | d4) & full;
        if (large_geom) {
            row_support_mask |= random_extra_digit(full, row_support_mask, rng);
            col_support_mask |= random_extra_digit(full, col_support_mask, rng);
            cross_support_mask |= random_extra_digit(full, cross_support_mask, rng);
        }
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = target_mask;
            switch (i) {
            case 0: m = pivot_mask; break;
            case 1: m = branch_a_mask; break;
            case 2: m = branch_b_mask; break;
            case 3: m = target_mask; break;
            case 4: m = row_support_mask; break;
            case 5: m = col_support_mask; break;
            default: m = cross_support_mask; break;
            }
            const int idx = sc.anchors[static_cast<size_t>(i)];
            sc.allowed_masks[static_cast<size_t>(idx)] = m & full;
        }
        return;
    }

    if (kind == PatternKind::LoopLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i % 3 == 0) ? mask_c : ((i & 1) ? mask_a : mask_b);
            if (large_geom && (i % 4 == 0)) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::ColorLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = ((i & 1) == 0) ? mask_a : (mask_a | random_extra_digit(full, mask_a, rng));
            if (std::popcount(m) < 2) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::PetalLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i == 0) ? (mask_a | mask_b) : ((i & 1) ? mask_a : mask_b);
            if (i == 0 || (large_geom && (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::IntersectionLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        const uint64_t box_support = (core | random_extra_digit(full, core, rng)) & full;
        const uint64_t line_support = (core | random_extra_digit(full, core, rng) | random_extra_digit(full, core, rng)) & full;
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i < 2) ? core : ((i < 5) ? line_support : box_support);
            if (std::popcount(m) < 2) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::FishLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = core;
            if (i >= 4 || (large_geom && (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::FrankenLike || kind == PatternKind::MutantLike || kind == PatternKind::SquirmLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = core;
            if (kind == PatternKind::SquirmLike) {
                if (i >= 9 || (large_geom && (i % 4 == 0))) {
                    m |= random_extra_digit(full, m, rng);
                }
            } else if (i >= 4 || (large_geom && (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            if (kind == PatternKind::MutantLike && i >= 5) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::SwordfishLike || kind == PatternKind::JellyfishLike || kind == PatternKind::FinnedFishLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = core;
            if (kind == PatternKind::JellyfishLike && (i >= 9 || (large_geom && (i % 4 == 0)))) {
                m |= random_extra_digit(full, m, rng);
            }
            if (kind == PatternKind::FinnedFishLike && (i >= 4 || (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::AlsLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i % 3 == 0) ? (mask_a | mask_b) : (mask_b | mask_c);
            if (std::popcount(m) < 3) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::ExclusionLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i & 1) ? mask_a : mask_b;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::AicLike || kind == PatternKind::GroupedAicLike ||
        kind == PatternKind::GroupedCycleLike || kind == PatternKind::NiceLoopLike ||
        kind == PatternKind::XChainLike || kind == PatternKind::XYChainLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i & 1) ? mask_a : mask_b;
            if (kind == PatternKind::GroupedAicLike || kind == PatternKind::NiceLoopLike || (i % 3 == 0)) {
                m |= random_extra_digit(full, m, rng);
            }
            if (kind == PatternKind::GroupedCycleLike && (i % 2 == 0)) {
                m = mask_a;
            }
            if (kind == PatternKind::XYChainLike && (i % 2 == 1)) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::EmptyRectangleLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        const uint64_t support = (core | random_extra_digit(full, core, rng)) & full;
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i < 3) ? core : support;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    if (kind == PatternKind::RemotePairsLike) {
        const uint64_t pair = random_digit_mask(topo.n, 2, rng);
        uint64_t victim = (pair | random_extra_digit(full, pair, rng)) & full;
        if (std::popcount(victim) < 3) {
            victim |= random_extra_digit(full, victim, rng);
        }
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i < 4) ? pair : victim;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        return;
    }

    // Domyślnie Chain.
    for (int i = 0; i < sc.anchor_count; ++i) {
        uint64_t m = (i & 1) ? mask_a : mask_b;
        if (large_geom && (i % 3 == 2)) {
            m |= random_extra_digit(full, m, rng);
        }
        sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
    }
}

} // namespace sudoku_hpc::pattern_forcing
