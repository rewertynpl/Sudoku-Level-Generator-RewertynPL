//Author copyright Marcin Matysek (Rewertyn)
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

namespace sudoku_hpc {

enum class RequiredStrategy : int {
    None = 0,
    NakedSingle,
    HiddenSingle,
    PointingPairs,
    BoxLineReduction,
    NakedPair,
    HiddenPair,
    NakedTriple,
    HiddenTriple,
    NakedQuad,
    HiddenQuad,
    XWing,
    YWing,
    Skyscraper,
    TwoStringKite,
    EmptyRectangle,
    RemotePairs,
    Swordfish,
    FinnedXWingSashimi,
    SimpleColoring,
    BUGPlusOne,
    UniqueRectangle,
    XYZWing,
    WWing,
    Jellyfish,
    XChain,
    XYChain,
    WXYZWing,
    FinnedSwordfishJellyfish,
    ALSXZ,
    UniqueLoop,
    AvoidableRectangle,
    BivalueOddagon,
    UniqueRectangleExtended,
    HiddenUniqueRectangle,
    BUGType2,
    BUGType3,
    BUGType4,
    BorescoperQiuDeadlyPattern,
    Medusa3D,
    AIC,
    GroupedAIC,
    GroupedXCycle,
    ContinuousNiceLoop,
    ALSXYWing,
    ALSChain,
    AlignedPairExclusion,
    AlignedTripleExclusion,
    ALSAIC,
    SueDeCoq,
    DeathBlossom,
    FrankenFish,
    MutantFish,
    KrakenFish,
    Squirmbag,
    MSLS,
    Exocet,
    SeniorExocet,
    SKLoop,
    PatternOverlayMethod,
    ForcingChains,
    DynamicForcingChains,
    Backtracking,
};

enum class RejectReason : uint8_t {
    None = 0,
    Prefilter,
    Logic,
    Uniqueness,
    Strategy,
    ExactNoProgress,
    Replay,
    DistributionBias,
    UniquenessBudget,
};

enum class StrategyCoverageGrade : uint8_t {
    TextbookFull = 0,
    FamilyApprox = 1,
    Partial = 2,
    WiredOnly = 3,
    Untested = 4,
};

enum class PatternGeneratorPolicy : uint8_t {
    ExactRequired = 0,
    ExactPreferredFamilyFallback = 1,
    FamilyOnly = 2,
    Unsupported = 3,
};

enum class StrategyZeroAllocGrade : uint8_t {
    HotpathZeroAllocOk = 0,
    TlsAllocOnly = 1,
    VectorInHotpath = 2,
    NeedsScratchpadRefactor = 3,
};

enum class StrategyAuditDecision : uint8_t {
    Keep = 0,
    Tighten = 1,
    Rewrite = 2,
    ExactTemplateMissing = 3,
};

struct RequiredStrategyAttemptInfo {
    bool analyzed_required_strategy = false;
    bool required_slot_entered = false;
    bool required_strategy_use_confirmed = false;
    bool required_strategy_hit_confirmed = false;
    bool matched_required_strategy = false;
    bool required_strategy_certified_exact = false;
    bool required_strategy_exact_contract_met = false;
    bool exact_no_progress = false;
    bool family_fallback_used = false;
};

struct ClueRange {
    int min_clues = 0;
    int max_clues = 0;
};

#include "auto_clue_policy.h"

struct GenerateRunConfig {
    int box_rows = 3;
    int box_cols = 3;

    uint64_t target_puzzles = 100;
    int min_clues = 0;
    int max_clues = 0;

    int difficulty_level_required = 1;
    RequiredStrategy required_strategy = RequiredStrategy::None;

    int threads = 0;
    uint64_t seed = 0;

    int reseed_interval_s = 0;
    bool force_new_seed_per_attempt = true;

    double attempt_time_budget_s = 0.0;
    uint64_t attempt_node_budget = 0;
    uint64_t max_attempts = 0;
    uint64_t max_total_time_s = 0;

    bool symmetry_center = false;
    bool require_unique = true;
    bool write_individual_files = true;
    bool pause_on_exit_windows = false;

    std::string output_folder = "generated_sudoku_files";
    std::string output_file = "generated_sudoku.txt";

    bool pattern_forcing_enabled = false;
    int pattern_forcing_tries = 6;
    int pattern_forcing_anchor_count = 0;
    bool pattern_forcing_lock_anchors = true;

    bool mcts_digger_enabled = true;
    std::string mcts_tuning_profile = "auto";
    int mcts_digger_iterations = 0;
    double mcts_ucb_c = 1.41;
    int mcts_fail_cap = 0;
    int mcts_basic_logic_level = 5;
    int max_pattern_depth = 0;

    bool strict_logical = false;
    bool strict_canonical_strategies = false;
    bool allow_proxy_advanced = true;
    bool enable_quality_contract = true;
    bool enable_distribution_filter = false;
    bool enable_replay_validation = false;

