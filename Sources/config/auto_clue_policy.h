//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// This header is intentionally lightweight and header-only.
// It is meant to be included by Sources/config/run_config.h
// after RequiredStrategy and ClueRange have already been declared.

enum class AutoClueWindowPolicy : uint8_t {
    Shared = 0,
    Generator = 1,
    Certifier = 2,
};

enum class StrategyClueFamily : uint8_t {
    Generic = 0,
    EarlySingles,
    Intersections,
    Subsets,
    WingsChainsAls,
    Fish,
    Uniqueness,
    PetalBottleneck,
    TheoreticalExact,
    HeavyFish,
};

struct GoldilocksClueWindow {
    double ratio_lo = 0.0;
    double ratio_hi = 0.0;
};

struct StrategyClueFamilyPolicy {
    StrategyClueFamily family = StrategyClueFamily::Generic;

    // Shift applied to the base Goldilocks window.
    double generator_lo_shift = 0.0;
    double generator_hi_shift = 0.0;
    double certifier_lo_shift = 0.0;
    double certifier_hi_shift = 0.0;

    // Extra width adjustments.
    double generator_width_delta = 0.0;
    double certifier_width_delta = 0.0;

    // Structural hints for callers / diagnostics.
    bool preserve_structure = false;
    bool exact_sensitive = false;
};

inline int auto_clue_scaled_count(int nn, double ratio) {
    if (nn <= 0) return 0;
    const double clamped = std::clamp(ratio, 0.0, 1.0);
    return static_cast<int>(std::lround(static_cast<double>(nn) * clamped));
}

inline bool auto_clue_geometry_is_primary_3x3(int box_rows, int box_cols) {
    return box_rows == 3 && box_cols == 3;
}

inline bool auto_clue_geometry_is_asymmetric(int box_rows, int box_cols) {
    return box_rows != box_cols;
}

inline bool auto_clue_geometry_is_large(int n) {
    return n >= 12;
}

inline int auto_clue_strategy_min_level(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::None:
        case RequiredStrategy::NakedSingle:
        case RequiredStrategy::HiddenSingle:
            return 1;

        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
        case RequiredStrategy::NakedPair:
        case RequiredStrategy::HiddenPair:
        case RequiredStrategy::NakedTriple:
        case RequiredStrategy::HiddenTriple:
            return 2;

        case RequiredStrategy::NakedQuad:
        case RequiredStrategy::HiddenQuad:
        case RequiredStrategy::XWing:
        case RequiredStrategy::YWing:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
            return 3;

        case RequiredStrategy::Swordfish:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::UniqueRectangle:
        case RequiredStrategy::BUGPlusOne:
        case RequiredStrategy::WWing:
        case RequiredStrategy::SimpleColoring:
            return 4;

        case RequiredStrategy::Jellyfish:
        case RequiredStrategy::WXYZWing:
        case RequiredStrategy::FinnedSwordfishJellyfish:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
        case RequiredStrategy::ALSXZ:
        case RequiredStrategy::UniqueLoop:
        case RequiredStrategy::AvoidableRectangle:
        case RequiredStrategy::BivalueOddagon:
        case RequiredStrategy::UniqueRectangleExtended:
        case RequiredStrategy::HiddenUniqueRectangle:
        case RequiredStrategy::BUGType2:
        case RequiredStrategy::BUGType3:
        case RequiredStrategy::BUGType4:
        case RequiredStrategy::BorescoperQiuDeadlyPattern:
            return 5;

        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::AlignedPairExclusion:
        case RequiredStrategy::AlignedTripleExclusion:
            return 6;

        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return 7;

        case RequiredStrategy::Backtracking:
            return 8;
    }
    return 1;
}

