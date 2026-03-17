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

inline bool auto_clue_geometry_is_asymmetric(int box_rows, int box_cols) {
    return box_rows != box_cols;
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
            return StrategyClueFamily::HeavyFish;

        case RequiredStrategy::MSLS:
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

struct HardSplitDef {
    int gen_min;
    int gen_max;
    int cert_min;
    int cert_max;
};

// Rygorystyczne widełki zagęszczenia na bazie klasycznego 9x9 (N=9)
inline HardSplitDef get_n9_hard_split(StrategyClueFamily family) {
    switch (family) {
        case StrategyClueFamily::EarlySingles:     return {30, 36, 28, 34};
        case StrategyClueFamily::Intersections:    return {36, 42, 25, 29};
        case StrategyClueFamily::Subsets:          return {38, 44, 24, 28};
        case StrategyClueFamily::WingsChainsAls:   return {45, 52, 22, 26};
        case StrategyClueFamily::Fish:             return {42, 48, 23, 27};
        case StrategyClueFamily::Uniqueness:       return {48, 56, 25, 28};
        case StrategyClueFamily::PetalBottleneck:  return {45, 52, 22, 25};
        case StrategyClueFamily::TheoreticalExact: return {50, 62, 22, 25};
        case StrategyClueFamily::HeavyFish:        return {50, 62, 22, 25}; // Fallback ochronny
        case StrategyClueFamily::Generic: default: return {38, 44, 24, 28};
    }
}

// Rygorystyczne widełki zagęszczenia na bazie planszy 12x12 (N=12) - asymetrycznej 4x3
inline HardSplitDef get_n12_hard_split(StrategyClueFamily family) {
    switch (family) {
        case StrategyClueFamily::EarlySingles:     return {55, 65, 50, 60};
        case StrategyClueFamily::Intersections:    return {65, 75, 45, 55};
        case StrategyClueFamily::Subsets:          return {70, 80, 42, 50};
        case StrategyClueFamily::WingsChainsAls:   return {85, 95, 38, 46};
        case StrategyClueFamily::Fish:             return {80, 90, 40, 48};
        case StrategyClueFamily::Uniqueness:       return {90, 105, 42, 48};
        case StrategyClueFamily::PetalBottleneck:  return {85, 95, 35, 45};
        case StrategyClueFamily::TheoreticalExact: return {95, 110, 35, 42};
        case StrategyClueFamily::HeavyFish:        return {100, 115, 40, 46};
        case StrategyClueFamily::Generic: default: return {70, 80, 42, 50};
    }
}

inline ClueRange resolve_auto_clue_range_goldilocks(
    int box_rows,
    int box_cols,
    int difficulty_level,
    RequiredStrategy required,
    AutoClueWindowPolicy policy = AutoClueWindowPolicy::Shared) {

    const int n = std::max(1, box_rows * box_cols);
    const int nn = n * n;
    
    // Twardy fallback dla trybów bez strategii strukturalnej (lub Bruteforce)
    if (required == RequiredStrategy::Backtracking || required == RequiredStrategy::None) {
        if (required == RequiredStrategy::Backtracking) {
            return {std::max(4, n), nn};
        }
        int base_min = static_cast<int>(std::lround(0.28 * nn));
        int base_max = static_cast<int>(std::lround(0.40 * nn));
        return {std::clamp(base_min, 0, nn), std::clamp(base_max, 0, nn)};
    }

    const bool is_asym = auto_clue_geometry_is_asymmetric(box_rows, box_cols);
    // Shared zbiega w stronę Certyfikatora (najniższego mianownika gwarantującego unikalność po usunięciu rusztowania)
    const bool use_gen_bounds = (policy == AutoClueWindowPolicy::Generator);
    
    StrategyClueFamily family = auto_clue_strategy_family(required);
    
    HardSplitDef n9 = get_n9_hard_split(family);
    HardSplitDef n12 = get_n12_hard_split(family);
    
    double ratio_min, ratio_max;
    
    if (n == 9) {
        ratio_min = static_cast<double>(use_gen_bounds ? n9.gen_min : n9.cert_min) / 81.0;
        ratio_max = static_cast<double>(use_gen_bounds ? n9.gen_max : n9.cert_max) / 81.0;
    } else if (n == 12) {
        ratio_min = static_cast<double>(use_gen_bounds ? n12.gen_min : n12.cert_min) / 144.0;
        ratio_max = static_cast<double>(use_gen_bounds ? n12.gen_max : n12.cert_max) / 144.0;
    } else if (n < 9) {
        ratio_min = static_cast<double>(use_gen_bounds ? n9.gen_min : n9.cert_min) / 81.0;
        ratio_max = static_cast<double>(use_gen_bounds ? n9.gen_max : n9.cert_max) / 81.0;
    } else if (n > 12) {
        ratio_min = static_cast<double>(use_gen_bounds ? n12.gen_min : n12.cert_min) / 144.0;
        ratio_max = static_cast<double>(use_gen_bounds ? n12.gen_max : n12.cert_max) / 144.0;
    } else {
        // Skalowanie proporcjonalne (interpolacja) dla wymiarów przejściowych (np. N=10)
        double t = (static_cast<double>(n) - 9.0) / 3.0;
        double r9_min = static_cast<double>(use_gen_bounds ? n9.gen_min : n9.cert_min) / 81.0;
        double r9_max = static_cast<double>(use_gen_bounds ? n9.gen_max : n9.cert_max) / 81.0;
        double r12_min = static_cast<double>(use_gen_bounds ? n12.gen_min : n12.cert_min) / 144.0;
        double r12_max = static_cast<double>(use_gen_bounds ? n12.gen_max : n12.cert_max) / 144.0;
        ratio_min = r9_min + t * (r12_min - r9_min);
        ratio_max = r9_max + t * (r12_max - r9_max);
    }
    
    // Korekta geometryczna dla asymetrii (N=12 już ma ją wbitą bazowo z samej tabeli)
    if (is_asym && n != 12) {
        double asym_shift = use_gen_bounds ? 0.015 : 0.010;
        ratio_min += asym_shift;
        ratio_max += asym_shift;
    }
    
    int out_min = static_cast<int>(std::lround(ratio_min * nn));
    int out_max = static_cast<int>(std::lround(ratio_max * nn));
    
    // Absolutne wymuszanie sztywnych krawędzi tabeli dla referencyjnych wymiarów (unikamy błędów float lround)
    if (n == 9 && !is_asym) {
        out_min = use_gen_bounds ? n9.gen_min : n9.cert_min;
        out_max = use_gen_bounds ? n9.gen_max : n9.cert_max;
    } else if (n == 12) {
        out_min = use_gen_bounds ? n12.gen_min : n12.cert_min;
        out_max = use_gen_bounds ? n12.gen_max : n12.cert_max;
    }
    
    out_min = std::clamp(out_min, 0, nn);
    out_max = std::clamp(out_max, out_min, nn);
    
    return {out_min, out_max};
}