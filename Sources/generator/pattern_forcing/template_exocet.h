#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <random>

#include "../../core/board.h"

namespace sudoku_hpc::pattern_forcing {

inline uint64_t pf_full_mask_for_n(int n) {
    if (n <= 0) return 0ULL;
    if (n >= 64) return ~0ULL;
    return (1ULL << n) - 1ULL;
}

struct ExactPatternTemplatePlan {
    bool valid = false;
    bool explicit_skeleton = false;
    int anchor_count = 0;
    std::array<int, 64> anchor_idx{};
    std::array<uint64_t, 64> anchor_masks{};
    int skeleton_count = 0;
    std::array<int, 256> skeleton_idx{};
    std::array<uint64_t, 256> skeleton_masks{};

    inline int find_anchor(int idx) const {
        for (int i = 0; i < anchor_count; ++i) if (anchor_idx[static_cast<size_t>(i)] == idx) return i;
        return -1;
    }
    inline int find_skeleton(int idx) const {
        for (int i = 0; i < skeleton_count; ++i) if (skeleton_idx[static_cast<size_t>(i)] == idx) return i;
        return -1;
    }
    inline bool add_anchor(int idx, uint64_t mask) {
        if (idx < 0 || mask == 0ULL) return false;
        const int ex = find_anchor(idx);
        if (ex >= 0) {
            anchor_masks[static_cast<size_t>(ex)] &= mask;
            return anchor_masks[static_cast<size_t>(ex)] != 0ULL;
        }
        if (anchor_count >= static_cast<int>(anchor_idx.size())) return false;
        anchor_idx[static_cast<size_t>(anchor_count)] = idx;
        anchor_masks[static_cast<size_t>(anchor_count)] = mask;
        ++anchor_count;
        valid = anchor_count > 0;
        return true;
    }
    inline bool add_skeleton(int idx, uint64_t mask) {
        if (idx < 0 || mask == 0ULL) return false;
        const int ex = find_skeleton(idx);
        if (ex >= 0) {
            skeleton_masks[static_cast<size_t>(ex)] &= mask;
            return skeleton_masks[static_cast<size_t>(ex)] != 0ULL;
        }
        if (skeleton_count >= static_cast<int>(skeleton_idx.size())) return false;
        skeleton_idx[static_cast<size_t>(skeleton_count)] = idx;
        skeleton_masks[static_cast<size_t>(skeleton_count)] = mask;
        ++skeleton_count;
        return true;
    }
};

class TemplateExocet {
public:
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool senior_mode = false) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 6 || topo.nn <= 0) return false;
        const uint64_t full = pf_full_mask_for_n(n);

        for (int tries = 0; tries < 512; ++tries) {
            const int base = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
            const int r = topo.cell_row[static_cast<size_t>(base)];
            const int c = topo.cell_col[static_cast<size_t>(base)];
            int mate = -1;
            for (int p = topo.peer_offsets[static_cast<size_t>(base)];
                 p < topo.peer_offsets[static_cast<size_t>(base + 1)]; ++p) {
                const int idx = topo.peers_flat[static_cast<size_t>(p)];
                if (topo.cell_row[static_cast<size_t>(idx)] == r && topo.cell_col[static_cast<size_t>(idx)] != c) {
                    mate = idx;
                    break;
                }
            }
            if (mate < 0) continue;

            int t1 = -1, t2 = -1;
            for (int idx = 0; idx < topo.nn; ++idx) {
                if (idx == base || idx == mate) continue;
                const bool see_b = topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(base)] ||
                                   topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(base)] ||
                                   topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(base)];
                const bool see_m = topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(mate)] ||
                                   topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(mate)] ||
                                   topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(mate)];
                if (see_b && !see_m && t1 < 0) t1 = idx;
                else if (see_m && !see_b && t2 < 0) t2 = idx;
                if (t1 >= 0 && t2 >= 0) break;
            }
            if (t1 < 0 || t2 < 0) continue;

            const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
            int d2 = d1;
            while (d2 == d1) d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
            uint64_t base_mask = (1ULL << d1) | (1ULL << d2);
            uint64_t ex1 = base_mask | (1ULL << ((d2 + 1) % n));
            uint64_t ex2 = base_mask | (1ULL << ((d2 + 2) % n));
            if (senior_mode) {
                ex1 |= (1ULL << ((d2 + 3) % n));
                ex2 |= (1ULL << ((d2 + 4) % n));
            }
            plan.add_anchor(base, base_mask);
            plan.add_anchor(mate, base_mask);
            plan.add_anchor(t1, ex1 & full);
            plan.add_anchor(t2, ex2 & full);
            plan.add_skeleton(t1, base_mask);
            plan.add_skeleton(t2, base_mask);
            plan.valid = plan.anchor_count >= 4;
            return plan.valid;
        }
        return false;
    }
};

} // namespace sudoku_hpc::pattern_forcing
