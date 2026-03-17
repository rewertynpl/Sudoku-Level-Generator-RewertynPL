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
    if (v == avoid) {
        v = (avoid + 1) % bound;
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
    if (v == a || v == b) {
        for (int i = 0; i < bound; ++i) {
            if (i != a && i != b) {
                v = i;
                break;
            }
        }
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

inline bool pf_find_cell_in_box_row(const GenericTopology& topo, int box, int row, int exclude_a, int exclude_b, int& out_idx) {
    out_idx = -1;
    const int house = pf_box_house(topo, box);
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (idx == exclude_a || idx == exclude_b) continue;
        if (topo.cell_row[static_cast<size_t>(idx)] != row) continue;
        out_idx = idx;
        return true;
    }
    return false;
}

inline bool pf_find_cell_in_box_col(const GenericTopology& topo, int box, int col, int exclude_a, int exclude_b, int& out_idx) {
    out_idx = -1;
    const int house = pf_box_house(topo, box);
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (idx == exclude_a || idx == exclude_b) continue;
        if (topo.cell_col[static_cast<size_t>(idx)] != col) continue;
        out_idx = idx;
        return true;
    }
    return false;
}

inline bool pf_collect_house_members(const GenericTopology& topo, int house, int* out_cells, int cap, int& out_count) {
    out_count = 0;
    if (cap <= 0) return false;
    const int begin = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = begin; p < end && out_count < cap; ++p) {
        out_cells[out_count++] = topo.houses_flat[static_cast<size_t>(p)];
    }
    return out_count > 0;
}

inline bool pf_pick_two_distinct_members(const GenericTopology& topo,
                                         int house,
                                         std::mt19937_64& rng,
                                         int& out_a,
                                         int& out_b) {
    int cells[64]{};
    int count = 0;
    if (!pf_collect_house_members(topo, house, cells, 64, count) || count < 2) {
        out_a = -1;
        out_b = -1;
        return false;
    }
    const int i1 = pf_rand_mod(rng, count);
    int i2 = pf_pick_distinct_index(rng, count, i1);
    if (i2 == i1) return false;
    out_a = cells[i1];
    out_b = cells[i2];
    return true;
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
        const int house = pf_box_house(topo, box);
        const int begin = topo.house_offsets[static_cast<size_t>(house)];
        const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
        int line_seen[64]{};
        int line_count[64]{};
        int unique_lines = 0;
        for (int p = begin; p < end; ++p) {
            const int idx = topo.houses_flat[static_cast<size_t>(p)];
            const int line = by_row ? topo.cell_row[static_cast<size_t>(idx)] : topo.cell_col[static_cast<size_t>(idx)];
            int pos = -1;
            for (int i = 0; i < unique_lines; ++i) {
                if (line_seen[i] == line) { pos = i; break; }
            }
            if (pos < 0) {
                pos = unique_lines;
                line_seen[unique_lines++] = line;
            }
            ++line_count[pos];
        }
        for (int i = 0; i < unique_lines; ++i) {
            if (line_count[i] >= min_count) {
                if (out_house_index) *out_house_index = line_seen[i];
                return box;
            }
        }
    }
    return -1;
}

inline void pf_fill_anchor_masks(PatternScratch& sc, uint64_t mask) {
    for (int i = 0; i < sc.anchor_count; ++i) {
        sc.anchor_masks[static_cast<size_t>(i)] = mask;
        sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = mask;
    }
}

inline void pf_mark_structure_protected(PatternScratch& sc, bool corridor, bool anti_single) {
    sc.corridor_protected = sc.corridor_protected || corridor;
    sc.anti_single_protected = sc.anti_single_protected || anti_single;
}

} // namespace detail

// --- Szablony luźne / pół-kanoniczne (fallback gdy brakuje Exact Template) ---

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
    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
    return sc.anchor_count >= 6;
}