    std::string vip_grade_target = "gold";
    std::string vip_min_grade_by_geometry_path;
    std::string vip_score_profile = "standard";

    std::string cpu_backend = "scalar";

    bool stage_start = false;
    bool stage_end = false;
    bool perf_ab_suite = false;
    bool fast_test_mode = false;
    std::string benchmark_output_file = "benchmark_report.txt";

    bool split_auto_clue_policy = true;
    bool auto_clue_family_overrides = true;
    bool print_clue_policy_debug = false;
    double goldilocks_generator_bias = 0.0;
    double goldilocks_certifier_bias = 0.0;

    // Contract-stability controls for required named strategies (P1-P5 first).
    bool dual_clue_policy_enforced = true;
    bool prefer_required_named_first = true;
    bool suppress_equivalent_generics_for_required = true;
    bool preserve_required_pattern_anchors = true;
    bool certifier_sparsify_for_required_named = true;
    bool generator_dense_seed_for_required_named = true;
    bool protect_required_pattern_from_singles = true;
};

struct GenerateRunResult {
    uint64_t accepted = 0;
    uint64_t written = 0;
    uint64_t attempts = 0;
    uint64_t rejected = 0;
    std::string measurement_profile = "strict-contract";
    int effective_min_clues = 0;
    int effective_max_clues = 0;
    int effective_shared_min_clues = 0;
    int effective_shared_max_clues = 0;
    int effective_generator_min_clues = 0;
    int effective_generator_max_clues = 0;
    int effective_certifier_min_clues = 0;
    int effective_certifier_max_clues = 0;

    uint64_t reject_prefilter = 0;
    uint64_t reject_logic = 0;
    uint64_t reject_uniqueness = 0;
    uint64_t reject_strategy = 0;
    uint64_t reject_exact_no_progress = 0;
    uint64_t reject_replay = 0;
    uint64_t reject_distribution_bias = 0;
    uint64_t reject_uniqueness_budget = 0;

    uint64_t uniqueness_calls = 0;
    uint64_t uniqueness_nodes = 0;
    double uniqueness_elapsed_ms = 0.0;
    double uniqueness_avg_ms = 0.0;

    std::string cpu_backend_selected = "scalar";
    double kernel_time_ms = 0.0;
    uint64_t kernel_calls = 0;
    double backend_efficiency_score = 0.0;
    double asymmetry_efficiency_index = 0.0;

    uint64_t logic_steps_total = 0;
    uint64_t strategy_naked_use = 0;
    uint64_t strategy_naked_hit = 0;
    uint64_t strategy_hidden_use = 0;
    uint64_t strategy_hidden_hit = 0;
    uint64_t mcts_advanced_evals = 0;
    uint64_t certifier_required_strategy_analyzed = 0;
    uint64_t certifier_required_slot_entered = 0;
    uint64_t certifier_required_strategy_use = 0;
    uint64_t certifier_required_strategy_hit = 0;
    uint64_t required_strategy_certified_exact = 0;
    uint64_t mcts_required_strategy_analyzed = 0;
    uint64_t mcts_required_strategy_use = 0;
    uint64_t mcts_required_strategy_hit = 0;
    uint64_t pattern_exact_template_used = 0;
    uint64_t pattern_family_fallback_used = 0;
    uint64_t required_strategy_exact_contract_met = 0;

    double vip_score = 0.0;
    std::string vip_grade = "none";
    bool vip_contract_ok = false;
    std::string vip_contract_fail_reason;

    std::string premium_signature;
    std::string premium_signature_v2;

    double elapsed_s = 0.0;
    double accepted_per_sec = 0.0;
};

enum class StrategySmokeVariant : uint8_t {
    Primary = 0,
    Asymmetric = 1,
};

struct StrategySmokeProfile {
    bool enabled = false;
    StrategySmokeVariant variant = StrategySmokeVariant::Primary;
    const char* variant_label = "primary";
    int box_rows = 0;
    int box_cols = 0;
    int difficulty = 0;
    RequiredStrategy required_strategy = RequiredStrategy::None;
    uint64_t seed = 0;
    bool pattern_forcing = false;
    const char* mcts_profile = "auto";
    bool strict_canonical = true;
    bool allow_proxy_advanced = false;
    bool fast_test = true;
    uint64_t max_total_time_s = 20;
    uint64_t max_attempts = 0;
    uint64_t min_required_use = 1;
    uint64_t min_required_hit = 1;
    bool exact_contract_required = true;
};

inline std::string normalize_token(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (const unsigned char ch : in) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

inline const char* to_string(StrategyCoverageGrade grade) {
    switch (grade) {
        case StrategyCoverageGrade::TextbookFull: return "textbook_full";
        case StrategyCoverageGrade::FamilyApprox: return "family_approx";
        case StrategyCoverageGrade::Partial: return "partial";
        case StrategyCoverageGrade::WiredOnly: return "wired_only";
        case StrategyCoverageGrade::Untested: return "untested";
    }
    return "untested";
}

inline const char* to_string(PatternGeneratorPolicy policy) {
    switch (policy) {
        case PatternGeneratorPolicy::ExactRequired: return "exact_required";
        case PatternGeneratorPolicy::ExactPreferredFamilyFallback: return "exact_preferred_fallback_family";
        case PatternGeneratorPolicy::FamilyOnly: return "family_only";
        case PatternGeneratorPolicy::Unsupported: return "unsupported";
    }
    return "unsupported";
}

inline const char* to_string(StrategyZeroAllocGrade grade) {
    switch (grade) {
        case StrategyZeroAllocGrade::HotpathZeroAllocOk: return "hotpath_zero_alloc_ok";
        case StrategyZeroAllocGrade::TlsAllocOnly: return "tls_alloc_only";
        case StrategyZeroAllocGrade::VectorInHotpath: return "vector_in_hotpath";
        case StrategyZeroAllocGrade::NeedsScratchpadRefactor: return "needs_scratchpad_refactor";
    }
    return "needs_scratchpad_refactor";
}

inline const char* to_string(StrategyAuditDecision decision) {
    switch (decision) {
        case StrategyAuditDecision::Keep: return "keep";
        case StrategyAuditDecision::Tighten: return "tighten";
        case StrategyAuditDecision::Rewrite: return "rewrite";
        case StrategyAuditDecision::ExactTemplateMissing: return "exact-template-missing";
    }
    return "rewrite";
}

inline std::string to_string(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::None: return "none";
        case RequiredStrategy::NakedSingle: return "nakedsingle";
        case RequiredStrategy::HiddenSingle: return "hiddensingle";
        case RequiredStrategy::PointingPairs: return "pointingpairs";
        case RequiredStrategy::BoxLineReduction: return "boxlinereduction";
        case RequiredStrategy::NakedPair: return "nakedpair";
        case RequiredStrategy::HiddenPair: return "hiddenpair";
        case RequiredStrategy::NakedTriple: return "nakedtriple";
        case RequiredStrategy::HiddenTriple: return "hiddentriple";
        case RequiredStrategy::NakedQuad: return "nakedquad";
        case RequiredStrategy::HiddenQuad: return "hiddenquad";
        case RequiredStrategy::XWing: return "xwing";
        case RequiredStrategy::YWing: return "ywing";
        case RequiredStrategy::Skyscraper: return "skyscraper";
        case RequiredStrategy::TwoStringKite: return "twostringkite";
        case RequiredStrategy::EmptyRectangle: return "emptyrectangle";
        case RequiredStrategy::RemotePairs: return "remotepairs";
        case RequiredStrategy::Swordfish: return "swordfish";
        case RequiredStrategy::FinnedXWingSashimi: return "finnedxwingsashimi";
        case RequiredStrategy::SimpleColoring: return "simplecoloring";
        case RequiredStrategy::BUGPlusOne: return "bugplusone";
        case RequiredStrategy::UniqueRectangle: return "uniquerectangle";
        case RequiredStrategy::XYZWing: return "xyzwing";
        case RequiredStrategy::WWing: return "wwing";
        case RequiredStrategy::Jellyfish: return "jellyfish";
        case RequiredStrategy::XChain: return "xchain";
        case RequiredStrategy::XYChain: return "xychain";
        case RequiredStrategy::WXYZWing: return "wxyzwing";
        case RequiredStrategy::FinnedSwordfishJellyfish: return "finnedswordfishjellyfish";
        case RequiredStrategy::ALSXZ: return "alsxz";
        case RequiredStrategy::UniqueLoop: return "uniqueloop";
        case RequiredStrategy::AvoidableRectangle: return "avoidablerectangle";
        case RequiredStrategy::BivalueOddagon: return "bivalueoddagon";
        case RequiredStrategy::UniqueRectangleExtended: return "uniquerectangleextended";
        case RequiredStrategy::HiddenUniqueRectangle: return "hiddenuniquerectangle";
        case RequiredStrategy::BUGType2: return "bugtype2";
        case RequiredStrategy::BUGType3: return "bugtype3";
        case RequiredStrategy::BUGType4: return "bugtype4";
        case RequiredStrategy::BorescoperQiuDeadlyPattern: return "borescoperqiudeadlypattern";
        case RequiredStrategy::Medusa3D: return "medusa3d";
        case RequiredStrategy::AIC: return "aic";
        case RequiredStrategy::GroupedAIC: return "groupedaic";
        case RequiredStrategy::GroupedXCycle: return "groupedxcycle";
        case RequiredStrategy::ContinuousNiceLoop: return "continuousniceloop";
        case RequiredStrategy::ALSXYWing: return "alsxywing";
        case RequiredStrategy::ALSChain: return "alschain";
        case RequiredStrategy::AlignedPairExclusion: return "alignedpairexclusion";
        case RequiredStrategy::AlignedTripleExclusion: return "alignedtripleexclusion";
        case RequiredStrategy::ALSAIC: return "alsaic";
        case RequiredStrategy::SueDeCoq: return "suedecoq";
        case RequiredStrategy::DeathBlossom: return "deathblossom";
        case RequiredStrategy::FrankenFish: return "frankenfish";
        case RequiredStrategy::MutantFish: return "mutantfish";
        case RequiredStrategy::KrakenFish: return "krakenfish";
        case RequiredStrategy::Squirmbag: return "squirmbag";
        case RequiredStrategy::MSLS: return "msls";
        case RequiredStrategy::Exocet: return "exocet";
        case RequiredStrategy::SeniorExocet: return "seniorexocet";
        case RequiredStrategy::SKLoop: return "skloop";
        case RequiredStrategy::PatternOverlayMethod: return "patternoverlaymethod";
        case RequiredStrategy::ForcingChains: return "forcingchains";
        case RequiredStrategy::DynamicForcingChains: return "dynamicforcingchains";
        case RequiredStrategy::Backtracking: return "backtracking";
    }
    return "none";
}

inline bool parse_required_strategy(std::string_view raw, RequiredStrategy& out) {
    const std::string key = normalize_token(raw);
    if (key.empty() || key == "none") { out = RequiredStrategy::None; return true; }

    static const std::array<std::pair<std::string_view, RequiredStrategy>, 73> map = {{
        {"nakedsingle", RequiredStrategy::NakedSingle},
        {"hiddensingle", RequiredStrategy::HiddenSingle},
        {"pointingpairs", RequiredStrategy::PointingPairs},
        {"pointingpair", RequiredStrategy::PointingPairs},
        {"pointing", RequiredStrategy::PointingPairs},
        {"boxlinereduction", RequiredStrategy::BoxLineReduction},
        {"boxline", RequiredStrategy::BoxLineReduction},
        {"linereduction", RequiredStrategy::BoxLineReduction},
        {"nakedpair", RequiredStrategy::NakedPair},
        {"hiddenpair", RequiredStrategy::HiddenPair},
        {"nakedtriple", RequiredStrategy::NakedTriple},
        {"hiddentriple", RequiredStrategy::HiddenTriple},
        {"nakedquad", RequiredStrategy::NakedQuad},
        {"hiddenquad", RequiredStrategy::HiddenQuad},
        {"xwing", RequiredStrategy::XWing},
        {"ywing", RequiredStrategy::YWing},
        {"xywing", RequiredStrategy::YWing},
        {"skyscraper", RequiredStrategy::Skyscraper},
        {"twostringkite", RequiredStrategy::TwoStringKite},
        {"emptyrectangle", RequiredStrategy::EmptyRectangle},
        {"remotepairs", RequiredStrategy::RemotePairs},
        {"swordfish", RequiredStrategy::Swordfish},
        {"finnedxwingsashimi", RequiredStrategy::FinnedXWingSashimi},
        {"finnedxwing", RequiredStrategy::FinnedXWingSashimi},
        {"sashimixwing", RequiredStrategy::FinnedXWingSashimi},
        {"simplecoloring", RequiredStrategy::SimpleColoring},
        {"bugplusone", RequiredStrategy::BUGPlusOne},
        {"uniquerectangle", RequiredStrategy::UniqueRectangle},
        {"urtype1", RequiredStrategy::UniqueRectangle},
        {"xyzwing", RequiredStrategy::XYZWing},
        {"wwing", RequiredStrategy::WWing},
        {"jellyfish", RequiredStrategy::Jellyfish},
        {"xchain", RequiredStrategy::XChain},
        {"xychain", RequiredStrategy::XYChain},
        {"wxyzwing", RequiredStrategy::WXYZWing},
        {"alsxzwing", RequiredStrategy::WXYZWing},
        {"finnedswordfishjellyfish", RequiredStrategy::FinnedSwordfishJellyfish},
        {"alsxz", RequiredStrategy::ALSXZ},
        {"uniqueloop", RequiredStrategy::UniqueLoop},
        {"avoidablerectangle", RequiredStrategy::AvoidableRectangle},
        {"bivalueoddagon", RequiredStrategy::BivalueOddagon},
        {"uniquerectangleextended", RequiredStrategy::UniqueRectangleExtended},
        {"hiddenuniquerectangle", RequiredStrategy::HiddenUniqueRectangle},
        {"bugtype2", RequiredStrategy::BUGType2},
        {"bugtype3", RequiredStrategy::BUGType3},
        {"bugtype4", RequiredStrategy::BUGType4},
        {"borescoperqiudeadlypattern", RequiredStrategy::BorescoperQiuDeadlyPattern},
        {"qiu", RequiredStrategy::BorescoperQiuDeadlyPattern},
        {"medusa3d", RequiredStrategy::Medusa3D},
        {"aic", RequiredStrategy::AIC},
        {"groupedaic", RequiredStrategy::GroupedAIC},
        {"groupedxcycle", RequiredStrategy::GroupedXCycle},
        {"continuousniceloop", RequiredStrategy::ContinuousNiceLoop},
        {"alsxywing", RequiredStrategy::ALSXYWing},
        {"alschain", RequiredStrategy::ALSChain},
        {"alignedpairexclusion", RequiredStrategy::AlignedPairExclusion},
        {"alignedtripleexclusion", RequiredStrategy::AlignedTripleExclusion},
        {"alsaic", RequiredStrategy::ALSAIC},
        {"suedecoq", RequiredStrategy::SueDeCoq},
        {"deathblossom", RequiredStrategy::DeathBlossom},
        {"frankenfish", RequiredStrategy::FrankenFish},
        {"mutantfish", RequiredStrategy::MutantFish},
        {"krakenfish", RequiredStrategy::KrakenFish},
        {"squirmbag", RequiredStrategy::Squirmbag},
        {"msls", RequiredStrategy::MSLS},
        {"exocet", RequiredStrategy::Exocet},
        {"seniorexocet", RequiredStrategy::SeniorExocet},
        {"skloop", RequiredStrategy::SKLoop},
        {"patternoverlaymethod", RequiredStrategy::PatternOverlayMethod},
        {"forcingchains", RequiredStrategy::ForcingChains},
        {"dynamicforcingchains", RequiredStrategy::DynamicForcingChains},
        {"backtracking", RequiredStrategy::Backtracking},
        {"bruteforce", RequiredStrategy::Backtracking},
    }};

    for (const auto& [name, value] : map) {
        if (key == name) {
            out = value;
            return true;
        }
    }
    return false;
}

inline bool is_geometry_size_supported(int box_rows, int box_cols) {
    if (box_rows <= 0 || box_cols <= 0) {
        return false;
    }
    const int n = box_rows * box_cols;
    return n >= 4 && n <= 64;
}

inline int strategy_min_level(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::None:
            return 1;

        // P1 - Easy
        case RequiredStrategy::NakedSingle:
        case RequiredStrategy::HiddenSingle:
        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
            return 1;

        // P2 - Medium
        case RequiredStrategy::NakedPair:
        case RequiredStrategy::HiddenPair:
        case RequiredStrategy::NakedTriple:
        case RequiredStrategy::HiddenTriple:
            return 2;

        // P3 - Hard
        case RequiredStrategy::NakedQuad:
        case RequiredStrategy::HiddenQuad:
        case RequiredStrategy::XWing:
        case RequiredStrategy::YWing:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
            return 3;

        // P4 - Expert
        case RequiredStrategy::Swordfish:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::UniqueRectangle:
        case RequiredStrategy::BUGPlusOne:
        case RequiredStrategy::WWing:
        case RequiredStrategy::SimpleColoring:
            return 4;

        // P5 - Diabolical
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

        // P6 - Nightmare
        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::AlignedPairExclusion:
        case RequiredStrategy::AlignedTripleExclusion:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
            return 6;

        // P7 - Theoretical
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return 7;

        // P8 - Brute force
        case RequiredStrategy::Backtracking:
            return 8;
    }
    return 8;
}

