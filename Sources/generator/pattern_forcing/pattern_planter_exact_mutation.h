#pragma once

#include <bit>
#include <cstdint>
#include <random>

#include "pattern_planter_exact_scoring.h"

namespace sudoku_hpc::pattern_forcing {

inline uint64_t pick_random_bit_from_mask(uint64_t mask, std::mt19937_64& rng) {
    const int bits = std::popcount(mask);
    if (bits <= 0) return 0ULL;
    int pick = static_cast<int>(rng() % static_cast<uint64_t>(bits));
    while (mask != 0ULL) {
        const int d0 = std::countr_zero(mask);
        const uint64_t bit = 1ULL << d0;
        if (pick-- == 0) return bit;
        mask &= ~bit;
    }
    return 0ULL;
}

inline uint64_t random_extra_digit(uint64_t full, uint64_t base, std::mt19937_64& rng) {
    return pick_random_bit_from_mask(full & ~base, rng);
}

inline int adaptive_mutation_strength(RequiredStrategy required, int zero_use_streak, int failure_streak, PatternMutationSource src) {
    int strength = 1 + std::min(4, zero_use_streak / 2) + std::min(4, failure_streak / 2);
    if (strategy_prefers_named_structures_before_generic(required)) ++strength;
    if (strategy_suppress_equivalent_generic_families(required)) ++strength;
    if (src == PatternMutationSource::Best) ++strength;
    return std::clamp(strength, 1, 8);
}

inline bool mutate_mask_keep_min_bits(uint64_t full, uint64_t& mask, std::mt19937_64& rng, int min_bits, int max_bits) {
    mask &= full;
    int bits = std::popcount(mask);
    bool changed = false;
    if (bits < min_bits) {
        while (bits < min_bits) {
            const uint64_t add = random_extra_digit(full, mask, rng);
            if (add == 0ULL) break;
            mask |= add;
            ++bits;
            changed = true;
        }
    }
    while (bits > max_bits) {
        const uint64_t rem = pick_random_bit_from_mask(mask, rng);
        if (rem == 0ULL) break;
        mask &= ~rem;
        --bits;
        changed = true;
    }
    return changed;
}

inline bool mutate_generic_template_plan(const GenericTopology& topo, ExactPatternTemplatePlan& plan, std::mt19937_64& rng, int strength) {
    if (!plan.valid || plan.anchor_count <= 0) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        changed |= mutate_mask_keep_min_bits(full, plan.anchor_masks[static_cast<size_t>(idx)], rng, 2, 4);
    }
    return changed;
}

inline bool mutate_exocet_template_plan(const GenericTopology& topo, ExactPatternTemplatePlan& plan, std::mt19937_64& rng, bool senior_mode, int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    const uint64_t base_mask = plan.anchor_masks[0] & plan.anchor_masks[1];
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = 2 + static_cast<int>(rng() % static_cast<uint64_t>(std::max(1, plan.anchor_count - 2)));
        uint64_t& mask = plan.anchor_masks[static_cast<size_t>(idx)];
        mask |= random_extra_digit(full, senior_mode ? 0ULL : base_mask, rng);
        changed |= mutate_mask_keep_min_bits(full, mask, rng, 3, senior_mode ? 5 : 4);
    }
    return changed;
}

inline bool mutate_exact_template_for_family(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {

    switch (required_strategy) {
        case RequiredStrategy::Exocet:
            return mutate_exocet_template_plan(topo, plan, rng, false, strength);
        case RequiredStrategy::SeniorExocet:
            return mutate_exocet_template_plan(topo, plan, rng, true, strength);
        default:
            return mutate_generic_template_plan(topo, plan, rng, strength);
    }
}

} // namespace sudoku_hpc::pattern_forcing