inline bool build_exocet_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.box_rows <= 1 || topo.box_cols <= 1) {
        return false;
    }
    const int box_count = topo.box_rows_count * topo.box_cols_count;
    if (box_count < 2) {
        return false;
    }

    const int start_box = detail::pf_rand_mod(rng, box_count);
    for (int probe = 0; probe < box_count; ++probe) {
        const int box = (start_box + probe) % box_count;
        const int house = detail::pf_box_house(topo, box);
        if (detail::pf_house_size(topo, house) < 2) {
            continue;
        }

        int b1 = -1;
        int b2 = -1;
        const int begin = topo.house_offsets[static_cast<size_t>(house)];
        const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
        for (int p = begin; p < end && b2 < 0; ++p) {
            const int a = topo.houses_flat[static_cast<size_t>(p)];
            for (int q = p + 1; q < end; ++q) {
                const int c = topo.houses_flat[static_cast<size_t>(q)];
                if (topo.cell_row[static_cast<size_t>(a)] == topo.cell_row[static_cast<size_t>(c)]) continue;
                if (topo.cell_col[static_cast<size_t>(a)] == topo.cell_col[static_cast<size_t>(c)]) continue;
                b1 = a;
                b2 = c;
                break;
            }
        }
        if (b1 < 0 || b2 < 0) {
            continue;
        }

        const int row1 = topo.cell_row[static_cast<size_t>(b1)];
        const int row2 = topo.cell_row[static_cast<size_t>(b2)];
        const int col1 = topo.cell_col[static_cast<size_t>(b1)];
        const int col2 = topo.cell_col[static_cast<size_t>(b2)];

        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;

        for (int t = 0; t < box_count; ++t) {
            if (t == box) continue;
            const int tr = t / topo.box_cols_count;
            const int tc = t % topo.box_cols_count;

            // Target box musi wspoldzielic bande wierszy (row band) lub kolumn (col band) 
            // - chroni przed asymetrycznym mismatchowaniem
            bool try_rows = (tr == br);
            bool try_cols = (tc == bc);
            if (!try_rows && !try_cols) continue;

            int t1 = -1, t2 = -1;
            bool ok = false;
            
            if (try_rows) {
                ok = detail::pf_find_cell_in_box_row(topo, t, row1, -1, -1, t1) &&
                     detail::pf_find_cell_in_box_row(topo, t, row2, t1, -1, t2);
            }
            if (!ok && try_cols) {
                t1 = -1; t2 = -1;
                ok = detail::pf_find_cell_in_box_col(topo, t, col1, -1, -1, t1) &&
                     detail::pf_find_cell_in_box_col(topo, t, col2, t1, -1, t2);
            }
            
            if (ok && t1 >= 0 && t2 >= 0 && t1 != t2) {
                detail::pf_add_anchor_protected(sc, b1);
                detail::pf_add_anchor_protected(sc, b2);
                detail::pf_add_anchor_protected(sc, t1);
                detail::pf_add_anchor_protected(sc, t2);

                const int tbox = topo.cell_box[static_cast<size_t>(t1)];
                detail::pf_add_house_members(sc, topo, detail::pf_box_house(topo, tbox), 6, t1, t2);
                detail::pf_mark_structure_protected(sc, true, true);
                return sc.anchor_count >= 4;
            }
        }
    }
    return false;
}

inline bool build_loop_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 3) {
        return false;
    }
    const int n = topo.n;
    
    // Tworzy prawidlowy 6-elementowy zamkniety cykl geometryczny (Proper Loop)
    const int r1 = detail::pf_rand_mod(rng, n);
    const int r2 = detail::pf_pick_distinct_index(rng, n, r1);
    const int r3 = detail::pf_pick_third_distinct_index(rng, n, r1, r2);
    
    const int c1 = detail::pf_rand_mod(rng, n);
    const int c2 = detail::pf_pick_distinct_index(rng, n, c1);
    const int c3 = detail::pf_pick_third_distinct_index(rng, n, c1, c2);

    if (r1 == r2 || r1 == r3 || r2 == r3) return false;
    if (c1 == c2 || c1 == c3 || c2 == c3) return false;

    detail::pf_add_anchor_protected(sc, r1 * n + c1);
    detail::pf_add_anchor_protected(sc, r1 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c1);

    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
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

    const int line_house = by_row ? house_index : (topo.n + house_index);
    const int begin = topo.house_offsets[static_cast<size_t>(line_house)];
    const int end = topo.house_offsets[static_cast<size_t>(line_house + 1)];
    int external_support = 0;
    for (int p = begin; p < end && sc.anchor_count < 6; ++p) {
        const int idx = topo.houses_flat[static_cast<size_t>(p)];
        if (topo.cell_box[static_cast<size_t>(idx)] == box) {
            continue;
        }
        if (detail::pf_add_anchor_protected(sc, idx)) {
            ++external_support;
        }
    }

    detail::pf_add_house_members(sc, topo, detail::pf_box_house(topo, box), 8, a, b);
    detail::pf_mark_structure_protected(sc, true, true);
    return sc.anchor_count >= 4 && external_support >= 1;
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

    const int c3 = detail::pf_pick_third_distinct_index(rng, n, c1, c2);
    if (c3 != c1 && c3 != c2) {
        detail::pf_add_anchor_protected(sc, r1 * n + c3);
        detail::pf_add_anchor_protected(sc, r2 * n + c3);
    }
    detail::pf_mark_structure_protected(sc, true, true);
    return sc.anchor_count >= 4;
}

