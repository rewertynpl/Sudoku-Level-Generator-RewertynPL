#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <random>
#include <vector>

#include "../../core/board.h"
#include "../../config/run_config.h"

#include "template_exocet.h"
#include "template_sk_loop.h"
#include "template_forcing.h"
#include "template_msls_overlay.h"
#include "template_p6_exact.h"

namespace sudoku_hpc::pattern_forcing {

enum class PatternKind : uint8_t {
    None = 0,
    Chain,
    ExocetLike,
    LoopLike,
    ForcingLike,
    ColorLike,
    PetalLike,
    IntersectionLike,
    FishLike,
    FrankenLike,
    MutantLike,
    SquirmLike,
    AlsLike,
    ExclusionLike,
    AicLike,
    GroupedAicLike,
    GroupedCycleLike,
    NiceLoopLike,
    XChainLike,
    XYChainLike,
    EmptyRectangleLike,
    RemotePairsLike,
    SwordfishLike,
    JellyfishLike,
    FinnedFishLike,
    OverlayLike,
    MedusaLike
};

enum class PatternMutationSource : uint8_t {
    Random = 0,
    Last,
    Best
};

enum class TargetPattern : uint8_t {
    None = 0,
    XChain,
    XYChain,
    Exocet,
    MSLS
};

struct PatternSeedView {
    PatternKind kind = PatternKind::None;
    PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;
    const std::vector<uint16_t>* seed_puzzle = nullptr;
    const std::vector<uint64_t>* allowed_masks = nullptr;
    const std::vector<uint8_t>* protected_cells = nullptr;
    const int* anchor_idx = nullptr;
    const uint64_t* anchor_masks = nullptr;
    int anchor_count = 0;
    bool exact_template = false;
    bool family_fallback_used = false;
    bool required_strategy_exact_contract_met = false;
    bool corridor_protected = false;
    bool anti_single_protected = false;
    int template_score = 0;
    int best_template_score = 0;
    int template_score_delta = 0;
    int mutation_strength = 0;
    int planner_zero_use_streak = 0;
    int planner_failure_streak = 0;
    int adaptive_target_strength = 0;
    int survival_score = 0;
    int protected_count = 0;
    PatternMutationSource mutation_source = PatternMutationSource::Random;
};

struct PatternScratch {
    int prepared_nn = 0;
    std::vector<uint16_t> seed_puzzle;
    std::vector<uint64_t> allowed_masks;
    std::vector<uint8_t> protected_cells;
    std::array<int, 64> anchors{};
    std::array<uint64_t, 64> anchor_masks{};
    int anchor_count = 0;
    bool exact_template = false;
    int template_score = 0;
    int survival_score = 0;
    bool corridor_protected = false;
    bool anti_single_protected = false;

    void ensure(const GenericTopology& topo) {
        if (prepared_nn == topo.nn) return;
        seed_puzzle.assign(static_cast<size_t>(topo.nn), 0U);
        allowed_masks.assign(static_cast<size_t>(topo.nn), 0ULL);
        protected_cells.assign(static_cast<size_t>(topo.nn), 0U);
        prepared_nn = topo.nn;
    }

    void reset(const GenericTopology& topo) {
        ensure(topo);
        const uint64_t full = pf_full_mask_for_n(topo.n);
        std::fill(seed_puzzle.begin(), seed_puzzle.end(), 0U);
        std::fill(allowed_masks.begin(), allowed_masks.end(), full);
        std::fill(protected_cells.begin(), protected_cells.end(), static_cast<uint8_t>(0));
        std::fill(anchors.begin(), anchors.end(), -1);
        std::fill(anchor_masks.begin(), anchor_masks.end(), 0ULL);
        anchor_count = 0;
        exact_template = false;
        template_score = 0;
        survival_score = 0;
        corridor_protected = false;
        anti_single_protected = false;
    }

