#pragma once

#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateSKLoop {
public:
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool dense_mode = false) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 4) return false;
        const uint64_t full = pf_full_mask_for_n(n);

        for (int tries = 0; tries < 512; ++tries) {
            const int a = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
            const int r1 = topo.cell_row[static_cast<size_t>(a)];
            const int c1 = topo.cell_col[static_cast<size_t>(a)];
            int b = -1, c = -1, d = -1;
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (idx != a && topo.cell_row[static_cast<size_t>(idx)] == r1 && topo.cell_col[static_cast<size_t>(idx)] != c1) { b = idx; break; }
            }
            if (b < 0) continue;
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (idx != a && topo.cell_col[static_cast<size_t>(idx)] == c1 && topo.cell_row[static_cast<size_t>(idx)] != r1) { c = idx; break; }
            }
            if (c < 0) continue;
            const int rr = topo.cell_row[static_cast<size_t>(c)];
            const int cc = topo.cell_col[static_cast<size_t>(b)];
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (topo.cell_row[static_cast<size_t>(idx)] == rr && topo.cell_col[static_cast<size_t>(idx)] == cc) { d = idx; break; }
            }
            if (d < 0 || d == a || d == b || d == c) continue;

            int x = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int y = x; while (y == x) y = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int z = y; while (z == x || z == y) z = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int w = z; while (w == x || w == y || w == z) w = static_cast<int>(rng() % static_cast<uint64_t>(n));
            const uint64_t core = (1ULL << x) | (1ULL << y);
            plan.add_anchor(a, core);
            plan.add_anchor(d, core);
            plan.add_anchor(b, (core | (1ULL << z)) & full);
            plan.add_anchor(c, (core | (1ULL << w) | (dense_mode ? (1ULL << z) : 0ULL)) & full);
            plan.add_skeleton(a, core);
            plan.add_skeleton(b, (1ULL << y) | (1ULL << z));
            plan.add_skeleton(d, core);
            plan.add_skeleton(c, (1ULL << x) | (1ULL << w));
            plan.valid = plan.anchor_count >= 4;
            return plan.valid;
        }
        return false;
    }
};

} // namespace sudoku_hpc::pattern_forcing