inline bool build_franken_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_fish_like_anchors(topo, sc, rng) || topo.box_rows <= 1 || topo.box_cols <= 1) {
        return false;
    }
    const int seed = sc.anchors[0];
    const int box = topo.cell_box[static_cast<size_t>(seed)];
    detail::pf_add_house_members(sc, topo, detail::pf_box_house(topo, box), 8, seed);
    detail::pf_mark_structure_protected(sc, true, true);
    return sc.anchor_count >= 5;
}

inline bool build_mutant_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_franken_like_anchors(topo, sc, rng)) {
        return false;
    }
    detail::pf_add_anchor_protected(sc, detail::pf_rand_mod(rng, topo.nn));
    detail::pf_add_anchor_protected(sc, detail::pf_rand_mod(rng, topo.nn));
    detail::pf_mark_structure_protected(sc, true, true);
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
        int candidate = detail::pf_rand_mod(rng, n);
        for (int tries = 0; tries < 128; ++tries) {
            bool used = false;
            for (int j = 0; j < i; ++j) used = used || (rows[j] == candidate);
            if (!used) break;
            candidate = detail::pf_rand_mod(rng, n);
        }
        rows[i] = candidate;
        for (int j = 0; j < i; ++j) {
            if (rows[i] == rows[j]) {
                rows[i] = (rows[j] + i + 1) % n;
            }
        }
    }
    for (int i = 1; i < 5; ++i) {
        int candidate = detail::pf_rand_mod(rng, n);
        for (int tries = 0; tries < 128; ++tries) {
            bool used = false;
            for (int j = 0; j < i; ++j) used = used || (cols[j] == candidate);
            if (!used) break;
            candidate = detail::pf_rand_mod(rng, n);
        }
        cols[i] = candidate;
        for (int j = 0; j < i; ++j) {
            if (cols[i] == cols[j]) {
                cols[i] = (cols[j] + i + 1) % n;
            }
        }
    }
    for (int ri = 0; ri < 5; ++ri) {
        for (int ci = 0; ci < 5; ++ci) {
            detail::pf_add_anchor_protected(sc, rows[ri] * n + cols[ci]);
        }
    }
    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
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
    detail::pf_mark_structure_protected(sc, true, true);
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

    detail::pf_add_anchor_protected(sc, r1 * n + c1);
    detail::pf_add_anchor_protected(sc, r1 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c2);
    detail::pf_add_anchor_protected(sc, r2 * n + c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c3);
    detail::pf_add_anchor_protected(sc, r3 * n + c1);
    detail::pf_mark_structure_protected(sc, true, true);
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
        const uint64_t target_core = random_digit_mask(topo.n, 2, rng);
        uint64_t base_mask = target_core;
        
        while(std::popcount(base_mask) < (large_geom ? 4 : 3)) {
            base_mask |= (1ULL << (rng() % topo.n));
        }
        
        if (sc.anchor_count >= 1) sc.allowed_masks[static_cast<size_t>(sc.anchors[0])] = base_mask & full;
        if (sc.anchor_count >= 2) sc.allowed_masks[static_cast<size_t>(sc.anchors[1])] = base_mask & full;
        
        for (int i = 2; i < sc.anchor_count; ++i) {
            const int idx = sc.anchors[static_cast<size_t>(i)];
            uint64_t target_mask = target_core;
            while(std::popcount(target_mask) < (large_geom ? 5 : 4)) {
                target_mask |= (1ULL << (rng() % topo.n));
            }
            sc.allowed_masks[static_cast<size_t>(idx)] = target_mask & full;
        }
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
        return;
    }

    if (kind == PatternKind::ExclusionLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i & 1) ? mask_a : mask_b;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
        return;
    }

    if (kind == PatternKind::EmptyRectangleLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        const uint64_t support = (core | random_extra_digit(full, core, rng)) & full;
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i < 3) ? core : support;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = m & full;
        }
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
        sc.corridor_protected = true;
        sc.anti_single_protected = true;
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
    sc.corridor_protected = true;
    sc.anti_single_protected = true;
}

} // namespace sudoku_hpc::pattern_forcing