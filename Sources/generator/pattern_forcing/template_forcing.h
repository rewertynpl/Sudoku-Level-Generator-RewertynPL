#pragma once

#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateForcing {
public:
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool dynamic_mode = false) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 4) return false;
        const uint64_t full = pf_full_mask_for_n(n);

        for (int tries = 0; tries < 512; ++tries) {
            const int a = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
            int b = -1, c = -1, d = -1;
            const int ar = topo.cell_row[static_cast<size_t>(a)];
            const int ac = topo.cell_col[static_cast<size_t>(a)];
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (idx == a) continue;
                if (topo.cell_row[static_cast<size_t>(idx)] == ar && topo.cell_col[static_cast<size_t>(idx)] != ac) { b = idx; break; }
            }
            if (b < 0) continue;
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (idx == a || idx == b) continue;
                if (topo.cell_col[static_cast<size_t>(idx)] == ac && topo.cell_row[static_cast<size_t>(idx)] != ar) { c = idx; break; }
            }
            if (c < 0) continue;
            const int br = topo.cell_row[static_cast<size_t>(c)];
            const int bc = topo.cell_col[static_cast<size_t>(b)];
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (topo.cell_row[static_cast<size_t>(idx)] == br && topo.cell_col[static_cast<size_t>(idx)] == bc) { d = idx; break; }
            }
            if (d < 0 || d == a || d == b || d == c) continue;

            const int x = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int y = x; while (y == x) y = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int z = y; while (z == x || z == y) z = static_cast<int>(rng() % static_cast<uint64_t>(n));
            const uint64_t m1 = (1ULL << x) | (1ULL << y);
            const uint64_t m2 = (1ULL << y) | (1ULL << z);
            const uint64_t m3 = (1ULL << x) | (1ULL << z);
            plan.add_anchor(a, m1 & full);
            plan.add_anchor(b, m2 & full);
            plan.add_anchor(c, m3 & full);
            plan.add_anchor(d, dynamic_mode ? ((m1 | m2 | m3) & full) : (m1 & full));
            plan.add_skeleton(a, m1 & full);
            plan.add_skeleton(b, m2 & full);
            plan.add_skeleton(c, m3 & full);
            plan.add_skeleton(d, (m1 | m2) & full);
            plan.valid = plan.anchor_count >= 4;
            return plan.valid;
        }
        return false;
    }
};

} // namespace sudoku_hpc::pattern_forcing