    bool add_anchor(int idx) {
        if (idx < 0 || idx >= prepared_nn) return false;
        for (int i = 0; i < anchor_count; ++i) {
            if (anchors[static_cast<size_t>(i)] == idx) return false;
        }
        if (anchor_count >= static_cast<int>(anchors.size())) return false;
        anchors[static_cast<size_t>(anchor_count)] = idx;
        ++anchor_count;
        return true;
    }
};

inline PatternScratch& tls_pattern_scratch() {
    thread_local PatternScratch s;
    return s;
}

inline bool pattern_required_is_fragile_candidate_structure(RequiredStrategy required_strategy) {
    return strategy_prefers_named_structures_before_generic(required_strategy) ||
           strategy_requires_exact_only(required_strategy) ||
           strategy_suppress_equivalent_generic_families(required_strategy) ||
           strategy_prefers_generator_certifier_split(required_strategy);
}

inline void protect_pattern_cells_from_masks(const GenericTopology& topo, PatternScratch& sc) {
    for (int idx = 0; idx < topo.nn; ++idx) {
        if (sc.seed_puzzle[static_cast<size_t>(idx)] != 0U) {
            sc.protected_cells[static_cast<size_t>(idx)] = 1U;
        }
        const uint64_t mask = sc.allowed_masks[static_cast<size_t>(idx)] & pf_full_mask_for_n(topo.n);
        if (mask != 0ULL && std::popcount(mask) <= 2) {
            sc.protected_cells[static_cast<size_t>(idx)] = 1U;
        }
    }
}

inline uint64_t truncate_mask_bits(uint64_t mask, int keep_bits) {
    if (mask == 0ULL || keep_bits <= 0) return mask;
    if (std::popcount(mask) <= keep_bits) return mask;
    uint64_t out = 0ULL;
    for (int kept = 0; kept < keep_bits && mask != 0ULL; ++kept) {
        const int bit = std::countr_zero(mask);
        out |= (1ULL << bit);
        mask &= (mask - 1ULL);
    }
    return out;
}

inline uint64_t exact_family_core_mask(const ExactPatternTemplatePlan& plan, int begin_idx, int end_idx) {
    uint64_t core = 0ULL;
    for (int i = begin_idx; i < end_idx; ++i) {
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        core = (core == 0ULL) ? mask : (core & mask);
    }
    return core;
}

inline void expand_exact_template_skeleton(const GenericTopology& topo, PatternKind kind, ExactPatternTemplatePlan& plan) {
    if (!plan.valid || plan.anchor_count <= 0 || plan.explicit_skeleton) return;

    int core_count = std::min(plan.anchor_count, 4);
    int extra_cap = 10;
    int keep_bits = 3;
    switch (kind) {
        case PatternKind::IntersectionLike: keep_bits = 4; extra_cap = 12; break;
        case PatternKind::FishLike:
        case PatternKind::FrankenLike:
        case PatternKind::MutantLike:
        case PatternKind::SquirmLike:
        case PatternKind::SwordfishLike:
        case PatternKind::JellyfishLike:
        case PatternKind::FinnedFishLike: extra_cap = 12; break;
        case PatternKind::AlsLike: keep_bits = 4; break;
        case PatternKind::OverlayLike: extra_cap = 14; break;
        default: break;
    }

    const uint64_t full = pf_full_mask_for_n(topo.n);
    for (int i = 0; i < core_count; ++i) {
        plan.add_skeleton(plan.anchor_idx[static_cast<size_t>(i)], plan.anchor_masks[static_cast<size_t>(i)] & full);
    }

    uint64_t base_core = exact_family_core_mask(plan, 0, std::max(core_count, 1));
    if (base_core == 0ULL) base_core = truncate_mask_bits(plan.anchor_masks[0], keep_bits);

    int added = 0;
    for (int idx = 0; idx < topo.nn && added < extra_cap; ++idx) {
        if (plan.find_anchor(idx) >= 0 || plan.find_skeleton(idx) >= 0) continue;
        const int row = topo.cell_row[static_cast<size_t>(idx)];
        const int col = topo.cell_col[static_cast<size_t>(idx)];
        const int box = topo.cell_box[static_cast<size_t>(idx)];
        int seen = 0;
        uint64_t mask = base_core;
        for (int i = 0; i < core_count; ++i) {
            const int a = plan.anchor_idx[static_cast<size_t>(i)];
            if (topo.cell_row[static_cast<size_t>(a)] == row ||
                topo.cell_col[static_cast<size_t>(a)] == col ||
                topo.cell_box[static_cast<size_t>(a)] == box) {
                ++seen;
                mask |= truncate_mask_bits(plan.anchor_masks[static_cast<size_t>(i)], keep_bits);
            }
        }
        mask = truncate_mask_bits(mask & full, keep_bits);
        if (seen >= 2 && std::popcount(mask) >= 2 && mask != full) {
            if (plan.add_skeleton(idx, mask)) ++added;
        }
    }
}

inline void protect_exact_template_skeleton(PatternScratch& sc, const ExactPatternTemplatePlan& plan, RequiredStrategy) {
    for (int i = 0; i < plan.skeleton_count; ++i) {
        const int idx = plan.skeleton_idx[static_cast<size_t>(i)];
        if (idx < 0 || idx >= sc.prepared_nn) continue;
        if (plan.find_anchor(idx) >= 0) continue;
        sc.protected_cells[static_cast<size_t>(idx)] = 1U;
    }
    sc.corridor_protected = true;
}

struct PatternMutationState {
    RequiredStrategy strategy = RequiredStrategy::None;
    PatternKind kind = PatternKind::None;
    bool have_last = false;
    bool have_best = false;
    ExactPatternTemplatePlan last_plan{};
    ExactPatternTemplatePlan best_plan{};
    int last_score = -1;
    int best_score = -1;
    int failure_streak = 0;
    int zero_use_streak = 0;

    void reset(RequiredStrategy rs, PatternKind pk) {
        strategy = rs;
        kind = pk;
        have_last = false;
        have_best = false;
        last_plan = {};
        best_plan = {};
        last_score = -1;
        best_score = -1;
        failure_streak = 0;
        zero_use_streak = 0;
    }
};

inline PatternMutationState& tls_pattern_mutation_state() {
    thread_local PatternMutationState s;
    return s;
}

inline uint64_t random_digit_mask(int n, int want, std::mt19937_64& rng) {
    if (n <= 0) return 0ULL;
    const int k = std::clamp(want, 1, std::min(n, 63));
    uint64_t m = 0ULL;
    int placed = 0;
    int guard = 0;
    while (placed < k && guard < n * 8) {
        ++guard;
        const int d0 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const uint64_t bit = 1ULL << d0;
        if ((m & bit) != 0ULL) continue;
        m |= bit;
        ++placed;
    }
    if (m == 0ULL) m = (1ULL << (rng() % static_cast<uint64_t>(n)));
    return m;
}

} // namespace sudoku_hpc::pattern_forcing