inline bool strategy_requires_exact_only(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::AlignedPairExclusion:
        case RequiredStrategy::AlignedTripleExclusion:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return true;
        default:
            return false;
    }
}

inline bool strategy_is_named_structure(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
        case RequiredStrategy::XWing:
        case RequiredStrategy::YWing:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
        case RequiredStrategy::Swordfish:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::BUGPlusOne:
        case RequiredStrategy::UniqueRectangle:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::WWing:
        case RequiredStrategy::Jellyfish:
        case RequiredStrategy::WXYZWing:
        case RequiredStrategy::FinnedSwordfishJellyfish:
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
            return true;
        default:
            return strategy_requires_exact_only(rs);
    }
}

inline bool strategy_is_generic_equivalence_family(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_named_structures_before_generic(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
        case RequiredStrategy::YWing:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
        case RequiredStrategy::Swordfish:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::BUGPlusOne:
        case RequiredStrategy::UniqueRectangle:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::WWing:
        case RequiredStrategy::WXYZWing:
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
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
            return true;
        default:
            return false; // Generics like AIC, ForcingChains, XYChain return false
    }
}

inline bool strategy_suppress_equivalent_generic_families(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
        case RequiredStrategy::YWing:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
        case RequiredStrategy::Swordfish:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::BUGPlusOne:
        case RequiredStrategy::UniqueRectangle:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::WWing:
        case RequiredStrategy::WXYZWing:
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
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_generator_certifier_split(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::None:
        case RequiredStrategy::NakedSingle:
        case RequiredStrategy::HiddenSingle:
        case RequiredStrategy::Backtracking:
            return false;
        default:
            return true; // All Named and Advanced strategies strongly prefer dual clue windows!
    }
}

inline bool strategy_prefers_dual_clue_windows(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::None:
        case RequiredStrategy::NakedSingle:
        case RequiredStrategy::HiddenSingle:
        case RequiredStrategy::Backtracking:
            return false;
        default:
            return true; // Hard Split policy enabled for all P2+
    }
}

inline bool strategy_allows_3x3_theoretical_exact(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_asymmetric_geometry(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
            return true;
        default:
            return false;
    }
}

inline bool strategy_requires_n_at_least_12(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::MSLS:
            return true;
        default:
            return false; // Exocet, SKLoop, POM are allowed on N=9
    }
}

inline bool strategy_prefers_dense_search_budget(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return true;
        default:
            return false;
    }
}

inline bool required_strategy_selectable_for_geometry(RequiredStrategy rs, int box_rows, int box_cols) {
    if (!is_geometry_size_supported(box_rows, box_cols)) {
        return false;
    }
    const int n = box_rows * box_cols;
    if (n < 5 && strategy_min_level(rs) >= 6) {
        return false;
    }
    if (strategy_requires_n_at_least_12(rs) && n < 12) {
        return false;
    }
    if ((rs == RequiredStrategy::GroupedAIC ||
         rs == RequiredStrategy::GroupedXCycle ||
         rs == RequiredStrategy::ContinuousNiceLoop ||
         rs == RequiredStrategy::ALSAIC) &&
        n < 9) {
        return false;
    }
    return true;
}