inline StrategyClueFamily auto_clue_strategy_family(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::NakedSingle:
        case RequiredStrategy::HiddenSingle:
            return StrategyClueFamily::EarlySingles;

        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
            return StrategyClueFamily::Intersections;

        case RequiredStrategy::NakedPair:
        case RequiredStrategy::HiddenPair:
        case RequiredStrategy::NakedTriple:
        case RequiredStrategy::HiddenTriple:
        case RequiredStrategy::NakedQuad:
        case RequiredStrategy::HiddenQuad:
            return StrategyClueFamily::Subsets;

        case RequiredStrategy::YWing:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::WWing:
        case RequiredStrategy::WXYZWing:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
        case RequiredStrategy::ALSXZ:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::AlignedPairExclusion:
        case RequiredStrategy::AlignedTripleExclusion:
            return StrategyClueFamily::WingsChainsAls;

        case RequiredStrategy::XWing:
        case RequiredStrategy::Swordfish:
        case RequiredStrategy::Jellyfish:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::FinnedSwordfishJellyfish:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
            return StrategyClueFamily::Fish;

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
            return StrategyClueFamily::Uniqueness;

        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
            return StrategyClueFamily::PetalBottleneck;

        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::MSLS:
            return StrategyClueFamily::HeavyFish;

        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return StrategyClueFamily::TheoreticalExact;

        default:
            return StrategyClueFamily::Generic;
    }
}

inline StrategyClueFamilyPolicy auto_clue_family_policy(RequiredStrategy required) {
    StrategyClueFamilyPolicy out{};
    out.family = auto_clue_strategy_family(required);

    switch (out.family) {
        case StrategyClueFamily::EarlySingles:
            out.generator_lo_shift = -0.030;
            out.generator_hi_shift = -0.015;
            out.certifier_lo_shift = -0.040;
            out.certifier_hi_shift = -0.020;
            break;

        case StrategyClueFamily::Intersections:
        case StrategyClueFamily::Subsets:
            out.generator_lo_shift = -0.010;
            out.generator_hi_shift = +0.010;
            out.certifier_lo_shift = -0.020;
            out.certifier_hi_shift = -0.005;
            break;

        case StrategyClueFamily::WingsChainsAls:
            out.generator_lo_shift = +0.015;
            out.generator_hi_shift = +0.030;
            out.certifier_lo_shift = -0.010;
            out.certifier_hi_shift = +0.005;
            out.generator_width_delta = +0.010;
            out.certifier_width_delta = -0.005;
            out.preserve_structure = true;
            out.exact_sensitive = true;
            break;

        case StrategyClueFamily::Fish:
            out.generator_lo_shift = +0.005;
            out.generator_hi_shift = +0.020;
            out.certifier_lo_shift = -0.005;
            out.certifier_hi_shift = +0.000;
            out.preserve_structure = true;
            break;

        case StrategyClueFamily::Uniqueness:
            out.generator_lo_shift = +0.010;
            out.generator_hi_shift = +0.025;
            out.certifier_lo_shift = +0.000;
            out.certifier_hi_shift = +0.010;
            out.preserve_structure = true;
            break;

        case StrategyClueFamily::PetalBottleneck:
            out.generator_lo_shift = -0.015;
            out.generator_hi_shift = +0.000;
            out.certifier_lo_shift = -0.030;
            out.certifier_hi_shift = -0.010;
            out.generator_width_delta = -0.005;
            out.certifier_width_delta = -0.010;
            break;

        case StrategyClueFamily::TheoreticalExact:
            out.generator_lo_shift = +0.025;
            out.generator_hi_shift = +0.045;
            out.certifier_lo_shift = -0.015;
            out.certifier_hi_shift = +0.005;
            out.generator_width_delta = +0.005;
            out.certifier_width_delta = -0.010;
            out.preserve_structure = true;
            out.exact_sensitive = true;
            break;

        case StrategyClueFamily::HeavyFish:
            out.generator_lo_shift = +0.030;
            out.generator_hi_shift = +0.050;
            out.certifier_lo_shift = +0.000;
            out.certifier_hi_shift = +0.015;
            out.generator_width_delta = +0.010;
            out.certifier_width_delta = -0.005;
            out.preserve_structure = true;
            out.exact_sensitive = true;
            break;

        case StrategyClueFamily::Generic:
        default:
            out.generator_lo_shift = +0.005;
            out.generator_hi_shift = +0.015;
            out.certifier_lo_shift = -0.010;
            out.certifier_hi_shift = -0.005;
            break;
    }

    return out;
}

