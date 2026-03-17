
// ============================================================================
// SUDOKU HPC - QUALITY BENCHMARK / STRATEGY AUDIT
// File: quality_benchmark.h
// Description: Generates dual-lane audit reports for certifier coverage and
//              generator seedability, plus smoke profile registry export.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../config/run_config.h"
#include "../logic/logic_result.h"
#include "../logic/sudoku_logic_engine.h"

namespace sudoku_hpc::maintenance {

inline const char* bool_flag(bool value) {
    return value ? "1" : "0";
}

inline const char* impl_tier_label(logic::StrategyImplTier tier) {
    switch (tier) {
    case logic::StrategyImplTier::Full: return "full";
    case logic::StrategyImplTier::Hybrid: return "hybrid";
    case logic::StrategyImplTier::Proxy: return "proxy";
    case logic::StrategyImplTier::Disabled: return "disabled";
    }
    return "disabled";
}

inline std::string csv_escape(std::string_view raw) {
    std::string escaped;
    escaped.reserve(raw.size() + 4);
    escaped.push_back('"');
    for (const char ch : raw) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

inline std::string smoke_profile_cli(const StrategySmokeProfile& profile) {
    if (!profile.enabled) {
        return "";
    }

    std::ostringstream out;
    out << "--cli"
        << " --box-rows " << profile.box_rows
        << " --box-cols " << profile.box_cols
        << " --difficulty " << profile.difficulty
        << " --required-strategy " << to_string(profile.required_strategy)
        << " --seed " << profile.seed
        << " --target 1"
        << " --threads 1"
        << " --max-total-time-s " << profile.max_total_time_s
        << " --max-attempts " << profile.max_attempts
        << " --benchmark-mode";
    if (profile.pattern_forcing) {
        out << " --pattern-forcing";
    } else {
        out << " --no-mcts-digger";
    }
    out << " --mcts-profile " << profile.mcts_profile;
    if (profile.strict_canonical) {
        out << " --strict-canonical-strategies";
    }
    if (profile.allow_proxy_advanced) {
        out << " --allow-proxy-advanced";
    } else {
        out << " --no-proxy-advanced";
    }
    if (profile.exact_contract_required) {
        out << " --strict-logical";
    }
    out << (profile.fast_test ? " --fast-test" : " --no-fast-test");
    return out.str();
}

inline std::string generator_seedability_label(const logic::GenericLogicCertify::StrategyAuditRow& row) {
    if (row.generator_exact_template_wired && row.generator_family_fallback_wired) {
        return "exact+fallback";
    }
    if (row.generator_exact_template_wired) {
        return "exact-only";
    }
    if (row.generator_family_fallback_wired) {
        return "family-fallback-only";
    }
    if (row.generator_dispatch_wired) {
        return "dispatcher-only";
    }
    return "unsupported";
}

inline std::string roadmap_lane_label(const logic::GenericLogicCertify::StrategyAuditRow& row) {
    switch (row.audit_decision) {
    case StrategyAuditDecision::Rewrite:
        return (row.level >= 6) ? "rewrite-first" : "rewrite";
    case StrategyAuditDecision::Tighten:
        return (row.level >= 6) ? "tighten-next" : "tighten";
    case StrategyAuditDecision::ExactTemplateMissing:
        return "exact-template-missing";
    case StrategyAuditDecision::Keep:
        return "keep";
    }
    return "keep";
}

inline std::filesystem::path quality_benchmark_csv_path(const std::string& report_path) {
    std::filesystem::path csv_path(report_path);
    if (csv_path.extension().empty()) {
        csv_path += ".csv";
    } else {
        csv_path.replace_extension(".csv");
    }
    return csv_path;
}

inline bool write_quality_benchmark_report(
    const std::string& report_path,
    uint64_t max_cases,
    std::string* csv_path_out = nullptr) {
    namespace fs = std::filesystem;

    const fs::path txt_path(report_path);
    const fs::path csv_path = quality_benchmark_csv_path(report_path);
    if (!txt_path.parent_path().empty()) {
        fs::create_directories(txt_path.parent_path());
    }
    if (!csv_path.parent_path().empty()) {
        fs::create_directories(csv_path.parent_path());
    }

    std::ofstream txt(txt_path, std::ios::out | std::ios::trunc);
    std::ofstream csv(csv_path, std::ios::out | std::ios::trunc);
    if (!txt || !csv) {
        return false;
    }

    const auto summary = logic::GenericLogicCertify::build_audit_summary();
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::tm tm_now = *std::localtime(&now);

    txt << "Sudoku HPC quality benchmark / strategy audit\n";
    txt << "Generated: " << std::asctime(&tm_now);
    txt << "\n";
    txt << "[Summary]\n";
    txt << "corridor_policy=named-before-generic\n";
    txt << "smoke_use_metric=selected-step-per-slot\n";
    txt << "smoke_hit_metric=effective-progress-per-slot\n";
    txt << "total_slots=" << summary.total_slots << "\n";
    txt << "canonical_full=" << summary.canonical_full << "\n";
    txt << "textbook_full=" << summary.textbook_full << "\n";
    txt << "family_approx=" << summary.family_approx << "\n";
    txt << "partial=" << summary.partial << "\n";
    txt << "parser_selectable=" << summary.parser_selectable << "\n";
    txt << "certifier_wired=" << summary.certifier_wired << "\n";
    txt << "mcts_dispatch_wired=" << summary.mcts_dispatch_wired << "\n";
    txt << "generator_dispatch_wired=" << summary.generator_dispatch_wired << "\n";
    txt << "generator_exact_template_wired=" << summary.generator_exact_template_wired << "\n";
    txt << "generator_family_fallback_wired=" << summary.generator_family_fallback_wired << "\n";
    txt << "family_fallback_only=" << summary.family_fallback_only << "\n";
    txt << "exact_required_but_missing=" << summary.exact_required_but_missing << "\n";
    txt << "smoke_profile_present=" << summary.smoke_profile_present << "\n";
    txt << "asymmetric_smoke_profile_present=" << summary.asymmetric_smoke_profile_present << "\n";
    txt << "rewrite_candidates=" << summary.rewrite_candidates << "\n";
    txt << "tighten_candidates=" << summary.tighten_candidates << "\n";
    txt << "\n";
    txt << "[Rows]\n";

    csv
        << "slot,required_strategy,id,level,impl_tier,coverage_grade,asymmetry_verified,zero_alloc_grade,"
        << "audit_decision,parser_selectable,certifier_wired,mcts_dispatch_wired,generator_policy,"
        << "generator_dispatch_wired,generator_exact_template_wired,generator_family_fallback_wired,"
        << "generator_seedability,smoke_profile_present,smoke_asymmetric_present,canonical_full,"
        << "proxy_or_disabled,family_fallback_only,exact_contract_required,exact_required_but_missing,"
        << "roadmap_lane,smoke_primary_variant,smoke_primary_box_rows,smoke_primary_box_cols,"
        << "smoke_primary_difficulty,smoke_primary_seed,smoke_primary_pattern_forcing,smoke_primary_mcts_profile,"
        << "smoke_primary_strict_canonical,smoke_primary_allow_proxy,smoke_primary_max_total_time_s,"
        << "smoke_primary_max_attempts,smoke_primary_min_required_use,smoke_primary_min_required_hit,"
        << "smoke_primary_exact_contract_required,smoke_primary_cli,smoke_asymmetric_variant,"
        << "smoke_asymmetric_box_rows,smoke_asymmetric_box_cols,smoke_asymmetric_difficulty,"
        << "smoke_asymmetric_seed,smoke_asymmetric_pattern_forcing,smoke_asymmetric_mcts_profile,"
        << "smoke_asymmetric_strict_canonical,smoke_asymmetric_allow_proxy,smoke_asymmetric_max_total_time_s,"
        << "smoke_asymmetric_max_attempts,smoke_asymmetric_min_required_use,smoke_asymmetric_min_required_hit,"
        << "smoke_asymmetric_exact_contract_required,smoke_asymmetric_cli\n";

    std::vector<logic::GenericLogicCertify::StrategyAuditRow> rows;
    rows.reserve(logic::kStrategySlotCount);
    logic::GenericLogicCertify::for_each_strategy_audit_row([&rows](const auto& row) {
        rows.push_back(row);
    });
    if (max_cases > 0 && max_cases < rows.size()) {
        rows.resize(static_cast<size_t>(max_cases));
    } else if (max_cases == 0) {
        max_cases = static_cast<uint64_t>(rows.size());
    }

    for (const auto& row : rows) {
        const StrategySmokeProfile primary = primary_strategy_smoke_profile(row.required_strategy);
        const StrategySmokeProfile asymmetric = row.supports_asymmetric
            ? asymmetric_strategy_smoke_profile(row.required_strategy)
            : StrategySmokeProfile{};
        const bool primary_exact_contract_required =
            row.exact_contract_required && primary.exact_contract_required;
        const ClueRange primary_generator_clues = resolve_auto_clue_range(primary.box_rows, primary.box_cols, primary.difficulty, row.required_strategy, AutoClueWindowPolicy::Generator);
        const ClueRange primary_certifier_clues = resolve_auto_clue_range(primary.box_rows, primary.box_cols, primary.difficulty, row.required_strategy, AutoClueWindowPolicy::Certifier);
        const uint64_t primary_min_use =
            std::max<uint64_t>(primary.min_required_use, row.required_strategy == RequiredStrategy::None ? 0ULL : 1ULL);
        const uint64_t primary_min_hit =
            std::max<uint64_t>(primary.min_required_hit, row.required_strategy == RequiredStrategy::None ? 0ULL : 1ULL);
        const bool asymmetric_exact_contract_required =
            row.exact_contract_required && asymmetric.exact_contract_required;
        const ClueRange asymmetric_generator_clues = asymmetric.enabled ? resolve_auto_clue_range(asymmetric.box_rows, asymmetric.box_cols, asymmetric.difficulty, row.required_strategy, AutoClueWindowPolicy::Generator) : ClueRange{};
        const ClueRange asymmetric_certifier_clues = asymmetric.enabled ? resolve_auto_clue_range(asymmetric.box_rows, asymmetric.box_cols, asymmetric.difficulty, row.required_strategy, AutoClueWindowPolicy::Certifier) : ClueRange{};
        const uint64_t asymmetric_min_use =
            std::max<uint64_t>(asymmetric.min_required_use, row.required_strategy == RequiredStrategy::None ? 0ULL : 1ULL);
        const uint64_t asymmetric_min_hit =
            std::max<uint64_t>(asymmetric.min_required_hit, row.required_strategy == RequiredStrategy::None ? 0ULL : 1ULL);
        const std::string seedability = generator_seedability_label(row);
        const std::string roadmap_lane = roadmap_lane_label(row);
        StrategySmokeProfile primary_export = primary;
        primary_export.exact_contract_required = primary_exact_contract_required;
        StrategySmokeProfile asymmetric_export = asymmetric;
        asymmetric_export.exact_contract_required = asymmetric_exact_contract_required;
        const std::string primary_cli = smoke_profile_cli(primary_export);
        const std::string asymmetric_cli = smoke_profile_cli(asymmetric_export);

        txt
            << row.slot << " "
            << to_string(row.required_strategy)
            << " level=" << static_cast<int>(row.level)
            << " tier=" << impl_tier_label(row.impl_tier)
            << " coverage=" << to_string(row.coverage_grade)
            << " decision=" << to_string(row.audit_decision)
            << " canonical=" << bool_flag(row.canonical_full)
            << " parser=" << bool_flag(row.parser_selectable)
            << " certifier=" << bool_flag(row.certifier_wired)
            << " exact=" << bool_flag(row.generator_exact_template_wired)
            << " fallback=" << bool_flag(row.generator_family_fallback_wired)
            << " smoke=" << bool_flag(row.smoke_profile_present)
            << " seedability=" << seedability
            << " primary_generator_clues=" << primary_generator_clues.min_clues << "-" << primary_generator_clues.max_clues
            << " primary_certifier_clues=" << primary_certifier_clues.min_clues << "-" << primary_certifier_clues.max_clues
            << "\n";

        csv
            << row.slot << ","
            << csv_escape(to_string(row.required_strategy)) << ","
            << csv_escape(row.id) << ","
            << static_cast<int>(row.level) << ","
            << csv_escape(impl_tier_label(row.impl_tier)) << ","
            << csv_escape(to_string(row.coverage_grade)) << ","
            << bool_flag(row.asymmetry_verified) << ","
            << csv_escape(to_string(row.zero_alloc_grade)) << ","
            << csv_escape(to_string(row.audit_decision)) << ","
            << bool_flag(row.parser_selectable) << ","
            << bool_flag(row.certifier_wired) << ","
            << bool_flag(row.mcts_dispatch_wired) << ","
            << csv_escape(to_string(row.generator_policy)) << ","
            << bool_flag(row.generator_dispatch_wired) << ","
            << bool_flag(row.generator_exact_template_wired) << ","
            << bool_flag(row.generator_family_fallback_wired) << ","
            << csv_escape(seedability) << ","
            << bool_flag(row.smoke_profile_present) << ","
            << bool_flag(row.asymmetric_smoke_profile_present) << ","
            << bool_flag(row.canonical_full) << ","
            << bool_flag(row.proxy_or_disabled) << ","
            << bool_flag(row.family_fallback_only) << ","
            << bool_flag(row.exact_contract_required) << ","
            << bool_flag(row.exact_required_but_missing) << ","
            << csv_escape(roadmap_lane) << ","
            << csv_escape(primary.variant_label) << ","
            << primary.box_rows << ","
            << primary.box_cols << ","
            << primary.difficulty << ","
            << primary.seed << ","
            << bool_flag(primary.pattern_forcing) << ","
            << csv_escape(primary.mcts_profile) << ","
            << bool_flag(primary.strict_canonical) << ","
            << bool_flag(primary.allow_proxy_advanced) << ","
            << primary.max_total_time_s << ","
            << primary.max_attempts << ","
            << primary_min_use << ","
            << primary_min_hit << ","
            << bool_flag(primary_exact_contract_required) << ","
            << csv_escape(primary_cli) << ","
            << csv_escape(std::to_string(primary_generator_clues.min_clues) + "-" + std::to_string(primary_generator_clues.max_clues)) << ","
            << csv_escape(std::to_string(primary_certifier_clues.min_clues) + "-" + std::to_string(primary_certifier_clues.max_clues)) << ","
            << csv_escape(asymmetric.variant_label) << ","
            << asymmetric.box_rows << ","
            << asymmetric.box_cols << ","
            << asymmetric.difficulty << ","
            << asymmetric.seed << ","
            << bool_flag(asymmetric.pattern_forcing) << ","
            << csv_escape(asymmetric.mcts_profile) << ","
            << bool_flag(asymmetric.strict_canonical) << ","
            << bool_flag(asymmetric.allow_proxy_advanced) << ","
            << asymmetric.max_total_time_s << ","
            << asymmetric.max_attempts << ","
            << asymmetric_min_use << ","
            << asymmetric_min_hit << ","
            << bool_flag(asymmetric_exact_contract_required) << ","
            << csv_escape(asymmetric_cli) << ","
            << csv_escape(std::to_string(asymmetric_generator_clues.min_clues) + "-" + std::to_string(asymmetric_generator_clues.max_clues)) << ","
            << csv_escape(std::to_string(asymmetric_certifier_clues.min_clues) + "-" + std::to_string(asymmetric_certifier_clues.max_clues))
            << "\n";
    }

    txt << "\n[Roadmap: Rewrite First]\n";
    for (const auto& row : rows) {
        if (row.level < 6 || row.audit_decision != StrategyAuditDecision::Rewrite) {
            continue;
        }
        txt << "- " << to_string(row.required_strategy)
            << " (level " << static_cast<int>(row.level)
            << ", coverage=" << to_string(row.coverage_grade)
            << ", seedability=" << generator_seedability_label(row) << ")\n";
    }

    txt << "\n[Roadmap: Tighten Next]\n";
    for (const auto& row : rows) {
        if (row.level < 6 || row.audit_decision != StrategyAuditDecision::Tighten) {
            continue;
        }
        txt << "- " << to_string(row.required_strategy)
            << " (level " << static_cast<int>(row.level)
            << ", coverage=" << to_string(row.coverage_grade)
            << ", seedability=" << generator_seedability_label(row) << ")\n";
    }

    txt << "\n[Dual-lane Notes]\n";
    txt << "- certifier_canonical counts only textbook/full/hotpath-zero-alloc slots.\n";
    txt << "- generator_seedability reports exact template vs family fallback independently.\n";
    txt << "- exact-required strategies must not rely on family fallback in smoke validation.\n";
    txt << "- csv_path=" << csv_path.string() << "\n";

    if (csv_path_out != nullptr) {
        *csv_path_out = csv_path.string();
    }
    return true;
}

} // namespace sudoku_hpc::maintenance