inline bool difficulty_level_selectable_for_geometry(int level, int box_rows, int box_cols) {
    if (!is_geometry_size_supported(box_rows, box_cols)) {
        return false;
    }
    return level >= 1 && level <= 8;
}

inline double suggest_time_budget_s(int box_rows, int box_cols, int difficulty_level) {
    const int n = std::max(1, box_rows) * std::max(1, box_cols);
    const int lvl = std::clamp(difficulty_level, 1, 8);
    const double base = 0.018 * static_cast<double>(n) * static_cast<double>(n);
    const double diff = 0.85 + static_cast<double>(lvl) * 0.55;
    const double mult = (lvl >= 7) ? 2.5 : 1.0;
    return std::clamp((base + diff) * mult, 1.0, 360.0);
}

inline int strategy_adjusted_level(int difficulty_level, RequiredStrategy required) {
    return std::max(std::clamp(difficulty_level, 1, 8), strategy_min_level(required));
}

inline int suggest_reseed_interval_s(int box_rows, int box_cols, int effective_level) {
    const int n = std::max(1, box_rows) * std::max(1, box_cols);
    const int lvl = std::clamp(effective_level, 1, 8);
    const int base = std::max(1, n / 3);
    return std::clamp(base + lvl, 2, 90);
}

inline int suggest_attempt_time_budget_seconds(int box_rows, int box_cols, int effective_level) {
    return static_cast<int>(std::ceil(suggest_time_budget_s(box_rows, box_cols, effective_level)));
}

