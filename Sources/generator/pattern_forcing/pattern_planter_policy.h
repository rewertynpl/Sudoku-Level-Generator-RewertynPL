//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <array>
#include <cstdint>

#include "pattern_planter_types.h"

namespace sudoku_hpc::pattern_forcing {

struct PatternStrategyPolicy {
    PatternKind kind = PatternKind::None;
    PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;
    bool prefer_exact = false;
    bool preserve_anchors = false;
    bool suppress_equivalent_generics = false;
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


inline PatternKind pick_kind(RequiredStrategy required, int /*level*/) {
    switch (required) {
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return PatternKind::ExocetLike;
    case RequiredStrategy::SKLoop:
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
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
        return PatternKind::FishLike;
    case RequiredStrategy::FrankenFish:
        return PatternKind::FrankenLike;
    case RequiredStrategy::MutantFish:
        return PatternKind::MutantLike;
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::KrakenFish:
        return PatternKind::SquirmLike;
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
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
    PatternStrategyPolicy out{};
    out.kind = pick_kind(required, level);
    out.prefer_exact = level >= 6;
    out.preserve_anchors = level >= 5;
    out.suppress_equivalent_generics = level >= 6;

    switch (required) {
    case RequiredStrategy::None:
    case RequiredStrategy::Backtracking:
        out.generator_policy = PatternGeneratorPolicy::Unsupported;
        break;
    case RequiredStrategy::PointingPairs:
    case RequiredStrategy::BoxLineReduction:
        out.generator_policy = PatternGeneratorPolicy::FamilyOnly;
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
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
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
        out.generator_policy = PatternGeneratorPolicy::ExactPreferredFamilyFallback;
        break;
    default:
        out.generator_policy = (out.kind == PatternKind::None)
            ? PatternGeneratorPolicy::Unsupported
            : PatternGeneratorPolicy::FamilyOnly;
        break;
    }

    return out;
}

inline bool pattern_dispatch_wired(RequiredStrategy required, int level) {
    return pick_kind(required, level) != PatternKind::None;
}

inline bool pattern_exact_template_dispatch_wired(RequiredStrategy required, int /*level*/) {
    switch (required) {
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
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
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
        return true;
    case RequiredStrategy::PointingPairs:
    case RequiredStrategy::BoxLineReduction:
        return false;
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
    s.analyzed_sum += static_cast<uint64_t>(std::max(analyzed, 0));
    s.use_sum += static_cast<uint64_t>(std::max(use_count, 0));
    s.hit_sum += static_cast<uint64_t>(std::max(hit_count, 0));
    if (template_score > s.best_template_score) {
        s.best_template_score = template_score;
        s.best_kind = kind;
    }
}

} // namespace sudoku_hpc::pattern_forcing
