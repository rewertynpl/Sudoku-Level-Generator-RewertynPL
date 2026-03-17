//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "../../config/run_config.h"
#include "template_exocet.h"
#include "template_forcing.h"
#include "template_sk_loop.h"
#include "template_msls_overlay.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateP6Exact {
    static bool is_peer(const GenericTopology& topo, int a, int b) {
        return topo.cell_row[static_cast<size_t>(a)] == topo.cell_row[static_cast<size_t>(b)] ||
               topo.cell_col[static_cast<size_t>(a)] == topo.cell_col[static_cast<size_t>(b)] ||
               topo.cell_box[static_cast<size_t>(a)] == topo.cell_box[static_cast<size_t>(b)];
    }

    static int pick_distinct_index(int n, int avoid, std::mt19937_64& rng) {
        if (n <= 1) return avoid;
        int x = avoid;
        for (int g = 0; g < 128 && x == avoid; ++g) {
            x = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return x;
    }

    static int pick_third_index(int n, int a, int b, std::mt19937_64& rng) {
        if (n <= 2) return a;
        int x = a;
        for (int g = 0; g < 128 && (x == a || x == b); ++g) {
            x = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return x;
    }

    static uint64_t pick_two_digit_mask(int n, std::mt19937_64& rng) {
        if (n <= 1) return 1ULL;
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 128 && d2 == d1; ++g) {
            d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return (1ULL << d1) | (1ULL << d2);
    }

    static uint64_t pick_three_digit_mask(int n, std::mt19937_64& rng) {
        uint64_t m = pick_two_digit_mask(n, rng);
        for (int g = 0; g < 128 && std::popcount(m) < 3; ++g) {
            m |= (1ULL << (static_cast<int>(rng() % static_cast<uint64_t>(n))));
        }
        return m;
    }

    static bool build_chain_ring(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, int want, bool tri) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 4 || want < 4) return false;
        const uint64_t full = pf_full_mask_for_n(n);

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = pick_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = pick_distinct_index(n, c1, rng);
        int c3 = pick_third_index(n, c1, c2, rng);

        if (r1 == r2 || c1 == c2) return false;

        const int a = r1 * n + c1;
        const int b = r1 * n + c2;
        const int c = r2 * n + c2;
        const int d = r2 * n + (tri ? c3 : c1);

        uint64_t m1 = pick_two_digit_mask(n, rng) & full;
        uint64_t m2 = tri ? (pick_three_digit_mask(n, rng) & full) : m1;
        uint64_t m3 = pick_two_digit_mask(n, rng) & full;
        uint64_t m4 = tri ? (pick_three_digit_mask(n, rng) & full) : m3;

        if (m1 == 0ULL || m2 == 0ULL || m3 == 0ULL || m4 == 0ULL) return false;

        plan.add_anchor(a, m1);
        plan.add_anchor(b, m2);
        plan.add_anchor(c, m3);
        plan.add_anchor(d, m4);

        plan.add_skeleton(a, m1);
        plan.add_skeleton(b, m2);
        plan.add_skeleton(c, m3);
        plan.add_skeleton(d, m4);

        plan.valid = plan.anchor_count >= 4;
        return plan.valid;
    }