inline uint64_t suggest_attempt_node_budget(int box_rows, int box_cols, int effective_level) {
    const uint64_t n = static_cast<uint64_t>(std::max(1, box_rows) * std::max(1, box_cols));
    const uint64_t lvl = static_cast<uint64_t>(std::clamp(effective_level, 1, 8));
    return std::clamp<uint64_t>(n * n * (200 + 60 * lvl), 50'000ULL, 20'000'000ULL);
}

inline bool strategy_prefers_relaxed_candidate_clue_window(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::Jellyfish:
        case RequiredStrategy::WXYZWing:
        case RequiredStrategy::FinnedSwordfishJellyfish:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
        case RequiredStrategy::ALSXZ:
        case RequiredStrategy::RemotePairs:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::AlignedPairExclusion:
        case RequiredStrategy::AlignedTripleExclusion:
        case RequiredStrategy::ALSAIC:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_relaxed_theoretical_clue_ceiling(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::MSLS:
        case RequiredStrategy::PatternOverlayMethod:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_forcing_family_clue_ceiling(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_loop_overlay_clue_ceiling(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::SKLoop:
            return true;
        default:
            return false;
    }
}

inline bool strategy_prefers_preserved_core_seed_window(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
            return true;
        default:
            return false;
    }
}

inline double strategy_large_n_log_scale(int n) {
    if (n <= 9) {
        return 0.0;
    }
    const double denom = std::log2(64.0 / 9.0);
    if (denom <= 0.0) {
        return 0.0;
    }
    return std::clamp(std::log2(static_cast<double>(n) / 9.0) / denom, 0.0, 1.0);
}

inline int scaled_clue_count(int nn, double ratio) {
    return static_cast<int>(ratio * static_cast<double>(nn));
}

inline void apply_scaled_clue_window(
    int n,
    int nn,
    int& min_clues,
    int& max_clues,
    double base_min_ratio,
    double base_max_ratio,
    double ceiling_min_ratio,
    double ceiling_max_ratio) {
    const double scale = strategy_large_n_log_scale(n);
    const double scaled_min_ratio = base_min_ratio + scale * (ceiling_min_ratio - base_min_ratio);
    const double scaled_max_ratio = base_max_ratio + scale * (ceiling_max_ratio - base_max_ratio);
    const int scaled_min = scaled_clue_count(nn, scaled_min_ratio);
    const int scaled_max = scaled_clue_count(nn, scaled_max_ratio);
    min_clues = std::max(min_clues, scaled_min);
    max_clues = std::max(max_clues, scaled_max);
    max_clues = std::clamp(max_clues, min_clues, nn);
}

inline bool strategy_prefers_sparse_p6_bottleneck_window(RequiredStrategy required) {
    switch (required) {
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
            return true;
        default:
            return false;
    }
}

inline ClueRange resolve_auto_clue_range(int box_rows, int box_cols, int difficulty_level, RequiredStrategy required, AutoClueWindowPolicy policy = AutoClueWindowPolicy::Shared) {
    return resolve_auto_clue_range_goldilocks(box_rows, box_cols, difficulty_level, required, policy);
}

inline ClueRange resolve_auto_clue_range_generator(int box_rows, int box_cols, int difficulty_level, RequiredStrategy required) {
    const AutoClueWindowPolicy policy =
        strategy_prefers_dual_clue_windows(required) ? AutoClueWindowPolicy::Generator
                                                    : AutoClueWindowPolicy::Shared;
    return resolve_auto_clue_range(box_rows, box_cols, difficulty_level, required, policy);
}

inline ClueRange resolve_auto_clue_range_certifier(int box_rows, int box_cols, int difficulty_level, RequiredStrategy required) {
    const AutoClueWindowPolicy policy =
        strategy_prefers_dual_clue_windows(required) ? AutoClueWindowPolicy::Certifier
                                                    : AutoClueWindowPolicy::Shared;
    return resolve_auto_clue_range(box_rows, box_cols, difficulty_level, required, policy);
}

inline uint64_t strategy_smoke_seed(RequiredStrategy rs, StrategySmokeVariant variant) {
    const uint64_t base = 0xC0D3AULL;
    const uint64_t tag = static_cast<uint64_t>(static_cast<int>(rs) + 1);
    const uint64_t bias = (variant == StrategySmokeVariant::Asymmetric) ? 0xA510ULL : 0x510AULL;
    return (base << 20) ^ (tag * 0x9E3779B185EBCA87ULL) ^ bias;
}

inline uint64_t strategy_smoke_attempt_cap(RequiredStrategy rs, StrategySmokeVariant variant) {
    const int lvl = std::max(1, strategy_min_level(rs));
    uint64_t attempts = 32ULL;

    if (lvl >= 8) attempts = 48000ULL;          // P8 Boosted for 3x3 Exocet
    else if (lvl >= 7) attempts = 24000ULL;     // P7
    else if (lvl >= 6) attempts = 12000ULL;     // P6
    else if (lvl >= 5) attempts = 2400ULL;      // P5 exact/named patterns are brittle
    else if (lvl >= 4) attempts = 384ULL;       // P4
    else if (lvl >= 3) attempts = 192ULL;       // P3
    else if (lvl >= 2) attempts = 128ULL;       // P2
    else attempts = 96ULL;                      // P1 exact gate / intersections need retries

    if (strategy_prefers_dense_search_budget(rs) || strategy_prefers_named_structures_before_generic(rs)) {
        attempts += attempts / 2ULL;
    }
    if (variant == StrategySmokeVariant::Asymmetric) {
        attempts += attempts / 2ULL;
    }
    return attempts;
}

inline uint64_t strategy_smoke_time_cap_s(RequiredStrategy rs) {
    const int lvl = std::max(1, strategy_min_level(rs));
    if (lvl >= 8) {
        return strategy_prefers_dense_search_budget(rs) ? 360ULL : 300ULL;
    }
    if (lvl >= 7) {
        return strategy_prefers_dense_search_budget(rs) ? 300ULL : 240ULL;
    }
    if (lvl >= 6) {
        return strategy_prefers_dense_search_budget(rs) ? 120ULL : 90ULL;
    }
    if (lvl >= 5) {
        return 90ULL;
    }
    if (lvl >= 4) {
        return 60ULL;
    }
    if (lvl >= 3) {
        return 45ULL;
    }
    return 33ULL;
}

inline bool strategy_smoke_relaxed_hit_allowed(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::RemotePairs:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
        case RequiredStrategy::WXYZWing:
        case RequiredStrategy::FinnedSwordfishJellyfish:
        case RequiredStrategy::ALSXZ:
        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return true;
        default:
            return strategy_min_level(rs) >= 6;
    }
}

inline uint64_t strategy_smoke_min_required_hit(RequiredStrategy rs) {
    if (rs == RequiredStrategy::None || rs == RequiredStrategy::Backtracking) {
        return 0ULL;
    }
    if (strategy_requires_exact_only(rs)) {
        return 0ULL;
    }
    return strategy_smoke_relaxed_hit_allowed(rs) ? 0ULL : 1ULL;
}

inline uint64_t strategy_smoke_min_required_use(RequiredStrategy rs) {
    if (rs == RequiredStrategy::None || rs == RequiredStrategy::Backtracking) {
        return 0ULL;
    }
    return 1ULL;
}

inline bool strategy_smoke_exact_contract_required(RequiredStrategy rs) {
    return rs != RequiredStrategy::None && rs != RequiredStrategy::Backtracking;
}

inline StrategySmokeProfile strategy_smoke_profile(
    RequiredStrategy rs,
    StrategySmokeVariant variant = StrategySmokeVariant::Primary) {
    StrategySmokeProfile profile{};
    profile.required_strategy = rs;
    profile.variant = variant;
    profile.variant_label = (variant == StrategySmokeVariant::Asymmetric) ? "asymmetric" : "primary";

    if (rs == RequiredStrategy::None || rs == RequiredStrategy::Backtracking) {
        return profile;
    }

    const bool want_asymmetric =
        (variant == StrategySmokeVariant::Asymmetric) ||
        (strategy_prefers_asymmetric_geometry(rs) && !strategy_allows_3x3_theoretical_exact(rs));
    profile.box_rows = want_asymmetric ? 4 : 3;
    profile.box_cols = 3;

    if (!want_asymmetric && strategy_requires_n_at_least_12(rs)) {
        profile.box_rows = 4;
        profile.box_cols = 3;
    }

    profile.difficulty = std::max(1, strategy_min_level(rs));
    profile.seed = strategy_smoke_seed(rs, variant);
    profile.pattern_forcing = (profile.difficulty >= 5) || strategy_prefers_named_structures_before_generic(rs);
    profile.mcts_profile = (profile.difficulty >= 8) ? "p8" : ((profile.difficulty >= 7) ? "p7" : "auto");
    profile.strict_canonical = true;
    profile.allow_proxy_advanced = !strategy_suppress_equivalent_generic_families(rs);
    profile.fast_test = (profile.difficulty < 7) && !strategy_prefers_named_structures_before_generic(rs);
    profile.max_total_time_s = strategy_smoke_time_cap_s(rs);
    profile.max_attempts = strategy_smoke_attempt_cap(rs, variant);
    profile.min_required_use = strategy_smoke_min_required_use(rs);
    profile.min_required_hit = strategy_smoke_min_required_hit(rs);
    profile.exact_contract_required = strategy_smoke_exact_contract_required(rs);
    profile.enabled = required_strategy_selectable_for_geometry(rs, profile.box_rows, profile.box_cols);
    return profile;
}

inline StrategySmokeProfile primary_strategy_smoke_profile(RequiredStrategy rs) {
    return strategy_smoke_profile(rs, StrategySmokeVariant::Primary);
}

inline StrategySmokeProfile asymmetric_strategy_smoke_profile(RequiredStrategy rs) {
    return strategy_smoke_profile(rs, StrategySmokeVariant::Asymmetric);
}

inline bool strategy_has_smoke_profile(RequiredStrategy rs) {
    return primary_strategy_smoke_profile(rs).enabled;
}

inline std::string explain_generation_profile_text(const GenerateRunConfig& cfg) {
    std::ostringstream out;
    out << "Generation profile\n";
    out << "geometry=" << cfg.box_rows << "x" << cfg.box_cols << " (n=" << (cfg.box_rows * cfg.box_cols) << ")\n";
    out << "difficulty=" << cfg.difficulty_level_required << " required_strategy=" << to_string(cfg.required_strategy) << "\n";
    out << "mcts=" << (cfg.mcts_digger_enabled ? "on" : "off") << " profile=" << cfg.mcts_tuning_profile << "\n";
    out << "pattern_forcing=" << (cfg.pattern_forcing_enabled ? "on" : "off") << " tries=" << cfg.pattern_forcing_tries << "\n";
    out << "strict_canonical=" << (cfg.strict_canonical_strategies ? "on" : "off")
        << " allow_proxy_advanced=" << (cfg.allow_proxy_advanced ? "on" : "off")
        << " max_pattern_depth=" << cfg.max_pattern_depth << "\n";
    out << "fast_test_mode=" << (cfg.fast_test_mode ? "on" : "off") << "\n";
    out << "quality_contract=" << (cfg.enable_quality_contract ? "on" : "off") << " replay=" << (cfg.enable_replay_validation ? "on" : "off") << "\n";
    return out.str();
}

} // namespace sudoku_hpc