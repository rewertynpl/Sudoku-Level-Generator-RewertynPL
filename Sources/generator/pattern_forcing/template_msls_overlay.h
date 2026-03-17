#pragma once

#include <algorithm>
#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateMslsOverlay {
public:
    static bool push_unique_anchor(ExactPatternTemplatePlan& plan, int idx, uint64_t mask) {
        return plan.add_anchor(idx, mask);
    }

    static bool build_msls(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 0 || topo.box_cols <= 0) return false;
        const uint64_t full = pf_full_mask_for_n(n);

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;
        const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));
        const int col = c0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_cols));

        int center = -1;
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (topo.cell_row[static_cast<size_t>(idx)] == row && topo.cell_col[static_cast<size_t>(idx)] == col) { center = idx; break; }
        }
        if (center < 0) return false;

        int d[5]{};
        for (int i = 0; i < 5; ++i) {
            d[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            for (int g = 0; g < 64; ++g) {
                bool ok = true;
                for (int j = 0; j < i; ++j) if (d[j] == d[i]) ok = false;
                if (ok) break;
                d[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
        }
        const uint64_t cmask = (1ULL << d[0]) | (1ULL << d[1]) | (1ULL << d[2]);
        const uint64_t row_mask = (1ULL << d[1]) | (1ULL << d[3]);
        const uint64_t col_mask = (1ULL << d[2]) | (1ULL << d[4]);

        plan.add_anchor(center, cmask & full);
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (idx == center) continue;
            if (topo.cell_row[static_cast<size_t>(idx)] == row && topo.cell_box[static_cast<size_t>(idx)] == box) {
                plan.add_anchor(idx, row_mask & full);
                break;
            }
        }
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (idx == center) continue;
            if (topo.cell_col[static_cast<size_t>(idx)] == col && topo.cell_box[static_cast<size_t>(idx)] == box) {
                plan.add_anchor(idx, col_mask & full);
                break;
            }
        }
        plan.add_skeleton(center, cmask & full);
        plan.valid = plan.anchor_count >= 3;
        return plan.valid;
    }

    static bool build_overlay(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_msls(topo, rng, plan);
    }
};

} // namespace sudoku_hpc::pattern_forcing