public:
    static bool build_medusa(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, false);
    }

    static bool build_death_blossom(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 4) return false;

        const int center = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[static_cast<size_t>(center)];
        const int col = topo.cell_col[static_cast<size_t>(center)];
        const uint64_t stem = pick_three_digit_mask(n, rng) & pf_full_mask_for_n(n);
        plan.add_anchor(center, stem);

        int petals = 0;
        for (int idx = 0; idx < topo.nn && petals < 3; ++idx) {
            if (idx == center) continue;
            const bool row_peer = topo.cell_row[static_cast<size_t>(idx)] == row;
            const bool col_peer = topo.cell_col[static_cast<size_t>(idx)] == col;
            if (!(row_peer || col_peer)) continue;
            const uint64_t mask = pick_two_digit_mask(n, rng) & pf_full_mask_for_n(n);
            if (mask == 0ULL) continue;
            plan.add_anchor(idx, mask);
            plan.add_skeleton(idx, mask);
            ++petals;
        }

        plan.add_skeleton(center, stem);
        plan.valid = plan.anchor_count >= 4;
        return plan.valid;
    }

    static bool build_sue_de_coq(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 4) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int a = -1, b = -1, row_only = -1, col_only = -1;
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (topo.cell_box[static_cast<size_t>(idx)] != box) continue;
            if (a < 0) { a = idx; continue; }
            if (topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(a)]) { b = idx; break; }
        }
        if (a < 0 || b < 0) return false;

        const int row = topo.cell_row[static_cast<size_t>(a)];
        const int col = topo.cell_col[static_cast<size_t>(b)];

        for (int idx = 0; idx < topo.nn; ++idx) {
            if (topo.cell_row[static_cast<size_t>(idx)] == row && topo.cell_box[static_cast<size_t>(idx)] != box) { row_only = idx; break; }
        }
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (topo.cell_col[static_cast<size_t>(idx)] == col && topo.cell_box[static_cast<size_t>(idx)] != box) { col_only = idx; break; }
        }
        if (row_only < 0 || col_only < 0) return false;

        const uint64_t m1 = pick_three_digit_mask(n, rng) & pf_full_mask_for_n(n);
        const uint64_t m2 = pick_two_digit_mask(n, rng) & pf_full_mask_for_n(n);
        const uint64_t m3 = pick_two_digit_mask(n, rng) & pf_full_mask_for_n(n);
        plan.add_anchor(a, m1);
        plan.add_anchor(b, m1);
        plan.add_anchor(row_only, m2);
        plan.add_anchor(col_only, m3);
        plan.valid = plan.anchor_count >= 4;
        return plan.valid;
    }

    static bool build_kraken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 3, true);
    }

    static bool build_franken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 3, false);
    }

    static bool build_mutant_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 4, false);
    }

    static bool build_squirmbag(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 5, false);
    }

    static bool build_als_xy_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, true);
    }

    static bool build_als_xz(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, true);
    }

    static bool build_wxyz_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, true);
    }

    static bool build_als_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool aic_mode) {
        return build_chain_ring(topo, rng, plan, aic_mode ? 6 : 5, true);
    }

    static bool build_aligned_exclusion(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool triple_mode) {
        return build_chain_ring(topo, rng, plan, triple_mode ? 5 : 4, triple_mode);
    }

    static bool build_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 5, false);
    }

    static bool build_grouped_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 5, true);
    }

    static bool build_grouped_x_cycle(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, false);
    }

    static bool build_continuous_nice_loop(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, false);
    }

    static bool build_x_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, false);
    }

    static bool build_xy_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, false);
    }

    static bool build_empty_rectangle(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 4) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = pick_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = pick_distinct_index(n, c1, rng);
        if (r1 == r2 || c1 == c2) return false;

        const uint64_t mask = pick_two_digit_mask(n, rng) & pf_full_mask_for_n(n);
        plan.add_anchor(r1 * n + c1, mask);
        plan.add_anchor(r1 * n + c2, mask);
        plan.add_anchor(r2 * n + c1, mask);
        plan.add_skeleton(r2 * n + c2, mask);
        plan.valid = plan.anchor_count >= 3;
        return plan.valid;
    }

    static bool build_remote_pairs(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_chain_ring(topo, rng, plan, 4, false);
    }

    static bool build_large_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, int line_count, bool finned_mode) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < std::max(4, line_count)) return false;

        const uint64_t full = pf_full_mask_for_n(n);
        const uint64_t bitmask = pick_two_digit_mask(n, rng) & full;

        int used_rows[8];
        int used_cols[8];
        std::fill(std::begin(used_rows), std::end(used_rows), -1);
        std::fill(std::begin(used_cols), std::end(used_cols), -1);

        for (int i = 0; i < line_count; ++i) {
            int r = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int c = static_cast<int>(rng() % static_cast<uint64_t>(n));
            for (int g = 0; g < 128; ++g) {
                if (std::find(std::begin(used_rows), std::begin(used_rows) + i, r) == std::begin(used_rows) + i) break;
                r = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            for (int g = 0; g < 128; ++g) {
                if (std::find(std::begin(used_cols), std::begin(used_cols) + i, c) == std::begin(used_cols) + i) break;
                c = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            used_rows[i] = r;
            used_cols[i] = c;
            const int idx = r * n + c;
            plan.add_anchor(idx, bitmask);
            plan.add_skeleton(idx, bitmask);
        }

        if (finned_mode && line_count > 0) {
            int fr = used_rows[0];
            int fc = pick_distinct_index(n, used_cols[0], rng);
            plan.add_anchor(fr * n + fc, bitmask);
        }

        plan.valid = plan.anchor_count >= line_count;
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing
