//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <array>
#include <cstdint>

#include "pattern_planter_types.h"

namespace sudoku_hpc::pattern_forcing {

struct PatternStrategyPolicy {
    PatternKind kind = PatternKind::None;
    PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;

    // Planner hints.
    bool prefer_exact = false;
    bool preserve_anchors = false;
    bool suppress_equivalent_generics = false;

    // Additional stability hints for generator / digger / certifier handshake.
    bool prefer_named_before_generic = false;
    bool prefer_split_clue_windows = false;
    bool anti_single_guard = false;
    bool allow_on_primary_3x3 = true;
    bool exact_sensitive = false;

    int recommended_failure_streak_before_family_fallback = 0;
    int min_geometry_n = 1;
};

inline bool pattern_policy_requires_exact(PatternGeneratorPolicy policy) {
    return policy == PatternGeneratorPolicy::ExactRequired;
}

inline bool pattern_policy_allows_family_fallback(PatternGeneratorPolicy policy) {
    return policy == PatternGeneratorPolicy::ExactPreferredFamilyFallback ||
           policy == PatternGeneratorPolicy::FamilyOnly;
}

inline bool pattern_policy_supports_exact(PatternGeneratorPolicy policy) {
    return policy == PatternGeneratorPolicy::ExactRequired ||
           policy == PatternGeneratorPolicy::ExactPreferredFamilyFallback;
}

inline bool pattern_policy_is_named_structure_focus(RequiredStrategy required) {
    return strategy_prefers_named_structures_before_generic(required) ||
           strategy_suppress_equivalent_generic_families(required);
}

inline PatternKind pick_kind(RequiredStrategy required, int /*level*/) {
    switch (required) {
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return PatternKind::ExocetLike;

    case RequiredStrategy::SKLoop:
    case RequiredStrategy::UniqueRectangle:
    case RequiredStrategy::UniqueLoop:
    case RequiredStrategy::AvoidableRectangle:
    case RequiredStrategy::BivalueOddagon:
    case RequiredStrategy::UniqueRectangleExtended:
    case RequiredStrategy::HiddenUniqueRectangle:
    case RequiredStrategy::BUGPlusOne:
    case RequiredStrategy::BUGType2:
    case RequiredStrategy::BUGType3:
    case RequiredStrategy::BUGType4:
    case RequiredStrategy::BorescoperQiuDeadlyPattern:
        return PatternKind::LoopLike;

    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::MSLS:
        return PatternKind::ForcingLike;

    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::SimpleColoring:
        return PatternKind::ColorLike;

    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
        return PatternKind::PetalLike;

    case RequiredStrategy::PointingPairs:
    case RequiredStrategy::BoxLineReduction:
        return PatternKind::IntersectionLike;

    case RequiredStrategy::XWing:
        return PatternKind::FishLike;
    case RequiredStrategy::Swordfish:
        return PatternKind::SwordfishLike;
    case RequiredStrategy::Jellyfish:
        return PatternKind::JellyfishLike;
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
        return PatternKind::FinnedFishLike;
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
        return PatternKind::FishLike;
    case RequiredStrategy::EmptyRectangle:
        return PatternKind::EmptyRectangleLike;
    case RequiredStrategy::RemotePairs:
        return PatternKind::RemotePairsLike;

    case RequiredStrategy::FrankenFish:
        return PatternKind::FrankenLike;
    case RequiredStrategy::MutantFish:
        return PatternKind::MutantLike;
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::KrakenFish:
        return PatternKind::SquirmLike;

    case RequiredStrategy::YWing:
    case RequiredStrategy::XYZWing:
    case RequiredStrategy::WWing:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
        return PatternKind::AlsLike;

    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
        return PatternKind::ExclusionLike;

    case RequiredStrategy::AIC:
        return PatternKind::AicLike;
    case RequiredStrategy::GroupedAIC:
        return PatternKind::GroupedAicLike;
    case RequiredStrategy::GroupedXCycle:
        return PatternKind::GroupedCycleLike;
    case RequiredStrategy::ContinuousNiceLoop:
        return PatternKind::NiceLoopLike;
    case RequiredStrategy::XChain:
        return PatternKind::XChainLike;
    case RequiredStrategy::XYChain:
        return PatternKind::XYChainLike;

    default:
        return PatternKind::None;
    }
}

inline PatternStrategyPolicy pattern_strategy_policy(RequiredStrategy required, int level) {
    const int clamped_level = (level < 1) ? 1 : (level > 8 ? 8 : level);

    PatternStrategyPolicy out{};
    out.kind = pick_kind(required, clamped_level);
    out.prefer_named_before_generic = pattern_policy_is_named_structure_focus(required);
    out.prefer_split_clue_windows = strategy_prefers_generator_certifier_split(required);
    out.anti_single_guard = (clamped_level >= 3) || out.prefer_named_before_generic || out.prefer_split_clue_windows;
    
    // Zmiana minimalnej geometrii na 9 dla Exoceta, Loopów i struktur Theoretical
    if (strategy_requires_n_at_least_12(required)) {
        out.min_geometry_n = 12;
        out.allow_on_primary_3x3 = false;
    } else if (required == RequiredStrategy::Exocet || 
               required == RequiredStrategy::SeniorExocet || 
               required == RequiredStrategy::SKLoop || 
               required == RequiredStrategy::PatternOverlayMethod ||
               required == RequiredStrategy::MSLS) {
        out.min_geometry_n = 9;
        out.allow_on_primary_3x3 = true;
    } else {
        out.min_geometry_n = 1;
        out.allow_on_primary_3x3 = true;
    }

    out.exact_sensitive = strategy_requires_exact_only(required) || out.prefer_named_before_generic ||
                          out.prefer_split_clue_windows || clamped_level >= 4;

    out.prefer_exact = out.exact_sensitive || clamped_level >= 4;
    out.preserve_anchors = (clamped_level >= 3) || out.exact_sensitive ||
                           required == RequiredStrategy::PointingPairs ||
                           required == RequiredStrategy::BoxLineReduction;
    out.suppress_equivalent_generics = strategy_suppress_equivalent_generic_families(required) ||
                                       (out.prefer_named_before_generic && clamped_level >= 3);
    
    // Obniżenie progów mutacji - zapobiega nadmiernej agresji DLX na klasycznym 3x3
    out.recommended_failure_streak_before_family_fallback = out.exact_sensitive ? 5 : 3;

    switch (required) {
    case RequiredStrategy::None:
    case RequiredStrategy::Backtracking:
    case RequiredStrategy::NakedSingle:
    case RequiredStrategy::HiddenSingle:
    case RequiredStrategy::NakedPair:
    case RequiredStrategy::HiddenPair:
    case RequiredStrategy::NakedTriple:
    case RequiredStrategy::HiddenTriple:
    case RequiredStrategy::NakedQuad:
    case RequiredStrategy::HiddenQuad:
        out.generator_policy = PatternGeneratorPolicy::Unsupported;
        out.prefer_exact = false;
        out.preserve_anchors = false;
        out.suppress_equivalent_generics = false;
        out.anti_single_guard = false;
        out.exact_sensitive = false;
        out.recommended_failure_streak_before_family_fallback = 0;
        break;

    case RequiredStrategy::PointingPairs:
    case RequiredStrategy::BoxLineReduction:
        out.generator_policy = PatternGeneratorPolicy::FamilyOnly;
        out.prefer_exact = false;
        out.preserve_anchors = true;
        out.suppress_equivalent_generics = true;
        out.prefer_named_before_generic = true;
        out.prefer_split_clue_windows = true;
        out.anti_single_guard = true;
        out.exact_sensitive = true;
        out.recommended_failure_streak_before_family_fallback = 0;
        break;

    case RequiredStrategy::YWing:
    case RequiredStrategy::XYZWing:
    case RequiredStrategy::WWing:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::XWing:
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
    case RequiredStrategy::SimpleColoring:
    case RequiredStrategy::BUGPlusOne:
    case RequiredStrategy::UniqueRectangle:
    case RequiredStrategy::UniqueLoop:
    case RequiredStrategy::AvoidableRectangle:
    case RequiredStrategy::BivalueOddagon:
    case RequiredStrategy::UniqueRectangleExtended:
    case RequiredStrategy::HiddenUniqueRectangle:
    case RequiredStrategy::BUGType2:
    case RequiredStrategy::BUGType3:
    case RequiredStrategy::BUGType4:
    case RequiredStrategy::BorescoperQiuDeadlyPattern:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
    case RequiredStrategy::ALSXZ:
        out.generator_policy = PatternGeneratorPolicy::ExactPreferredFamilyFallback;
        out.prefer_exact = true;
        out.preserve_anchors = true;
        out.suppress_equivalent_generics = true;
        out.prefer_named_before_generic = true;
        out.prefer_split_clue_windows = true;
        out.anti_single_guard = true;
        out.exact_sensitive = true;
        out.recommended_failure_streak_before_family_fallback = 5; // Było 8
        break;

    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::MSLS:
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::Squirmbag:
        out.generator_policy = PatternGeneratorPolicy::ExactPreferredFamilyFallback;
        out.prefer_exact = true;
        out.preserve_anchors = true;
        out.suppress_equivalent_generics = true;
        out.prefer_named_before_generic = true;
        out.prefer_split_clue_windows = true;
        out.anti_single_guard = true;
        out.exact_sensitive = true;
        out.recommended_failure_streak_before_family_fallback = 6; // Było 10
        break;

    default:
        out.generator_policy = (out.kind == PatternKind::None)
            ? PatternGeneratorPolicy::Unsupported
            : PatternGeneratorPolicy::FamilyOnly;
        break;
    }

    // Guardrail: do not report unsupported exact routing for strategies with no mapped family.
    if (out.kind == PatternKind::None) {
        out.generator_policy = PatternGeneratorPolicy::Unsupported;
        out.prefer_exact = false;
        out.exact_sensitive = false;
        out.preserve_anchors = false;
        out.suppress_equivalent_generics = false;
        out.recommended_failure_streak_before_family_fallback = 0;
    }

    return out;
}

inline bool pattern_dispatch_wired(RequiredStrategy required, int level) {
    return pick_kind(required, level) != PatternKind::None;
}

inline bool pattern_exact_template_dispatch_wired(RequiredStrategy required, int /*level*/) {
    switch (required) {
    case RequiredStrategy::YWing:
    case RequiredStrategy::XYZWing:
    case RequiredStrategy::WWing:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::XWing:
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
    case RequiredStrategy::SimpleColoring:
    case RequiredStrategy::UniqueRectangle:
    case RequiredStrategy::BUGPlusOne:
    case RequiredStrategy::UniqueLoop:
    case RequiredStrategy::AvoidableRectangle:
    case RequiredStrategy::BivalueOddagon:
    case RequiredStrategy::UniqueRectangleExtended:
    case RequiredStrategy::HiddenUniqueRectangle:
    case RequiredStrategy::BUGType2:
    case RequiredStrategy::BUGType3:
    case RequiredStrategy::BUGType4:
    case RequiredStrategy::BorescoperQiuDeadlyPattern:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::MSLS:
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::Squirmbag:
        return true;

    case RequiredStrategy::PointingPairs:
    case RequiredStrategy::BoxLineReduction:
    case RequiredStrategy::None:
    case RequiredStrategy::Backtracking:
    case RequiredStrategy::NakedSingle:
    case RequiredStrategy::HiddenSingle:
    case RequiredStrategy::NakedPair:
    case RequiredStrategy::HiddenPair:
    case RequiredStrategy::NakedTriple:
    case RequiredStrategy::HiddenTriple:
    case RequiredStrategy::NakedQuad:
    case RequiredStrategy::HiddenQuad:
    default:
        return false;
    }
}

inline bool pattern_family_fallback_dispatch_wired(RequiredStrategy required, int level) {
    const PatternStrategyPolicy policy = pattern_strategy_policy(required, level);
    return pattern_dispatch_wired(required, level) &&
           pattern_policy_allows_family_fallback(policy.generator_policy);
}

inline const char* pattern_kind_label(PatternKind kind) {
    switch (kind) {
    case PatternKind::Chain: return "chain";
    case PatternKind::ExocetLike: return "exocet";
    case PatternKind::LoopLike: return "loop";
    case PatternKind::ForcingLike: return "forcing";
    case PatternKind::ColorLike: return "color";
    case PatternKind::PetalLike: return "petal";
    case PatternKind::IntersectionLike: return "intersection";
    case PatternKind::FishLike: return "fish";
    case PatternKind::FrankenLike: return "franken";
    case PatternKind::MutantLike: return "mutant";
    case PatternKind::SquirmLike: return "squirm";
    case PatternKind::AlsLike: return "als";
    case PatternKind::ExclusionLike: return "exclusion";
    case PatternKind::AicLike: return "aic";
    case PatternKind::GroupedAicLike: return "groupedaic";
    case PatternKind::GroupedCycleLike: return "groupedxcycle";
    case PatternKind::NiceLoopLike: return "continuousniceloop";
    case PatternKind::XChainLike: return "xchain";
    case PatternKind::XYChainLike: return "xychain";
    case PatternKind::EmptyRectangleLike: return "emptyrectangle";
    case PatternKind::RemotePairsLike: return "remotepairs";
    case PatternKind::SwordfishLike: return "swordfish";
    case PatternKind::JellyfishLike: return "jellyfish";
    case PatternKind::FinnedFishLike: return "finnedfish";
    case PatternKind::OverlayLike: return "overlay";
    case PatternKind::MedusaLike: return "medusa";
    case PatternKind::None:
    default:
        return "none";
    }
}

inline const char* pattern_mutation_source_label(PatternMutationSource source) {
    switch (source) {
    case PatternMutationSource::Last: return "last";
    case PatternMutationSource::Best: return "best";
    case PatternMutationSource::Random:
    default:
        return "random";
    }
}

struct PatternAttemptFeedbackStats {
    uint64_t attempts = 0;
    uint64_t exact_attempts = 0;
    uint64_t analyzed_sum = 0;
    uint64_t use_sum = 0;
    uint64_t hit_sum = 0;
    int best_template_score = -1;
    PatternKind best_kind = PatternKind::None;
};

inline PatternAttemptFeedbackStats& tls_pattern_attempt_feedback_stats() {
    thread_local PatternAttemptFeedbackStats s;
    return s;
}

inline void note_template_attempt_feedback(
    RequiredStrategy /*required*/,
    PatternKind kind,
    bool exact_template,
    int template_score,
    int analyzed,
    int use_count,
    int hit_count) {

    PatternAttemptFeedbackStats& s = tls_pattern_attempt_feedback_stats();
    ++s.attempts;
    if (exact_template) ++s.exact_attempts;
    s.analyzed_sum += static_cast<uint64_t>(analyzed > 0 ? analyzed : 0);
    s.use_sum += static_cast<uint64_t>(use_count > 0 ? use_count : 0);
    s.hit_sum += static_cast<uint64_t>(hit_count > 0 ? hit_count : 0);
    if (template_score > s.best_template_score) {
        s.best_template_score = template_score;
        s.best_kind = kind;
    }
}

} // namespace sudoku_hpc::pattern_forcing