inline GoldilocksClueWindow compute_goldilocks_base_window(
    int box_rows,
    int box_cols,
    int difficulty_level,
    RequiredStrategy required) {

    const int n = std::max(1, box_rows * box_cols);
    const int lvl = std::max(1, std::max(std::clamp(difficulty_level, 1, 8), auto_clue_strategy_min_level(required)));

    // Base Goldilocks zone:
    // - lower levels: denser puzzles
    // - higher levels: sparser puzzles
    // - higher geometry sizes: slightly denser again to preserve structure
    double ratio_hi = std::clamp(0.64 - 0.040 * static_cast<double>(lvl - 1), 0.18, 0.72);
    double ratio_lo = std::clamp(ratio_hi - 0.12, 0.10, ratio_hi);

    if (auto_clue_geometry_is_asymmetric(box_rows, box_cols)) {
        ratio_hi += 0.015;
        ratio_lo += 0.010;
    }

    if (auto_clue_geometry_is_large(n)) {
        ratio_hi += 0.020;
        ratio_lo += 0.015;
    }

    const StrategyClueFamily family = auto_clue_strategy_family(required);
    if (family == StrategyClueFamily::TheoreticalExact || family == StrategyClueFamily::HeavyFish) {
        ratio_hi += 0.020;
        ratio_lo += 0.015;
    } else if (family == StrategyClueFamily::PetalBottleneck) {
        ratio_hi -= 0.010;
        ratio_lo -= 0.010;
    }

    ratio_hi = std::clamp(ratio_hi, 0.16, 0.84);
    ratio_lo = std::clamp(ratio_lo, 0.08, ratio_hi);

    return GoldilocksClueWindow{ratio_lo, ratio_hi};
}

inline GoldilocksClueWindow adjust_goldilocks_for_generator(
    GoldilocksClueWindow base,
    int box_rows,
    int box_cols,
    int difficulty_level,
    RequiredStrategy required) {

    const int n = std::max(1, box_rows * box_cols);
    const int lvl = std::max(1, std::max(std::clamp(difficulty_level, 1, 8), auto_clue_strategy_min_level(required)));
    const StrategyClueFamilyPolicy policy = auto_clue_family_policy(required);

    double lo = base.ratio_lo + policy.generator_lo_shift;
    double hi = base.ratio_hi + policy.generator_hi_shift;

    const double width = std::max(0.05, (hi - lo) + policy.generator_width_delta);
    hi = std::max(hi, lo + width);

    // Harder requested levels usually need a little more structural density in generation.
    if (lvl >= 6) {
        lo += 0.005;
        hi += 0.010;
    }

    if (auto_clue_geometry_is_primary_3x3(box_rows, box_cols) && policy.exact_sensitive) {
        // 3x3 exact patterns are brittle: bias a touch denser in generator.
        lo += 0.010;
        hi += 0.015;
    }

    if (n >= 16 && policy.preserve_structure) {
        lo += 0.010;
        hi += 0.015;
    }

    lo = std::clamp(lo, 0.08, 0.88);
    hi = std::clamp(hi, lo, 0.90);
    return GoldilocksClueWindow{lo, hi};
}

inline GoldilocksClueWindow adjust_goldilocks_for_certifier(
    GoldilocksClueWindow base,
    int box_rows,
    int box_cols,
    int difficulty_level,
    RequiredStrategy required) {

    const int n = std::max(1, box_rows * box_cols);
    const int lvl = std::max(1, std::max(std::clamp(difficulty_level, 1, 8), auto_clue_strategy_min_level(required)));
    const StrategyClueFamilyPolicy policy = auto_clue_family_policy(required);

    double lo = base.ratio_lo + policy.certifier_lo_shift;
    double hi = base.ratio_hi + policy.certifier_hi_shift;

    const double width = std::max(0.05, (hi - lo) + policy.certifier_width_delta);
    hi = std::max(hi, lo + width);

    // Certifier should slightly prefer sparser states when exact-sensitive,
    // to reduce overshadowing by easy singles and generic proxies.
    if (policy.exact_sensitive) {
        lo -= 0.005;
        hi -= 0.005;
    }

    if (lvl >= 7 && auto_clue_geometry_is_primary_3x3(box_rows, box_cols)) {
        lo -= 0.005;
        hi -= 0.010;
    }

    const StrategyClueFamily family = auto_clue_strategy_family(required);
    if (family == StrategyClueFamily::PetalBottleneck) {
        lo -= 0.005;
        hi -= 0.005;
    }

    if (n >= 16 && policy.preserve_structure) {
        // Do not over-thin large exact boards.
        hi += 0.005;
    }

    lo = std::clamp(lo, 0.06, 0.84);
    hi = std::clamp(hi, lo, 0.88);
    return GoldilocksClueWindow{lo, hi};
}

inline ClueRange clue_range_from_goldilocks(int nn, GoldilocksClueWindow window) {
    ClueRange out{};
    out.min_clues = auto_clue_scaled_count(nn, window.ratio_lo);
    out.max_clues = auto_clue_scaled_count(nn, window.ratio_hi);
    if (out.max_clues < out.min_clues) out.max_clues = out.min_clues;
    out.min_clues = std::clamp(out.min_clues, 0, nn);
    out.max_clues = std::clamp(out.max_clues, out.min_clues, nn);
    return out;
}

inline ClueRange resolve_auto_clue_range_goldilocks(
    int box_rows,
    int box_cols,
    int difficulty_level,
    RequiredStrategy required,
    AutoClueWindowPolicy policy = AutoClueWindowPolicy::Shared) {

    const int n = std::max(1, box_rows * box_cols);
    const int nn = n * n;

    const GoldilocksClueWindow base =
        compute_goldilocks_base_window(box_rows, box_cols, difficulty_level, required);

    GoldilocksClueWindow adjusted = base;
    switch (policy) {
        case AutoClueWindowPolicy::Generator:
            adjusted = adjust_goldilocks_for_generator(base, box_rows, box_cols, difficulty_level, required);
            break;
        case AutoClueWindowPolicy::Certifier:
            adjusted = adjust_goldilocks_for_certifier(base, box_rows, box_cols, difficulty_level, required);
            break;
        case AutoClueWindowPolicy::Shared:
        default:
            break;
    }

    ClueRange out = clue_range_from_goldilocks(nn, adjusted);

    // Family-specific hard floors / caps after ratio conversion.
    const StrategyClueFamily family = auto_clue_strategy_family(required);
    if (family == StrategyClueFamily::HeavyFish) {
        const int min_floor = std::max(n, static_cast<int>(std::lround(0.28 * static_cast<double>(nn))));
        out.min_clues = std::max(out.min_clues, min_floor);
        out.max_clues = std::max(out.max_clues, out.min_clues);
    } else if (family == StrategyClueFamily::TheoreticalExact) {
        const int min_floor = std::max(n, static_cast<int>(std::lround(0.24 * static_cast<double>(nn))));
        out.min_clues = std::max(out.min_clues, min_floor);
        out.max_clues = std::max(out.max_clues, out.min_clues);
    } else if (family == StrategyClueFamily::PetalBottleneck) {
        const int max_cap = std::max(out.min_clues, static_cast<int>(std::lround(0.62 * static_cast<double>(nn))));
        out.max_clues = std::min(out.max_clues, max_cap);
        out.max_clues = std::max(out.max_clues, out.min_clues);
    }

    // Keep trivial/Bruteforce ends sane.
    if (required == RequiredStrategy::Backtracking) {
        out.min_clues = std::max(out.min_clues, std::max(4, n));
        out.max_clues = std::max(out.max_clues, out.min_clues);
    }

    return out;
}

