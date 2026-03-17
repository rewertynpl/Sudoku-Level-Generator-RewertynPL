//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <random>

#include "pattern_planter_anchor_builders.h"

namespace sudoku_hpc::pattern_forcing {
namespace detail {

inline bool build_family_fallback_seed(
    const GenericTopology& topo,
    PatternKind kind,
    PatternScratch& sc,
    std::mt19937_64& rng) {

    switch (kind) {
        case PatternKind::ExocetLike:
            return build_exocet_like_anchors(topo, sc, rng);
        case PatternKind::LoopLike:
            return build_loop_like_anchors(topo, sc, rng);
        case PatternKind::ColorLike:
            return build_color_like_anchors(topo, sc, rng);
        case PatternKind::PetalLike:
            return build_petal_like_anchors(topo, sc, rng);
        case PatternKind::IntersectionLike:
            return build_intersection_like_anchors(topo, sc, rng);
        case PatternKind::FishLike:
        case PatternKind::SwordfishLike:
        case PatternKind::JellyfishLike:
        case PatternKind::FinnedFishLike:
            return build_fish_like_anchors(topo, sc, rng);
        case PatternKind::FrankenLike:
            return build_franken_like_anchors(topo, sc, rng);
        case PatternKind::MutantLike:
            return build_mutant_like_anchors(topo, sc, rng);
        case PatternKind::SquirmLike:
            return build_squirm_like_anchors(topo, sc, rng);
        case PatternKind::AlsLike:
            return build_als_like_anchors(topo, sc, rng);
        case PatternKind::ExclusionLike:
            return build_exclusion_like_anchors(topo, sc, rng);
        case PatternKind::AicLike:
            return build_aic_like_anchors(topo, sc, rng);
        case PatternKind::GroupedAicLike:
            return build_grouped_aic_like_anchors(topo, sc, rng);
        case PatternKind::GroupedCycleLike:
            return build_grouped_cycle_like_anchors(topo, sc, rng);
        case PatternKind::NiceLoopLike:
            return build_niceloop_like_anchors(topo, sc, rng);
        case PatternKind::XChainLike:
            return build_aic_like_anchors(topo, sc, rng);
        case PatternKind::XYChainLike:
            return build_niceloop_like_anchors(topo, sc, rng);
        case PatternKind::EmptyRectangleLike:
            return build_empty_rectangle_like_anchors(topo, sc, rng);
        case PatternKind::RemotePairsLike:
            return build_remote_pairs_like_anchors(topo, sc, rng);
        case PatternKind::ForcingLike:
            return build_forcing_like_anchors(topo, sc, rng);
        case PatternKind::Chain:
            return build_chain_anchors(topo, sc, rng);
        default:
            return false;
    }
}

inline uint64_t normalized_seed_mask(int n, uint64_t mask) {
    const uint64_t full = pf_full_mask_for_n(n);
    mask &= full;
    return (mask != 0ULL) ? mask : full;
}

inline bool seed_cell_idx_valid(const GenericTopology& topo, int idx) {
    return idx >= 0 && idx < topo.nn;
}

inline void rebuild_anchor_masks_from_allowed(const GenericTopology& topo, PatternScratch& sc) {
    const uint64_t full = pf_full_mask_for_n(topo.n);
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        if (!seed_cell_idx_valid(topo, idx)) {
            sc.anchor_masks[static_cast<size_t>(i)] = 0ULL;
            continue;
        }
        uint64_t mask = sc.allowed_masks[static_cast<size_t>(idx)] & full;
        if (mask == 0ULL) {
            mask = full;
            sc.allowed_masks[static_cast<size_t>(idx)] = full;
        }
        sc.anchor_masks[static_cast<size_t>(i)] = mask;
    }
}

inline bool seed_anchor_masks_sound(const GenericTopology& topo, const PatternScratch& sc) {
    if (sc.anchor_count <= 0 || sc.anchor_count > static_cast<int>(sc.anchors.size())) {
        return false;
    }

    std::array<uint8_t, 4096> seen{};
    const uint64_t full = pf_full_mask_for_n(topo.n);
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        if (!seed_cell_idx_valid(topo, idx)) {
            return false;
        }
        if (idx >= static_cast<int>(seen.size())) {
            return false;
        }
        if (seen[static_cast<size_t>(idx)] != 0U) {
            return false;
        }
        seen[static_cast<size_t>(idx)] = 1U;

        const uint64_t am = sc.anchor_masks[static_cast<size_t>(i)] & full;
        const uint64_t lm = sc.allowed_masks[static_cast<size_t>(idx)] & full;
        if (am == 0ULL || lm == 0ULL) {
            return false;
        }
        if ((am & lm) == 0ULL) {
            return false;
        }
    }
    return true;
}

inline int count_unique_anchor_rows(const GenericTopology& topo, const PatternScratch& sc) {
    std::array<uint8_t, 64> used{};
    int count = 0;
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        const int row = topo.cell_row[static_cast<size_t>(idx)];
        if (row >= 0 && row < static_cast<int>(used.size()) && used[static_cast<size_t>(row)] == 0U) {
            used[static_cast<size_t>(row)] = 1U;
            ++count;
        }
    }
    return count;
}

inline int count_unique_anchor_cols(const GenericTopology& topo, const PatternScratch& sc) {
    std::array<uint8_t, 64> used{};
    int count = 0;
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        const int col = topo.cell_col[static_cast<size_t>(idx)];
        if (col >= 0 && col < static_cast<int>(used.size()) && used[static_cast<size_t>(col)] == 0U) {
            used[static_cast<size_t>(col)] = 1U;
            ++count;
        }
    }
    return count;
}

inline int count_unique_anchor_boxes(const GenericTopology& topo, const PatternScratch& sc) {
    std::array<uint8_t, 64> used{};
    int count = 0;
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        const int box = topo.cell_box[static_cast<size_t>(idx)];
        if (box >= 0 && box < static_cast<int>(used.size()) && used[static_cast<size_t>(box)] == 0U) {
            used[static_cast<size_t>(box)] = 1U;
            ++count;
        }
    }
    return count;
}

inline int max_anchor_popcount(const PatternScratch& sc) {
    int best = 0;
    for (int i = 0; i < sc.anchor_count; ++i) {
        best = std::max(best, static_cast<int>(std::popcount(sc.anchor_masks[static_cast<size_t>(i)])));
    }
    return best;
}

inline int min_anchor_popcount(const PatternScratch& sc) {
    int best = 65;
    for (int i = 0; i < sc.anchor_count; ++i) {
        best = std::min(best, static_cast<int>(std::popcount(sc.anchor_masks[static_cast<size_t>(i)])));
    }
    return (best == 65) ? 0 : best;
}

inline bool has_anchor_peer_pair(const GenericTopology& topo, const PatternScratch& sc) {
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int a = sc.anchors[static_cast<size_t>(i)];
        const int pb = topo.peer_offsets[static_cast<size_t>(a)];
        const int pe = topo.peer_offsets[static_cast<size_t>(a + 1)];
        for (int p = pb; p < pe; ++p) {
            const int b = topo.peers_flat[static_cast<size_t>(p)];
            for (int j = i + 1; j < sc.anchor_count; ++j) {
                if (sc.anchors[static_cast<size_t>(j)] == b) {
                    return true;
                }
            }
        }
    }
    return false;
}

inline bool validate_intersection_semantics(const GenericTopology& topo, const PatternScratch& sc) {
    if (sc.anchor_count < 3) {
        return false;
    }
    std::array<int, 64> row_count{};
    std::array<int, 64> col_count{};
    std::array<int, 64> box_count{};
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        ++row_count[static_cast<size_t>(topo.cell_row[static_cast<size_t>(idx)])];
        ++col_count[static_cast<size_t>(topo.cell_col[static_cast<size_t>(idx)])];
        ++box_count[static_cast<size_t>(topo.cell_box[static_cast<size_t>(idx)])];
    }
    int best_row = 0;
    int best_col = 0;
    int best_box = 0;
    for (int i = 0; i < topo.n; ++i) {
        best_row = std::max(best_row, row_count[static_cast<size_t>(i)]);
        best_col = std::max(best_col, col_count[static_cast<size_t>(i)]);
        best_box = std::max(best_box, box_count[static_cast<size_t>(i)]);
    }
    return best_box >= 2 && (best_row >= 2 || best_col >= 2);
}

inline bool validate_fish_semantics(const GenericTopology& topo, const PatternScratch& sc) {
    if (sc.anchor_count < 4) {
        return false;
    }
    const int rows = count_unique_anchor_rows(topo, sc);
    const int cols = count_unique_anchor_cols(topo, sc);
    return rows >= 2 && cols >= 2;
}

inline bool validate_loopish_semantics(const GenericTopology& topo, const PatternScratch& sc, int min_anchors) {
    if (sc.anchor_count < min_anchors) {
        return false;
    }
    return has_anchor_peer_pair(topo, sc);
}

inline bool validate_kind_semantics(const GenericTopology& topo, PatternKind kind, const PatternScratch& sc) {
    if (!seed_anchor_masks_sound(topo, sc)) {
        return false;
    }

    switch (kind) {
        case PatternKind::None:
            return false;

        case PatternKind::IntersectionLike:
            return validate_intersection_semantics(topo, sc);

        case PatternKind::FishLike:
        case PatternKind::SwordfishLike:
        case PatternKind::JellyfishLike:
        case PatternKind::FinnedFishLike:
        case PatternKind::FrankenLike:
        case PatternKind::MutantLike:
        case PatternKind::SquirmLike:
            return validate_fish_semantics(topo, sc);

        case PatternKind::AlsLike:
            return sc.anchor_count >= 2 && max_anchor_popcount(sc) >= 3;

        case PatternKind::ExclusionLike:
            return sc.anchor_count >= 2 && min_anchor_popcount(sc) >= 2;

        case PatternKind::AicLike:
        case PatternKind::GroupedAicLike:
        case PatternKind::GroupedCycleLike:
        case PatternKind::NiceLoopLike:
        case PatternKind::XChainLike:
        case PatternKind::XYChainLike:
        case PatternKind::ColorLike:
        case PatternKind::ForcingLike:
        case PatternKind::Chain:
            return validate_loopish_semantics(topo, sc, 2) && min_anchor_popcount(sc) >= 2;

        case PatternKind::RemotePairsLike:
            return validate_loopish_semantics(topo, sc, 4) && min_anchor_popcount(sc) >= 2;

        case PatternKind::EmptyRectangleLike:
            return sc.anchor_count >= 4 && count_unique_anchor_boxes(topo, sc) >= 1 &&
                   (count_unique_anchor_rows(topo, sc) >= 2 || count_unique_anchor_cols(topo, sc) >= 2);

        case PatternKind::LoopLike:
            return validate_loopish_semantics(topo, sc, 4);

        case PatternKind::ExocetLike:
        case PatternKind::PetalLike:
            return sc.anchor_count >= 4 && max_anchor_popcount(sc) >= 2;
    }
    return sc.anchor_count > 0;
}

inline bool finalize_seed_scratch(
    const GenericTopology& topo,
    const GenerateRunConfig& cfg,
    PatternKind kind,
    bool exact_template,
    PatternScratch& sc) {

    if (sc.anchor_count <= 0) {
        return false;
    }

    const uint64_t full = pf_full_mask_for_n(topo.n);
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        if (!seed_cell_idx_valid(topo, idx)) {
            return false;
        }
        uint64_t& slot = sc.allowed_masks[static_cast<size_t>(idx)];
        slot = normalized_seed_mask(topo.n, slot);
        sc.anchor_masks[static_cast<size_t>(i)] = slot;
    }

    for (int idx = 0; idx < topo.nn; ++idx) {
        uint64_t& slot = sc.allowed_masks[static_cast<size_t>(idx)];
        slot &= full;
        if (slot == 0ULL) {
            slot = full;
        }
    }

    rebuild_anchor_masks_from_allowed(topo, sc);
    if (!validate_kind_semantics(topo, kind, sc)) {
        return false;
    }

    if (cfg.pattern_forcing_lock_anchors) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            const int idx = sc.anchors[static_cast<size_t>(i)];
            sc.protected_cells[static_cast<size_t>(idx)] = static_cast<uint8_t>(1);
        }
    }

    sc.exact_template = exact_template;
    return true;
}

inline void fill_seed_output(
    PatternSeedView& out,
    PatternScratch& sc,
    const PatternStrategyPolicy& policy,
    PatternKind kind,
    bool exact_template,
    bool family_fallback_used,
    bool exact_contract_met,
    PatternMutationState& mutation,
    int adaptive_target_strength,
    PatternMutationSource mutation_source,
    int mutation_strength,
    int template_score,
    int template_score_delta) {

    out.kind = kind;
    out.generator_policy = policy.generator_policy;
    out.seed_puzzle = &sc.seed_puzzle;
    out.allowed_masks = &sc.allowed_masks;
    out.protected_cells = &sc.protected_cells;
    out.anchor_idx = sc.anchors.data();
    out.anchor_masks = sc.anchor_masks.data();
    out.anchor_count = sc.anchor_count;
    out.exact_template = exact_template;
    out.family_fallback_used = family_fallback_used;
    out.required_strategy_exact_contract_met = exact_contract_met;
    out.template_score = template_score;
    out.best_template_score = mutation.best_score;
    out.template_score_delta = template_score_delta;
    out.mutation_strength = mutation_strength;
    out.planner_zero_use_streak = mutation.zero_use_streak;
    out.planner_failure_streak = mutation.failure_streak;
    out.adaptive_target_strength = adaptive_target_strength;
    out.mutation_source = mutation_source;
}

inline bool apply_exact_plan_to_scratch(
    const GenericTopology& topo,
    const GenerateRunConfig& cfg,
    RequiredStrategy required_strategy,
    PatternKind default_kind,
    const ExactPatternTemplatePlan& exact_plan,
    PatternKind exact_kind,
    int exact_score,
    PatternScratch& sc) {

    if (!exact_plan.valid || exact_plan.anchor_count <= 0) {
        return false;
    }

    sc.exact_template = true;
    sc.template_score = exact_score;

    expand_exact_template_skeleton(
        topo,
        (exact_kind == PatternKind::None) ? default_kind : exact_kind,
        const_cast<ExactPatternTemplatePlan&>(exact_plan));

    for (int i = 0; i < exact_plan.anchor_count; ++i) {
        const int idx = exact_plan.anchor_idx[static_cast<size_t>(i)];
        if (!sc.add_anchor(idx)) {
            continue;
        }

        uint64_t mask = normalized_seed_mask(topo.n, exact_plan.anchor_masks[static_cast<size_t>(i)]);
        sc.allowed_masks[static_cast<size_t>(idx)] = mask;
        sc.anchor_masks[static_cast<size_t>(i)] = mask;
    }

    for (int i = 0; i < exact_plan.skeleton_count; ++i) {
        const int idx = exact_plan.skeleton_idx[static_cast<size_t>(i)];
        if (!seed_cell_idx_valid(topo, idx)) {
            continue;
        }
        const uint64_t mask = exact_plan.skeleton_masks[static_cast<size_t>(i)] & pf_full_mask_for_n(topo.n);
        if (mask == 0ULL) {
            continue;
        }

        uint64_t& slot = sc.allowed_masks[static_cast<size_t>(idx)];
        if (slot == 0ULL) {
            slot = mask;
        } else {
            slot &= mask;
            if (slot == 0ULL) {
                slot = mask;
            }
        }
    }

    if (cfg.pattern_forcing_lock_anchors) {
        protect_exact_template_skeleton(sc, exact_plan, required_strategy);
    }

    return finalize_seed_scratch(
        topo,
        cfg,
        (exact_kind == PatternKind::None) ? default_kind : exact_kind,
        true,
        sc);
}

inline bool choose_exact_template_plan(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    PatternKind kind,
    int forcing_level,
    std::mt19937_64& rng,
    PatternMutationState& mutation,
    bool wants_exact_templates,
    bool force_family_fallback,
    ExactPatternTemplatePlan& selected_plan,
    PatternKind& selected_kind,
    int& selected_score,
    PatternMutationSource& selected_source,
    int& selected_delta,
    int& selected_strength,
    int& adaptive_target_strength) {

    selected_plan = {};
    selected_kind = PatternKind::None;
    selected_score = -1;
    selected_source = PatternMutationSource::Random;
    selected_delta = 0;
    selected_strength = 0;
    adaptive_target_strength = 0;

    if (!wants_exact_templates || force_family_fallback) {
        return false;
    }

    bool have_plan = false;

    auto try_take_plan = [&](const ExactPatternTemplatePlan& plan,
                             PatternKind plan_kind,
                             int plan_score,
                             PatternMutationSource source,
                             int delta,
                             int strength) {
        if (!plan.valid || plan.anchor_count <= 0 || plan_score < 0) {
            return;
        }
        if (!have_plan || plan_score > selected_score) {
            selected_plan = plan;
            selected_kind = plan_kind;
            selected_score = plan_score;
            selected_source = source;
            selected_delta = delta;
            selected_strength = strength;
            have_plan = true;
        }
    };

    ExactPatternTemplatePlan base_plan{};
    PatternKind base_kind = PatternKind::None;
    int base_score = -1;
    if (try_exact_templates_for_level(
            topo, required_strategy, forcing_level, rng, base_plan, base_kind, &base_score)) {
        try_take_plan(base_plan, base_kind, base_score, PatternMutationSource::Random, 0, 0);
    }

    constexpr std::array<int, 4> kLevelOffsets{{-1, +1, -2, +2}};
    for (const int delta_level : kLevelOffsets) {
        const int candidate_level = std::clamp(forcing_level + delta_level, 1, 8);
        if (candidate_level == forcing_level) {
            continue;
        }
        ExactPatternTemplatePlan alt_plan{};
        PatternKind alt_kind = PatternKind::None;
        int alt_score = -1;
        if (try_exact_templates_for_level(
                topo, required_strategy, candidate_level, rng, alt_plan, alt_kind, &alt_score)) {
            try_take_plan(alt_plan, alt_kind, alt_score, PatternMutationSource::Random, 0, 0);
        }
    }

    if (mutation.have_last &&
        mutation.strategy == required_strategy &&
        mutation.kind == kind &&
        mutation.last_plan.valid &&
        (mutation.failure_streak > 0 || mutation.zero_use_streak > 0)) {
        ExactPatternTemplatePlan mutated = mutation.last_plan;
        const int strength = adaptive_mutation_strength(
            required_strategy, mutation.zero_use_streak, mutation.failure_streak, PatternMutationSource::Last);
        adaptive_target_strength = std::max(adaptive_target_strength, strength);
        if (mutate_exact_template_for_family(topo, required_strategy, mutated, rng, strength)) {
            const int mutated_score = score_exact_plan(topo, required_strategy, kind, mutated);
            const int floor_score = std::max(selected_score - 8, 0);
            if (mutated_score >= floor_score) {
                try_take_plan(
                    mutated,
                    kind,
                    mutated_score,
                    PatternMutationSource::Last,
                    mutated_score - std::max(0, mutation.last_score),
                    strength);
            }
        }
    }

    if (mutation.have_best &&
        mutation.strategy == required_strategy &&
        mutation.kind == kind &&
        mutation.best_plan.valid &&
        (mutation.zero_use_streak >= 2 || mutation.failure_streak >= 2)) {
        ExactPatternTemplatePlan mutated = mutation.best_plan;
        const int strength = adaptive_mutation_strength(
            required_strategy, mutation.zero_use_streak, mutation.failure_streak, PatternMutationSource::Best);
        adaptive_target_strength = std::max(adaptive_target_strength, strength);
        if (mutate_exact_template_for_family(topo, required_strategy, mutated, rng, strength)) {
            const int mutated_score = score_exact_plan(topo, required_strategy, kind, mutated);
            try_take_plan(
                mutated,
                kind,
                mutated_score,
                PatternMutationSource::Best,
                mutated_score - std::max(0, mutation.best_score),
                strength);
        }
    }

    return have_plan;
}

} // namespace detail

// Główna funkcja orkiestratora
inline bool build_seed(
    const GenericTopology& topo,
    const GenerateRunConfig& cfg,
    RequiredStrategy required_strategy,
    int difficulty_level_required,
    std::mt19937_64& rng,
    PatternSeedView& out) {

    out = {};
    if (!cfg.pattern_forcing_enabled) {
        return false;
    }

    int forcing_level = std::clamp(difficulty_level_required, 1, 8);
    if (cfg.max_pattern_depth > 0) {
        forcing_level = std::min(forcing_level, cfg.max_pattern_depth);
        forcing_level = std::clamp(forcing_level, 1, 8);
    }

    const PatternStrategyPolicy policy = pattern_strategy_policy(required_strategy, forcing_level);
    PatternKind kind = policy.kind;
    if (kind == PatternKind::None) {
        return false;
    }

    const bool strict_exact_family = pattern_policy_requires_exact(policy.generator_policy);
    const bool allows_family_fallback = pattern_policy_allows_family_fallback(policy.generator_policy);
    const bool wants_exact_templates = pattern_policy_supports_exact(policy.generator_policy);

    PatternScratch& sc = tls_pattern_scratch();
    sc.reset(topo);

    PatternMutationState& mutation = tls_pattern_mutation_state();
    if (mutation.strategy != required_strategy || mutation.kind != kind) {
        mutation.reset(required_strategy, kind);
    }

    const bool force_family_fallback =
        allows_family_fallback &&
        (required_strategy == RequiredStrategy::ForcingChains ||
         required_strategy == RequiredStrategy::DynamicForcingChains ||
         required_strategy == RequiredStrategy::GroupedXCycle ||
         required_strategy == RequiredStrategy::ContinuousNiceLoop) &&
        mutation.failure_streak >= 3;

    ExactPatternTemplatePlan exact_plan{};
    PatternKind exact_kind = PatternKind::None;
    int exact_score = -1;
    PatternMutationSource selected_source = PatternMutationSource::Random;
    int score_delta = 0;
    int selected_strength = 0;
    int adaptive_target_strength = 0;

    const bool have_exact_candidate = detail::choose_exact_template_plan(
        topo,
        required_strategy,
        kind,
        forcing_level,
        rng,
        mutation,
        wants_exact_templates,
        force_family_fallback,
        exact_plan,
        exact_kind,
        exact_score,
        selected_source,
        score_delta,
        selected_strength,
        adaptive_target_strength);

    if (have_exact_candidate &&
        detail::apply_exact_plan_to_scratch(
            topo,
            cfg,
            required_strategy,
            kind,
            exact_plan,
            exact_kind,
            exact_score,
            sc)) {

        mutation.strategy = required_strategy;
        mutation.kind = (exact_kind == PatternKind::None) ? kind : exact_kind;
        mutation.have_last = true;
        mutation.last_plan = exact_plan;
        mutation.last_score = exact_score;
        if (!mutation.have_best || exact_score > mutation.best_score) {
            mutation.have_best = true;
            mutation.best_plan = exact_plan;
            mutation.best_score = exact_score;
        }

        detail::fill_seed_output(
            out,
            sc,
            policy,
            (exact_kind == PatternKind::None) ? kind : exact_kind,
            true,
            false,
            !pattern_policy_requires_exact(policy.generator_policy) || sc.exact_template,
            mutation,
            adaptive_target_strength,
            selected_source,
            selected_strength,
            sc.template_score,
            score_delta);
        return true;
    }

    if (strict_exact_family) {
        return false;
    }
    if (!allows_family_fallback) {
        return false;
    }

    sc.reset(topo);
    bool ok = detail::build_family_fallback_seed(topo, kind, sc, rng);
    if (!ok || sc.anchor_count <= 0) {
        return false;
    }

    int anchor_target = cfg.pattern_forcing_anchor_count > 0
        ? cfg.pattern_forcing_anchor_count
        : default_anchor_count(topo, kind);
    anchor_target = std::clamp(anchor_target, sc.anchor_count, std::min(topo.nn, 32));

    int guard = 0;
    while (sc.anchor_count < anchor_target && guard < topo.nn * 4) {
        ++guard;
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        sc.add_anchor(idx);
    }

    apply_anchor_masks(topo, sc, kind, rng);
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        sc.anchor_masks[static_cast<size_t>(i)] = sc.allowed_masks[static_cast<size_t>(idx)];
    }

    if (cfg.pattern_forcing_lock_anchors) {
        protect_pattern_cells_from_masks(topo, sc);
    }

    if (!detail::finalize_seed_scratch(topo, cfg, kind, false, sc)) {
        return false;
    }

    detail::fill_seed_output(
        out,
        sc,
        policy,
        kind,
        false,
        true,
        !pattern_policy_requires_exact(policy.generator_policy),
        mutation,
        adaptive_target_strength,
        PatternMutationSource::Random,
        0,
        0,
        0);
    return true;
}

} // namespace sudoku_hpc::pattern_forcing
