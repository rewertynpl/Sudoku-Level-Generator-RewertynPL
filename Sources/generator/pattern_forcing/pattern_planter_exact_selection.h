// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: pattern_planter_exact_selection.h
// Opis: Wybiera i dopasowuje szablony geometryczne (Exact Templates) dla
//       żądanych strategii. Decyduje o doborze odpowiedniego buildera,
//       a także obsługuje kontrolowany fallback w obrębie tej samej rodziny
//       lub pokrewnego exact-core, minimalizując martwe ścieżki generatora.
//       Implementacja pozostaje zero-allocation na hot path.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include "pattern_planter_exact_mutation.h"

namespace sudoku_hpc::pattern_forcing {

inline bool try_exact_templates_for_level(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    int forcing_level,
    std::mt19937_64& rng,
    ExactPatternTemplatePlan& exact_plan,
    PatternKind& out_kind,
    int* out_score = nullptr) {

    exact_plan = {};
    out_kind = PatternKind::None;

    int best_score = -1;
    int best_family_score = -1;
    PatternKind best_any_kind = PatternKind::None;
    PatternKind best_family_kind = PatternKind::None;
    ExactPatternTemplatePlan best_any_plan{};
    ExactPatternTemplatePlan best_family_plan{};

    const PatternStrategyPolicy preferred_policy = pattern_strategy_policy(required_strategy, forcing_level);
    const PatternKind preferred_kind = preferred_policy.kind;
    const bool strict_exact_family = pattern_policy_requires_exact(preferred_policy.generator_policy);
    const bool allow_family_fallback = pattern_policy_allows_family_fallback(preferred_policy.generator_policy);
    const int n = topo.n;

    auto same_family_or_compatible = [&](PatternKind candidate_kind) -> bool {
        if (candidate_kind == preferred_kind) {
            return true;
        }

        switch (preferred_kind) {
        case PatternKind::LoopLike:
            return candidate_kind == PatternKind::LoopLike;
        case PatternKind::AlsLike:
            return candidate_kind == PatternKind::AlsLike ||
                   candidate_kind == PatternKind::AicLike ||
                   candidate_kind == PatternKind::GroupedAicLike;
        case PatternKind::AicLike:
            return candidate_kind == PatternKind::AicLike ||
                   candidate_kind == PatternKind::GroupedAicLike ||
                   candidate_kind == PatternKind::NiceLoopLike ||
                   candidate_kind == PatternKind::XChainLike ||
                   candidate_kind == PatternKind::XYChainLike;
        case PatternKind::GroupedAicLike:
            return candidate_kind == PatternKind::GroupedAicLike ||
                   candidate_kind == PatternKind::GroupedCycleLike ||
                   candidate_kind == PatternKind::NiceLoopLike ||
                   candidate_kind == PatternKind::AicLike;
        case PatternKind::GroupedCycleLike:
            return candidate_kind == PatternKind::GroupedCycleLike ||
                   candidate_kind == PatternKind::GroupedAicLike ||
                   candidate_kind == PatternKind::NiceLoopLike ||
                   candidate_kind == PatternKind::XChainLike;
        case PatternKind::NiceLoopLike:
            return candidate_kind == PatternKind::NiceLoopLike ||
                   candidate_kind == PatternKind::GroupedCycleLike ||
                   candidate_kind == PatternKind::GroupedAicLike ||
                   candidate_kind == PatternKind::AicLike ||
                   candidate_kind == PatternKind::XChainLike;
        case PatternKind::XChainLike:
            return candidate_kind == PatternKind::XChainLike ||
                   candidate_kind == PatternKind::GroupedCycleLike ||
                   candidate_kind == PatternKind::NiceLoopLike ||
                   candidate_kind == PatternKind::AicLike;
        case PatternKind::XYChainLike:
            return candidate_kind == PatternKind::XYChainLike ||
                   candidate_kind == PatternKind::AicLike ||
                   candidate_kind == PatternKind::AlsLike;
        case PatternKind::FishLike:
        case PatternKind::FrankenLike:
        case PatternKind::MutantLike:
        case PatternKind::SquirmLike:
        case PatternKind::SwordfishLike:
        case PatternKind::JellyfishLike:
        case PatternKind::FinnedFishLike:
            return candidate_kind == PatternKind::FishLike ||
                   candidate_kind == PatternKind::FrankenLike ||
                   candidate_kind == PatternKind::MutantLike ||
                   candidate_kind == PatternKind::SquirmLike ||
                   candidate_kind == PatternKind::SwordfishLike ||
                   candidate_kind == PatternKind::JellyfishLike ||
                   candidate_kind == PatternKind::FinnedFishLike;
        case PatternKind::ColorLike:
            return candidate_kind == PatternKind::ColorLike ||
                   candidate_kind == PatternKind::AicLike ||
                   candidate_kind == PatternKind::GroupedAicLike;
        default:
            return false;
        }
    };

    auto consider = [&](PatternKind candidate_kind, const ExactPatternTemplatePlan& candidate_plan) {
        if (!candidate_plan.valid) {
            return;
        }
        const int score = score_exact_plan(topo, required_strategy, candidate_kind, candidate_plan);
        if (score < 0) {
            return;
        }
        if (score > best_score) {
            best_score = score;
            best_any_plan = candidate_plan;
            best_any_kind = candidate_kind;
        }
        if (same_family_or_compatible(candidate_kind) && score > best_family_score) {
            best_family_score = score;
            best_family_plan = candidate_plan;
            best_family_kind = candidate_kind;
        }
    };

    auto repeat_attempts = [&](int tries, auto&& builder) {
        for (int attempt = 0; attempt < tries; ++attempt) {
            builder();
        }
    };

    const int large_board_bonus = (n >= 12) ? 2 : 0;
    const int chain_retry = 4 + large_board_bonus;
    const int grouped_retry = 6 + large_board_bonus;
    const int forcing_retry = 8 + large_board_bonus;
    const int loop_retry = 8 + large_board_bonus;
    const int overlay_retry = 10 + large_board_bonus;
    const int fish_retry = 4 + large_board_bonus;
    const int exocet_retry = 12 + large_board_bonus;

    auto try_exocet = [&](bool senior_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateExocet::build(topo, rng, candidate, senior_mode)) {
            consider(PatternKind::ExocetLike, candidate);
        }
    };
    auto try_sk = [&](bool dense_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateSKLoop::build(topo, rng, candidate, dense_mode)) {
            consider(PatternKind::LoopLike, candidate);
        }
    };
    auto try_msls = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateMslsOverlay::build_msls(topo, rng, candidate)) {
            consider(PatternKind::LoopLike, candidate);
        }
    };
    auto try_overlay = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateMslsOverlay::build_overlay(topo, rng, candidate)) {
            consider(PatternKind::LoopLike, candidate);
        }
    };
    auto try_forcing = [&](bool dynamic_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateForcing::build(topo, rng, candidate, dynamic_mode)) {
            consider(PatternKind::ForcingLike, candidate);
        }
    };
    auto try_medusa = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_medusa(topo, rng, candidate)) {
            consider(PatternKind::ColorLike, candidate);
        }
    };
    auto try_death_blossom = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_death_blossom(topo, rng, candidate)) {
            consider(PatternKind::PetalLike, candidate);
        }
    };
    auto try_suedecoq = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_sue_de_coq(topo, rng, candidate)) {
            consider(PatternKind::IntersectionLike, candidate);
        }
    };
    auto try_kraken = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_kraken_fish(topo, rng, candidate)) {
            consider(PatternKind::FishLike, candidate);
        }
    };
    auto try_franken = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_franken_fish(topo, rng, candidate)) {
            consider(PatternKind::FrankenLike, candidate);
        }
    };
    auto try_mutant = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_mutant_fish(topo, rng, candidate)) {
            consider(PatternKind::MutantLike, candidate);
        }
    };
    auto try_squirm = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_squirmbag(topo, rng, candidate)) {
            consider(PatternKind::SquirmLike, candidate);
        }
    };
    auto try_als_xy = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_als_xy_wing(topo, rng, candidate)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_als_xz = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_als_xz(topo, rng, candidate)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_wxyz = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_wxyz_wing(topo, rng, candidate)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_als_chain = [&](bool aic_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_als_chain(topo, rng, candidate, aic_mode)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_exclusion = [&](bool triple_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_aligned_exclusion(topo, rng, candidate, triple_mode)) {
            consider(PatternKind::ExclusionLike, candidate);
        }
    };
    auto try_aic = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_aic(topo, rng, candidate)) {
            consider(PatternKind::AicLike, candidate);
        }
    };
    auto try_grouped_aic = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_grouped_aic(topo, rng, candidate)) {
            consider(PatternKind::GroupedAicLike, candidate);
        }
    };
    auto try_grouped_xcycle = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_grouped_x_cycle(topo, rng, candidate)) {
            consider(PatternKind::GroupedCycleLike, candidate);
        }
    };
    auto try_continuous_nice_loop = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_continuous_nice_loop(topo, rng, candidate)) {
            consider(PatternKind::NiceLoopLike, candidate);
        }
    };
    auto try_xchain = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_x_chain(topo, rng, candidate)) {
            consider(PatternKind::XChainLike, candidate);
        }
    };
    auto try_xychain = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_xy_chain(topo, rng, candidate)) {
            consider(PatternKind::XYChainLike, candidate);
        }
    };
    auto try_empty_rectangle = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_empty_rectangle(topo, rng, candidate)) {
            consider(PatternKind::EmptyRectangleLike, candidate);
        }
    };
    auto try_remote_pairs = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_remote_pairs(topo, rng, candidate)) {
            consider(PatternKind::RemotePairsLike, candidate);
        }
    };
    auto try_large_fish = [&](PatternKind kind, int line_count, bool finned_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_large_fish(topo, rng, candidate, line_count, finned_mode)) {
            consider(kind, candidate);
        }
    };

    switch (required_strategy) {
    case RequiredStrategy::Exocet:
        repeat_attempts(exocet_retry, [&]() { try_exocet(false); });
        break;
    case RequiredStrategy::SeniorExocet:
        repeat_attempts(exocet_retry, [&]() { try_exocet(true); });
        break;
    case RequiredStrategy::SKLoop:
        repeat_attempts(loop_retry, [&]() { try_sk(false); });
        repeat_attempts(loop_retry / 2 + 1, [&]() { try_sk(true); });
        repeat_attempts(loop_retry / 2 + 1, [&]() { try_overlay(); });
        break;
    case RequiredStrategy::MSLS:
        repeat_attempts(overlay_retry, [&]() { try_msls(); });
        repeat_attempts(overlay_retry / 2, [&]() { try_overlay(); });
        break;
    case RequiredStrategy::PatternOverlayMethod:
        repeat_attempts(overlay_retry, [&]() { try_overlay(); });
        repeat_attempts(overlay_retry / 3 + 1, [&]() { try_msls(); });
        repeat_attempts(overlay_retry / 3 + 1, [&]() { try_sk(true); });
        break;
    case RequiredStrategy::Medusa3D:
        repeat_attempts(chain_retry, [&]() { try_medusa(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_aic(); });
        break;
    case RequiredStrategy::DeathBlossom:
        repeat_attempts(chain_retry, [&]() { try_death_blossom(); });
        break;
    case RequiredStrategy::SueDeCoq:
        repeat_attempts(chain_retry, [&]() { try_suedecoq(); });
        break;
    case RequiredStrategy::KrakenFish:
        repeat_attempts(fish_retry + 2, [&]() { try_kraken(); });
        repeat_attempts(fish_retry, [&]() { try_large_fish(PatternKind::FinnedFishLike, 3, true); });
        break;
    case RequiredStrategy::FrankenFish:
        repeat_attempts(fish_retry + 2, [&]() { try_franken(); });
        repeat_attempts(fish_retry / 2 + 1, [&]() { try_large_fish(PatternKind::SwordfishLike, 3, false); });
        break;
    case RequiredStrategy::MutantFish:
        repeat_attempts(fish_retry + 2, [&]() { try_mutant(); });
        repeat_attempts(fish_retry / 2 + 1, [&]() { try_large_fish(PatternKind::JellyfishLike, 4, false); });
        break;
    case RequiredStrategy::Squirmbag:
        repeat_attempts(fish_retry + 3, [&]() { try_squirm(); });
        repeat_attempts(fish_retry / 2 + 1, [&]() { try_large_fish(PatternKind::JellyfishLike, 4, false); });
        break;
    case RequiredStrategy::ALSXYWing:
        repeat_attempts(chain_retry, [&]() { try_als_xy(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_als_chain(false); });
        break;
    case RequiredStrategy::ALSXZ:
        repeat_attempts(chain_retry, [&]() { try_als_xz(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_als_xy(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_als_chain(false); });
        break;
    case RequiredStrategy::WXYZWing:
        repeat_attempts(chain_retry, [&]() { try_wxyz(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_als_xz(); });
        break;
    case RequiredStrategy::ALSChain:
        repeat_attempts(chain_retry, [&]() { try_als_chain(false); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_als_xz(); });
        break;
    case RequiredStrategy::ALSAIC:
        repeat_attempts(chain_retry, [&]() { try_als_chain(true); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_aic(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_grouped_aic(); });
        break;
    case RequiredStrategy::AlignedPairExclusion:
        repeat_attempts(chain_retry, [&]() { try_exclusion(false); });
        break;
    case RequiredStrategy::AlignedTripleExclusion:
        repeat_attempts(chain_retry, [&]() { try_exclusion(true); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_exclusion(false); });
        break;
    case RequiredStrategy::ForcingChains:
        repeat_attempts(forcing_retry, [&]() { try_forcing(false); });
        repeat_attempts(forcing_retry / 2 + 1, [&]() { try_aic(); });
        break;
    case RequiredStrategy::DynamicForcingChains:
        repeat_attempts(forcing_retry, [&]() { try_forcing(true); });
        repeat_attempts(forcing_retry / 2 + 1, [&]() { try_forcing(false); });
        repeat_attempts(forcing_retry / 2 + 1, [&]() { try_aic(); });
        break;
    case RequiredStrategy::AIC:
        repeat_attempts(chain_retry, [&]() { try_aic(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_xchain(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_xychain(); });
        break;
    case RequiredStrategy::GroupedAIC:
        repeat_attempts(grouped_retry, [&]() { try_grouped_aic(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_grouped_xcycle(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_continuous_nice_loop(); });
        break;
    case RequiredStrategy::GroupedXCycle:
        repeat_attempts(grouped_retry, [&]() { try_grouped_xcycle(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_grouped_aic(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_continuous_nice_loop(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_xchain(); });
        break;
    case RequiredStrategy::ContinuousNiceLoop:
        repeat_attempts(grouped_retry, [&]() { try_continuous_nice_loop(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_grouped_xcycle(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_grouped_aic(); });
        repeat_attempts(grouped_retry / 2 + 1, [&]() { try_aic(); });
        break;
    case RequiredStrategy::XChain:
        repeat_attempts(chain_retry, [&]() { try_xchain(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_aic(); });
        break;
    case RequiredStrategy::XYChain:
        repeat_attempts(chain_retry, [&]() { try_xychain(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_aic(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_als_xy(); });
        break;
    case RequiredStrategy::Swordfish:
        repeat_attempts(fish_retry, [&]() { try_large_fish(PatternKind::SwordfishLike, 3, false); });
        break;
    case RequiredStrategy::Jellyfish:
        repeat_attempts(fish_retry, [&]() { try_large_fish(PatternKind::JellyfishLike, 4, false); });
        repeat_attempts(fish_retry / 2 + 1, [&]() { try_large_fish(PatternKind::SwordfishLike, 3, false); });
        break;
    case RequiredStrategy::XWing:
        repeat_attempts(fish_retry, [&]() { try_large_fish(PatternKind::SwordfishLike, 2, false); });
        break;
    case RequiredStrategy::FinnedXWingSashimi:
        repeat_attempts(fish_retry, [&]() { try_large_fish(PatternKind::FinnedFishLike, 2, true); });
        break;
    case RequiredStrategy::FinnedSwordfishJellyfish:
        repeat_attempts(fish_retry, [&]() { try_large_fish(PatternKind::FinnedFishLike, 3, true); });
        repeat_attempts(fish_retry / 2 + 1, [&]() { try_large_fish(PatternKind::JellyfishLike, 4, false); });
        break;
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
        repeat_attempts(chain_retry, [&]() { try_xchain(); });
        break;
    case RequiredStrategy::EmptyRectangle:
        repeat_attempts(chain_retry, [&]() { try_empty_rectangle(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_xchain(); });
        break;
    case RequiredStrategy::RemotePairs:
        repeat_attempts(chain_retry, [&]() { try_remote_pairs(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_xychain(); });
        break;
    case RequiredStrategy::SimpleColoring:
        repeat_attempts(chain_retry, [&]() { try_medusa(); });
        repeat_attempts(chain_retry / 2 + 1, [&]() { try_xchain(); });
        break;
    default:
        break;
    }

    if (best_family_score < 0) {
        switch (required_strategy) {
        case RequiredStrategy::GroupedXCycle:
            repeat_attempts(grouped_retry, [&]() { try_grouped_xcycle(); });
            repeat_attempts(grouped_retry, [&]() { try_grouped_aic(); });
            repeat_attempts(grouped_retry / 2 + 1, [&]() { try_continuous_nice_loop(); });
            break;
        case RequiredStrategy::ContinuousNiceLoop:
            repeat_attempts(grouped_retry, [&]() { try_continuous_nice_loop(); });
            repeat_attempts(grouped_retry, [&]() { try_grouped_xcycle(); });
            repeat_attempts(grouped_retry / 2 + 1, [&]() { try_grouped_aic(); });
            break;
        case RequiredStrategy::GroupedAIC:
            repeat_attempts(grouped_retry, [&]() { try_grouped_aic(); });
            repeat_attempts(grouped_retry, [&]() { try_grouped_xcycle(); });
            repeat_attempts(grouped_retry / 2 + 1, [&]() { try_continuous_nice_loop(); });
            break;
        case RequiredStrategy::AIC:
            repeat_attempts(chain_retry, [&]() { try_aic(); });
            repeat_attempts(chain_retry, [&]() { try_xchain(); });
            repeat_attempts(chain_retry / 2 + 1, [&]() { try_xychain(); });
            break;
        case RequiredStrategy::Medusa3D:
            repeat_attempts(chain_retry, [&]() { try_medusa(); });
            repeat_attempts(chain_retry, [&]() { try_aic(); });
            break;
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            repeat_attempts(forcing_retry, [&]() { try_forcing(required_strategy == RequiredStrategy::DynamicForcingChains); });
            repeat_attempts(forcing_retry / 2 + 1, [&]() { try_aic(); });
            break;
        case RequiredStrategy::MSLS:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::SKLoop:
            repeat_attempts(overlay_retry, [&]() { try_msls(); });
            repeat_attempts(overlay_retry, [&]() { try_overlay(); });
            repeat_attempts(overlay_retry / 2 + 1, [&]() { try_sk(true); });
            break;
        default:
            break;
        }
    }

    if (!strict_exact_family && forcing_level >= 8) {
        try_exocet(false);
        try_exocet(true);
        try_msls();
        try_overlay();
        try_sk(true);
        try_kraken();
        try_franken();
        try_mutant();
        try_squirm();
        try_death_blossom();
        try_medusa();
        try_suedecoq();
        try_als_xy();
        try_als_xz();
        try_wxyz();
        try_als_chain(true);
        try_exclusion(true);
        try_grouped_xcycle();
        try_continuous_nice_loop();
        try_grouped_aic();
        try_aic();
        try_xchain();
        try_xychain();
        try_empty_rectangle();
        try_remote_pairs();
        try_large_fish(PatternKind::SwordfishLike, 3, false);
        try_large_fish(PatternKind::SwordfishLike, 2, false);
        try_large_fish(PatternKind::JellyfishLike, 4, false);
        try_large_fish(PatternKind::FinnedFishLike, 3, true);
        try_forcing(true);
    } else if (!strict_exact_family && forcing_level >= 7) {
        try_msls();
        try_overlay();
        try_sk(true);
        try_death_blossom();
        try_medusa();
        try_suedecoq();
        try_franken();
        try_mutant();
        try_squirm();
        try_als_xy();
        try_als_xz();
        try_wxyz();
        try_als_chain(false);
        try_exclusion(false);
        try_grouped_xcycle();
        try_continuous_nice_loop();
        try_grouped_aic();
        try_aic();
        try_xchain();
        try_xychain();
        try_empty_rectangle();
        try_remote_pairs();
        try_large_fish(PatternKind::SwordfishLike, 3, false);
        try_large_fish(PatternKind::SwordfishLike, 2, false);
        try_large_fish(PatternKind::FinnedFishLike, 2, true);
        try_forcing(false);
        if (allow_family_fallback) {
            try_exocet(false);
        }
    }

    if (best_family_score >= 0) {
        exact_plan = best_family_plan;
        out_kind = best_family_kind;
        best_score = best_family_score;
    } else if (!strict_exact_family || allow_family_fallback) {
        exact_plan = best_any_plan;
        out_kind = best_any_kind;
    }

    if (out_score != nullptr) {
        *out_score = best_score;
    }

    return best_score >= 0 && exact_plan.valid;
}

} // namespace sudoku_hpc::pattern_forcing
