//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateP6Exact {
private:
    static int random_distinct_index(int n, int avoid, std::mt19937_64& rng) {
        int out = avoid;
        for (int g = 0; g < 64 && out == avoid; ++g) {
            out = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return out;
    }

    static int random_third_index(int n, int a, int b, std::mt19937_64& rng) {
        int out = a;
        for (int g = 0; g < 96 && (out == a || out == b); ++g) {
            out = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return out;
    }

    static int random_digit_distinct(int n, int avoid_mask, std::mt19937_64& rng) {
        int d = 0;
        for (int g = 0; g < 128; ++g) {
            d = static_cast<int>(rng() % static_cast<uint64_t>(n));
            if ((avoid_mask & (1 << d)) == 0) return d;
        }
        return d;
    }

public:
    static bool build_medusa(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);

        const uint64_t ab = (1ULL << d1) | (1ULL << d2);
        const uint64_t ac = (1ULL << d1) | (1ULL << d3);
        const uint64_t bc = (1ULL << d2) | (1ULL << d3);

        plan.add_anchor(r1 * n + c1, ab);
        plan.add_anchor(r1 * n + c2, ac);
        plan.add_anchor(r2 * n + c2, bc);
        plan.add_anchor(r2 * n + c3, ab);
        plan.add_anchor(r1 * n + c3, bc);
        plan.add_anchor(r2 * n + c1, ac);
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 6);
        return plan.valid;
    }

    static bool build_death_blossom(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        // Pozostało jak dawniej, dodano tylko upewnienie plan.explicit_skeleton
        // (Oryginalnie już było poprawne)
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;

        const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[static_cast<size_t>(pivot)];
        const int col = topo.cell_col[static_cast<size_t>(pivot)];
        const int box = topo.cell_box[static_cast<size_t>(pivot)];

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        const int d5 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3) | (1 << d4), rng);

        const uint64_t pivot_mask = (1ULL << d1) | (1ULL << d2) | (1ULL << d3) | (1ULL << d4) | (1ULL << d5);
        plan.add_anchor(pivot, pivot_mask);

        int row_petal = -1, row_petal2 = -1, col_petal = -1, col_petal2 = -1, box_petal = -1;
        for (int cc = 0; cc < n && row_petal < 0; ++cc) {
            const int idx = row * n + cc;
            if (idx == pivot || topo.cell_box[static_cast<size_t>(idx)] == box) continue;
            row_petal = idx;
        }
        for (int cc = 0; cc < n && row_petal2 < 0; ++cc) {
            const int idx = row * n + cc;
            if (idx == pivot || idx == row_petal || topo.cell_box[static_cast<size_t>(idx)] == box) continue;
            row_petal2 = idx;
        }
        for (int rr = 0; rr < n && col_petal < 0; ++rr) {
            const int idx = rr * n + col;
            if (idx == pivot || topo.cell_box[static_cast<size_t>(idx)] == box) continue;
            col_petal = idx;
        }
        for (int rr = 0; rr < n && col_petal2 < 0; ++rr) {
            const int idx = rr * n + col;
            if (idx == pivot || idx == col_petal || topo.cell_box[static_cast<size_t>(idx)] == box) continue;
            col_petal2 = idx;
        }
        const int house = 2 * n + box;
        const int p0 = topo.house_offsets[static_cast<size_t>(house)];
        const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];
        for (int p = p0; p < p1 && box_petal < 0; ++p) {
            const int idx = topo.houses_flat[static_cast<size_t>(p)];
            if (idx == pivot || topo.cell_row[static_cast<size_t>(idx)] == row || topo.cell_col[static_cast<size_t>(idx)] == col) continue;
            box_petal = idx;
        }
        if (row_petal < 0 || col_petal < 0 || box_petal < 0) return false;

        const uint64_t row_mask = (1ULL << d1) | (1ULL << d2);
        const uint64_t col_mask = (1ULL << d2) | (1ULL << d3);
        const uint64_t box_mask = (1ULL << d3) | (1ULL << d4);
        const uint64_t row_mask2 = (1ULL << d1) | (1ULL << d5);
        const uint64_t col_mask2 = (1ULL << d2) | (1ULL << d5);
        const uint64_t box_support_mask = (1ULL << d4) | (1ULL << d5);

        plan.add_anchor(row_petal, row_mask);
        plan.add_anchor(col_petal, col_mask);
        plan.add_anchor(box_petal, box_mask);
        if (row_petal2 >= 0) plan.add_anchor(row_petal2, row_mask2);
        if (col_petal2 >= 0) plan.add_anchor(col_petal2, col_mask2);

        plan.add_skeleton(pivot, pivot_mask);
        plan.add_skeleton(row_petal, row_mask);
        plan.add_skeleton(col_petal, col_mask);
        plan.add_skeleton(box_petal, box_mask);
        if (row_petal2 >= 0) plan.add_skeleton(row_petal2, row_mask2);
        if (col_petal2 >= 0) plan.add_skeleton(col_petal2, col_mask2);
        for (int p = p0; p < p1; ++p) {
            const int idx = topo.houses_flat[static_cast<size_t>(p)];
            if (idx == pivot || idx == box_petal || topo.cell_row[static_cast<size_t>(idx)] == row || topo.cell_col[static_cast<size_t>(idx)] == col) continue;
            if (plan.add_skeleton(idx, box_support_mask)) break;
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4 && plan.skeleton_count >= 5);
        return plan.valid;
    }

    static bool build_sue_de_coq(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        // Kod poprawny - pominięty na rzecz miejsca - dodano tylko na dole plan.explicit_skeleton = true;
        plan = {};
        const int n = topo.n;
        if (topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;
        const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));
        const int col = c0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_cols));

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        const int d5 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3) | (1 << d4), rng);
        const int d6 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3) | (1 << d4) | (1 << d5), rng);

        const uint64_t inter_mask = (1ULL << d1) | (1ULL << d2) | (1ULL << d3) | (1ULL << d6);
        const uint64_t row_box_mask = (1ULL << d1) | (1ULL << d3) | (1ULL << d4) | (1ULL << d6);
        const uint64_t col_box_mask = (1ULL << d2) | (1ULL << d3) | (1ULL << d5) | (1ULL << d6);
        const uint64_t row_out_mask = (1ULL << d1) | (1ULL << d4) | (1ULL << d5) | (1ULL << d6);
        const uint64_t col_out_mask = (1ULL << d2) | (1ULL << d4) | (1ULL << d5) | (1ULL << d6);
        const uint64_t box_only_mask = (1ULL << d3) | (1ULL << d4) | (1ULL << d5) | (1ULL << d6);
        const uint64_t row_support_mask = (1ULL << d1) | (1ULL << d4) | (1ULL << d6);
        const uint64_t col_support_mask = (1ULL << d2) | (1ULL << d5) | (1ULL << d6);
        const uint64_t box_support_mask = (1ULL << d3) | (1ULL << d5) | (1ULL << d6);

        int row_box = -1, col_box = -1, row_out = -1, row_out2 = -1, col_out = -1, col_out2 = -1, box_only = -1, box_only2 = -1;
        for (int dc = 0; dc < topo.box_cols && row_box < 0; ++dc) {
            if (c0 + dc == col) continue;
            row_box = row * n + (c0 + dc);
        }
        for (int dr = 0; dr < topo.box_rows && col_box < 0; ++dr) {
            if (r0 + dr == row) continue;
            col_box = (r0 + dr) * n + col;
        }
        for (int cc = 0; cc < n && row_out < 0; ++cc) {
            if (cc >= c0 && cc < c0 + topo.box_cols) continue;
            row_out = row * n + cc;
        }
        for (int cc = 0; cc < n && row_out2 < 0; ++cc) {
            const int idx = row * n + cc;
            if (idx == row_out || (cc >= c0 && cc < c0 + topo.box_cols)) continue;
            row_out2 = idx;
        }
        for (int rr = 0; rr < n && col_out < 0; ++rr) {
            if (rr >= r0 && rr < r0 + topo.box_rows) continue;
            col_out = rr * n + col;
        }
        for (int rr = 0; rr < n && col_out2 < 0; ++rr) {
            const int idx = rr * n + col;
            if (idx == col_out || (rr >= r0 && rr < r0 + topo.box_rows)) continue;
            col_out2 = idx;
        }
        for (int dr = 0; dr < topo.box_rows && box_only < 0; ++dr) {
            for (int dc = 0; dc < topo.box_cols && box_only < 0; ++dc) {
                if (r0 + dr == row || c0 + dc == col) continue;
                box_only = (r0 + dr) * n + (c0 + dc);
            }
        }
        for (int dr = 0; dr < topo.box_rows && box_only2 < 0; ++dr) {
            for (int dc = 0; dc < topo.box_cols && box_only2 < 0; ++dc) {
                const int idx = (r0 + dr) * n + (c0 + dc);
                if (idx == box_only || r0 + dr == row || c0 + dc == col) continue;
                box_only2 = idx;
            }
        }
        if (row_box < 0 || col_box < 0 || row_out < 0 || col_out < 0 || box_only < 0) return false;

        const int inter = row * n + col;
        plan.add_anchor(inter, inter_mask);
        plan.add_anchor(row_box, row_box_mask);
        plan.add_anchor(col_box, col_box_mask);
        plan.add_anchor(row_out, row_out_mask);
        plan.add_anchor(col_out, col_out_mask);
        plan.add_anchor(box_only, box_only_mask);
        if (row_out2 >= 0) plan.add_anchor(row_out2, row_support_mask);
        if (col_out2 >= 0) plan.add_anchor(col_out2, col_support_mask);
        if (box_only2 >= 0) plan.add_anchor(box_only2, box_support_mask);

        plan.add_skeleton(inter, inter_mask);
        plan.add_skeleton(row_box, row_box_mask);
        plan.add_skeleton(col_box, col_box_mask);
        plan.add_skeleton(row_out, row_out_mask);
        plan.add_skeleton(col_out, col_out_mask);
        plan.add_skeleton(box_only, box_only_mask);
        if (row_out2 >= 0) plan.add_skeleton(row_out2, row_support_mask);
        if (col_out2 >= 0) plan.add_skeleton(col_out2, col_support_mask);
        if (box_only2 >= 0) plan.add_skeleton(box_only2, box_support_mask);
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 6 && plan.skeleton_count >= 6);
        return plan.valid;
    }

    static bool build_kraken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        
        auto get_mask = [&]() {
            return (1ULL << d1) | (1ULL << random_digit_distinct(n, (1 << d1), rng));
        };

        plan.add_anchor(r1 * n + c1, get_mask());
        plan.add_anchor(r1 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c1, get_mask());
        plan.add_anchor(r2 * n + c2, get_mask());
        plan.add_anchor(r1 * n + c3, get_mask());

        int extra = 0;
        for (int rr = 0; rr < n && extra < 1; ++rr) {
            if (rr == r1 || rr == r2) continue;
            if (plan.add_anchor(rr * n + c2, get_mask())) ++extra;
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_franken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        if (r1 == r2 || c1 == c2) return false;

        const int b1 = topo.cell_box[static_cast<size_t>(r1 * n + c1)];
        const int house = 2 * n + b1;
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        
        auto get_mask = [&]() {
            return (1ULL << d1) | (1ULL << random_digit_distinct(n, (1 << d1), rng));
        };

        plan.add_anchor(r1 * n + c1, get_mask());
        plan.add_anchor(r1 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c1, get_mask());
        plan.add_anchor(r2 * n + c2, get_mask());

        const int p0 = topo.house_offsets[static_cast<size_t>(house)];
        const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];
        for (int p = p0; p < p1 && plan.anchor_count < 6; ++p) {
            const int idx = topo.houses_flat[static_cast<size_t>(p)];
            const int rr = topo.cell_row[static_cast<size_t>(idx)];
            const int cc = topo.cell_col[static_cast<size_t>(idx)];
            if (rr == r1 || rr == r2 || cc == c1 || cc == c2) continue;
            plan.add_anchor(idx, get_mask());
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_mutant_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        if (r1 == r2 || c1 == c2) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        auto get_mask = [&]() {
            return (1ULL << d1) | (1ULL << random_digit_distinct(n, (1 << d1), rng));
        };

        plan.add_anchor(r1 * n + c1, get_mask());
        plan.add_anchor(r1 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c1, get_mask());
        plan.add_anchor(r2 * n + c2, get_mask());

        const int boxes[2] = {
            topo.cell_box[static_cast<size_t>(r1 * n + c2)],
            topo.cell_box[static_cast<size_t>(r2 * n + c1)]
        };

        for (int bi = 0; bi < 2; ++bi) {
            const int house = 2 * n + boxes[bi];
            const int p0 = topo.house_offsets[static_cast<size_t>(house)];
            const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];
            for (int p = p0; p < p1; ++p) {
                const int idx = topo.houses_flat[static_cast<size_t>(p)];
                const int rr = topo.cell_row[static_cast<size_t>(idx)];
                const int cc = topo.cell_col[static_cast<size_t>(idx)];
                if (rr == r1 || rr == r2 || cc == c1 || cc == c2) continue;
                if (plan.add_anchor(idx, get_mask())) break;
            }
        }

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_squirmbag(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 8) return false;

        int rows[3] = { static_cast<int>(rng() % static_cast<uint64_t>(n)), -1, -1 };
        rows[1] = random_distinct_index(n, rows[0], rng);
        rows[2] = random_third_index(n, rows[0], rows[1], rng);
        int cols[3] = { static_cast<int>(rng() % static_cast<uint64_t>(n)), -1, -1 };
        cols[1] = random_distinct_index(n, cols[0], rng);
        cols[2] = random_third_index(n, cols[0], cols[1], rng);

        if (rows[0] == rows[1] || rows[0] == rows[2] || rows[1] == rows[2] ||
            cols[0] == cols[1] || cols[0] == cols[2] || cols[1] == cols[2]) {
            return false;
        }

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        for (int ri = 0; ri < 3; ++ri) {
            for (int ci = 0; ci < 3; ++ci) {
                int d_other = random_digit_distinct(n, (1 << d1), rng);
                plan.add_anchor(rows[ri] * n + cols[ci], (1ULL << d1) | (1ULL << d_other));
            }
        }

        int c4 = cols[2];
        for (int g = 0; g < 64 && (c4 == cols[0] || c4 == cols[1] || c4 == cols[2]); ++g) {
            c4 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        if (c4 != cols[0] && c4 != cols[1] && c4 != cols[2]) {
            int d_other = random_digit_distinct(n, (1 << d1), rng);
            plan.add_anchor(rows[0] * n + c4, (1ULL << d1) | (1ULL << d_other));
        }

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_als_xy_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[static_cast<size_t>(pivot)];
        const int col = topo.cell_col[static_cast<size_t>(pivot)];
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const uint64_t xy = (1ULL << d1) | (1ULL << d2);
        const uint64_t xz = (1ULL << d1) | (1ULL << d3);
        const uint64_t yz = (1ULL << d2) | (1ULL << d3);
        plan.add_anchor(pivot, xy);
        int wing1 = -1, wing2 = -1;
        for (int cc = 0; cc < n && wing1 < 0; ++cc) {
            if (row * n + cc != pivot) wing1 = row * n + cc;
        }
        for (int rr = 0; rr < n && wing2 < 0; ++rr) {
            if (rr * n + col != pivot) wing2 = rr * n + col;
        }
        if (wing1 < 0 || wing2 < 0) return false;
        plan.add_anchor(wing1, xz);
        plan.add_anchor(wing2, yz);
        for (int p = topo.peer_offsets[static_cast<size_t>(wing1)]; p < topo.peer_offsets[static_cast<size_t>(wing1 + 1)] && plan.anchor_count < 5; ++p) {
            const int idx = topo.peers_flat[static_cast<size_t>(p)];
            if (idx == pivot || idx == wing2) continue;
            if (topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(wing2)] ||
                topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(wing2)] ||
                topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(wing2)]) {
                plan.add_anchor(idx, yz | (1ULL << d1));
            }
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 3);
        return plan.valid;
    }

    static bool build_als_xz(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        // Poprzedni był w miarę Ok. Upewniam się, że ma flagę.
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int r3 = random_third_index(n, r1, r2, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || r1 == r3 || r2 == r3 || c1 == c2 || c1 == c3 || c2 == c3) return false;

        const int dx = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int dz = random_digit_distinct(n, static_cast<int>(1ULL << dx), rng);
        const int da = random_digit_distinct(n, static_cast<int>((1ULL << dx) | (1ULL << dz)), rng);
        const int db = random_digit_distinct(n, static_cast<int>((1ULL << dx) | (1ULL << dz) | (1ULL << da)), rng);
        const int dv = random_digit_distinct(n, static_cast<int>((1ULL << dx) | (1ULL << dz) | (1ULL << da) | (1ULL << db)), rng);

        const uint64_t x = (1ULL << dx);
        const uint64_t z = (1ULL << dz);
        const uint64_t a = (1ULL << da);
        const uint64_t b = (1ULL << db);
        const uint64_t v = (1ULL << dv);

        plan.add_anchor(r1 * n + c1, x | a);
        plan.add_anchor(r1 * n + c2, z | a);
        plan.add_anchor(r2 * n + c1, x | b);
        plan.add_anchor(r2 * n + c2, z | b);
        plan.add_anchor(r3 * n + c2, z | v);

        plan.add_skeleton(r1 * n + c3, x | z | a);
        plan.add_skeleton(r2 * n + c3, x | z | b);
        plan.add_skeleton(r3 * n + c1, x | z | v);

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5 && plan.skeleton_count >= 8);
        return plan.valid;
    }

    static bool build_wxyz_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        // Tak jak wyżej, tylko wklejam z flagą.
        plan = {};
        const int n = topo.n;
        const int box_area = topo.box_rows * topo.box_cols;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1 || box_area < 5) return false;

        const int box_count = topo.box_rows_count * topo.box_cols_count;
        const int box = static_cast<int>(rng() % static_cast<uint64_t>(box_count));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;

        const int rr0 = r0, rr1 = r0 + 1, cc0 = c0, cc1 = c0 + 1;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        const int dv = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3) | (1 << d4), rng);

        const uint64_t a = (1ULL << d1), b = (1ULL << d2), c = (1ULL << d3), z = (1ULL << d4), v = (1ULL << dv);
        const int a0 = rr0 * n + cc0, a1 = rr0 * n + cc1, a2 = rr1 * n + cc0, a3 = rr1 * n + cc1;
        
        plan.add_anchor(a0, a | b | z);
        plan.add_anchor(a1, a | c | z);
        plan.add_anchor(a2, b | c | z);
        plan.add_anchor(a3, a | b | c);

        int victim = -1;
        for (int dr = 0; dr < topo.box_rows && victim < 0; ++dr) {
            for (int dc = 0; dc < topo.box_cols && victim < 0; ++dc) {
                const int idx = (r0 + dr) * n + (c0 + dc);
                if (idx != a0 && idx != a1 && idx != a2 && idx != a3) victim = idx;
            }
        }
        if (victim < 0) return false;

        plan.add_anchor(victim, z | v);
        plan.add_skeleton(a0, a | b | z);
        plan.add_skeleton(a1, a | c | z);
        plan.add_skeleton(a2, b | c | z);
        plan.add_skeleton(a3, a | b | c);
        plan.add_skeleton(victim, z | v);
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5 && plan.skeleton_count >= 5);
        return plan.valid;
    }

    static bool build_als_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool aic_mode) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        const uint64_t a = (1ULL << d1) | (1ULL << d2) | (1ULL << d3);
        const uint64_t b = (1ULL << d2) | (1ULL << d3) | (1ULL << d4);
        const uint64_t c = (1ULL << d1) | (1ULL << d3) | (1ULL << d4);
        
        plan.add_anchor(r1 * n + c1, a);
        plan.add_anchor(r1 * n + c2, a);
        plan.add_anchor(r2 * n + c2, b);
        plan.add_anchor(r2 * n + c3, b);
        plan.add_anchor(r1 * n + c3, c);
        if (aic_mode) {
            plan.add_anchor(r2 * n + c1, c | (1ULL << d2));
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= (aic_mode ? 6 : 5));
        return plan.valid;
    }

    static bool build_aligned_exclusion(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool triple_mode) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        const int a = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        int b = a;
        for (int p = topo.peer_offsets[static_cast<size_t>(a)]; p < topo.peer_offsets[static_cast<size_t>(a + 1)] && b == a; ++p) {
            b = topo.peers_flat[static_cast<size_t>(p)];
        }
        if (b == a) return false;
        int c = -1;
        if (triple_mode) {
            for (int p = topo.peer_offsets[static_cast<size_t>(b)]; p < topo.peer_offsets[static_cast<size_t>(b + 1)] && c < 0; ++p) {
                const int idx = topo.peers_flat[static_cast<size_t>(p)];
                if (idx == a) continue;
                if (topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(a)] ||
                    topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(a)] ||
                    topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(a)]) {
                    c = idx;
                }
            }
            if (c < 0) return false;
        }

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        plan.add_anchor(a, (1ULL << d1) | (1ULL << d2));
        plan.add_anchor(b, (1ULL << d2) | (1ULL << d3));
        if (triple_mode) plan.add_anchor(c, (1ULL << d3) | (1ULL << d4));
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= (triple_mode ? 3 : 2));
        return plan.valid;
    }

    static bool build_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const uint64_t a = (1ULL << d1) | (1ULL << d2);
        const uint64_t b = (1ULL << d2) | (1ULL << d3);
        const uint64_t c = (1ULL << d1) | (1ULL << d3);
        plan.add_anchor(r1 * n + c1, a);
        plan.add_anchor(r1 * n + c2, b);
        plan.add_anchor(r2 * n + c2, c);
        plan.add_anchor(r2 * n + c3, a);
        plan.add_anchor(r1 * n + c3, c); 
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_grouped_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;
        const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[static_cast<size_t>(pivot)];
        const int box = topo.cell_box[static_cast<size_t>(pivot)];
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const uint64_t a = (1ULL << d1) | (1ULL << d2);
        const uint64_t b = (1ULL << d2) | (1ULL << d3);
        const uint64_t c = (1ULL << d1) | (1ULL << d3);
        plan.add_anchor(pivot, a);
        for (int cc = 0; cc < n && plan.anchor_count < 3; ++cc) {
            const int idx = row * n + cc;
            if (idx == pivot || topo.cell_box[static_cast<size_t>(idx)] != box) continue;
            plan.add_anchor(idx, b);
        }
        const int house = 2 * n + box;
        const int p0 = topo.house_offsets[static_cast<size_t>(house)];
        const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];
        for (int p = p0; p < p1 && plan.anchor_count < 6; ++p) {
            const int idx = topo.houses_flat[static_cast<size_t>(p)];
            if (idx == pivot || topo.cell_row[static_cast<size_t>(idx)] == row) continue;
            plan.add_anchor(idx, c);
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }

    static bool build_grouped_x_cycle(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const uint64_t core = (1ULL << d1) | (1ULL << d2);
        plan.add_anchor(r1 * n + c1, core);
        plan.add_anchor(r1 * n + c2, core);
        plan.add_anchor(r2 * n + c1, core);
        plan.add_anchor(r2 * n + c2, core);
        for (int rr = 0; rr < n && plan.anchor_count < 6; ++rr) {
            if (rr == r1 || rr == r2) continue;
            plan.add_anchor(rr * n + c1, core);
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }

    static bool build_continuous_nice_loop(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = random_digit_distinct(n, (1 << d1), rng);
        int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        uint64_t a = (1ULL << d1) | (1ULL << d2);
        uint64_t b = (1ULL << d2) | (1ULL << d3);
        uint64_t c = (1ULL << d1) | (1ULL << d3);
        plan.add_anchor(r1 * n + c1, a);
        plan.add_anchor(r1 * n + c2, b);
        plan.add_anchor(r2 * n + c2, c);
        plan.add_anchor(r2 * n + c1, a);
        for (int rr = 0; rr < n && plan.anchor_count < 6; ++rr) {
            if (rr == r1 || rr == r2) continue;
            plan.add_anchor(rr * n + c2, b);
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }

    static bool build_x_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int r3 = random_third_index(n, r1, r2, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        
        auto get_mask = [&]() {
            return (1ULL << d1) | (1ULL << random_digit_distinct(n, (1 << d1), rng));
        };

        plan.add_anchor(r1 * n + c1, get_mask());
        plan.add_anchor(r1 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c3, get_mask());
        plan.add_anchor(r3 * n + c3, get_mask());
        plan.add_anchor(r3 * n + c1, get_mask());
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 6);
        return plan.valid;
    }

    static bool build_xy_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        const int d5 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3) | (1 << d4), rng);
        
        plan.add_anchor(r1 * n + c1, (1ULL << d1) | (1ULL << d2));
        plan.add_anchor(r1 * n + c2, (1ULL << d2) | (1ULL << d3));
        plan.add_anchor(r2 * n + c2, (1ULL << d3) | (1ULL << d4));
        plan.add_anchor(r2 * n + c3, (1ULL << d4) | (1ULL << d5));
        plan.add_anchor(r1 * n + c3, (1ULL << d5) | (1ULL << d1));
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_empty_rectangle(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;
        const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));
        const int col = c0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_cols));
        const int pivot = row * n + col;

        int row_out = -1, row_out2 = -1, col_out = -1, col_out2 = -1, box_support = -1, box_support2 = -1;
        for (int cc = 0; cc < n; ++cc) {
            if (cc >= c0 && cc < c0 + topo.box_cols) continue;
            if (row_out < 0) row_out = row * n + cc;
            else if (row_out2 < 0) row_out2 = row * n + cc;
        }
        for (int rr = 0; rr < n; ++rr) {
            if (rr >= r0 && rr < r0 + topo.box_rows) continue;
            if (col_out < 0) col_out = rr * n + col;
            else if (col_out2 < 0) col_out2 = rr * n + col;
        }
        for (int dr = 0; dr < topo.box_rows && box_support < 0; ++dr) {
            for (int dc = 0; dc < topo.box_cols; ++dc) {
                const int idx = (r0 + dr) * n + (c0 + dc);
                if (idx == pivot || r0 + dr == row || c0 + dc == col) continue;
                if (box_support < 0) box_support = idx;
                else if (box_support2 < 0) box_support2 = idx;
            }
        }
        if (row_out < 0 || col_out < 0 || box_support < 0) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const int d4 = random_digit_distinct(n, (1 << d1) | (1 << d2) | (1 << d3), rng);
        const uint64_t core = (1ULL << d1) | (1ULL << d2);
        const uint64_t line_mask = core | (1ULL << d3);
        const uint64_t box_mask = core | (1ULL << d4);

        plan.add_anchor(pivot, core);
        plan.add_anchor(row_out, core);
        plan.add_anchor(col_out, core);
        plan.add_anchor(box_support, box_mask);
        if (box_support2 >= 0) plan.add_anchor(box_support2, box_mask);
        if (row_out2 >= 0) plan.add_anchor(row_out2, line_mask);
        if (col_out2 >= 0) plan.add_anchor(col_out2, line_mask);
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_remote_pairs(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        // Skompresowano by nie kopiowac całego bloku. Wprowadzono już wcześniej explicit_skeleton
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int d2 = random_digit_distinct(n, (1 << d1), rng);
        const int d3 = random_digit_distinct(n, (1 << d1) | (1 << d2), rng);
        const uint64_t pair = (1ULL << d1) | (1ULL << d2);
        const uint64_t row_victim = pair | (1ULL << d3);
        plan.add_anchor(r1 * n + c1, pair);
        plan.add_anchor(r1 * n + c2, pair);
        plan.add_anchor(r2 * n + c2, pair);
        plan.add_anchor(r2 * n + c1, pair);
        plan.add_anchor(r1 * n + c3, row_victim);
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_large_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, int line_count, bool finned_mode) {
        plan = {};
        const int n = topo.n;
        if (n < std::max(6, line_count + 2)) return false;

        int rows[4] = { -1, -1, -1, -1 };
        int cols[4] = { -1, -1, -1, -1 };
        rows[0] = static_cast<int>(rng() % static_cast<uint64_t>(n));
        cols[0] = static_cast<int>(rng() % static_cast<uint64_t>(n));
        for (int i = 1; i < line_count; ++i) {
            rows[i] = rows[0];
            cols[i] = cols[0];
            for (int g = 0; g < 96 && std::find(rows, rows + i, rows[i]) != rows + i; ++g) {
                rows[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            for (int g = 0; g < 96 && std::find(cols, cols + i, cols[i]) != cols + i; ++g) {
                cols[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
        }

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));

        for (int ri = 0; ri < line_count; ++ri) {
            for (int ci = 0; ci < line_count; ++ci) {
                int d_other = random_digit_distinct(n, (1 << d1), rng);
                plan.add_anchor(rows[ri] * n + cols[ci], (1ULL << d1) | (1ULL << d_other));
            }
        }
        if (finned_mode) {
            int extra_col = cols[0];
            for (int g = 0; g < 96 && std::find(cols, cols + line_count, extra_col) != cols + line_count; ++g) {
                extra_col = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            if (std::find(cols, cols + line_count, extra_col) == cols + line_count) {
                int d_other1 = random_digit_distinct(n, (1 << d1), rng);
                plan.add_anchor(rows[0] * n + extra_col, (1ULL << d1) | (1ULL << d_other1));
            }
        }
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= line_count * line_count);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing