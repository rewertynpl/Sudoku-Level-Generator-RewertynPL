// ============================================================================
// SUDOKU HPC - GENERATOR PIPELINE
// Moduł: generator_facade.h
// Opis: Główna fasada sterująca procesem generacji łamigłówki (Pipeline).
//       Integruje Kernels, Pattern Forcing, MCTS Diggera oraz Certyfikator.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <vector>

// Core & Config
#include "../core/board.h"
#include "../config/run_config.h"

// Core Engines
#include "core_engines/dlx_solver.h"
#include "core_engines/solved_kernel.h"
#include "core_engines/quick_prefilter.h"

// Logic & Strategies (Fasada silnika certyfikacji)
#include "../logic/sudoku_logic_engine.h"

// Post-Processing
#include "post_processing/quality_metrics.h"
#include "post_processing/replay_validator.h"

// MCTS Digger
#include "mcts_digger/bottleneck_digger.h"

// Pattern Forcing
#include "pattern_forcing/pattern_planter.h"
#include "../utils/logging.h"

namespace sudoku_hpc::generator {

using core_engines::SearchAbortControl;

struct GenericPuzzleCandidate {
    std::vector<uint16_t> puzzle;
    std::vector<uint16_t> solution;
    int clues = 0;
};

// ============================================================================
// ZERO-ALLOCATION SERIALIZATION
// Zamiast std::ostringstream i tworzenia wielu mniejszych stringów używamy
// jednego bufora, a wartości wpisujemy przez systemowy std::to_chars().
// ============================================================================
inline std::string serialize_line_generic(
    uint64_t seed,
    const GenerateRunConfig& cfg,
    const GenericPuzzleCandidate& candidate,
    int nn) {
    
    std::string out;
    // Prealokacja maksymalnego bezpiecznego rozmiaru
    out.resize(128 + static_cast<size_t>(nn) * 4); 
    char* ptr = out.data();
    char* end = ptr + out.size(); 

    auto res1 = std::to_chars(ptr, end, seed); ptr = res1.ptr;
    *ptr++ = ',';
    auto res2 = std::to_chars(ptr, end, cfg.box_rows); ptr = res2.ptr;
    *ptr++ = ',';
    auto res3 = std::to_chars(ptr, end, cfg.box_cols); ptr = res3.ptr;

    const uint16_t* puz_ptr = candidate.puzzle.data();
    const uint16_t* sol_ptr = candidate.solution.data();

    for (int i = 0; i < nn; ++i) {
        *ptr++ = ',';
        const uint16_t v = puz_ptr[i];
        if (v != 0) {
            *ptr++ = 't'; // given (clue)
            auto res = std::to_chars(ptr, end, v); ptr = res.ptr;
        } else {
            // solution value
            auto res = std::to_chars(ptr, end, sol_ptr[i]); ptr = res.ptr;
        }
    }
    
    // Ucinamy sznurek do faktycznie wykorzystanego zakresu
    out.resize(ptr - out.data());
    return out;
}

// Struktura na wewnętrzne metryki pojedynczej próby, dla celów mikro-profilowania
struct AttemptPerfStats {
    uint64_t solved_elapsed_ns = 0;
    uint64_t dig_elapsed_ns = 0;
    uint64_t prefilter_elapsed_ns = 0;
    uint64_t logic_elapsed_ns = 0;
    uint64_t uniqueness_calls = 0;
    uint64_t uniqueness_nodes = 0;
    uint64_t uniqueness_elapsed_ns = 0;
    uint64_t logic_steps = 0;
    uint64_t strategy_naked_use = 0;
    uint64_t strategy_naked_hit = 0;
    uint64_t strategy_hidden_use = 0;
    uint64_t strategy_hidden_hit = 0;
    uint64_t certifier_required_strategy_analyzed = 0;
    uint64_t certifier_required_strategy_use = 0;
    uint64_t certifier_required_strategy_hit = 0;
    uint64_t mcts_advanced_evals = 0;
    uint64_t mcts_required_strategy_analyzed = 0;
    uint64_t mcts_required_strategy_use = 0;
    uint64_t mcts_required_strategy_hit = 0;
    int pattern_template_score = 0;
    int pattern_best_template_score = 0;
    bool pattern_exact_template = false;
    bool pattern_family_fallback_used = false;
    bool required_strategy_exact_contract_met = false;
    int pattern_template_score_delta = 0;
    int pattern_mutation_strength = 0;
    int pattern_planner_zero_use_streak = 0;
    int pattern_planner_failure_streak = 0;
    int pattern_adaptive_target_strength = 0;
    pattern_forcing::PatternKind pattern_template_family = pattern_forcing::PatternKind::None;
    PatternGeneratorPolicy pattern_generator_policy = PatternGeneratorPolicy::Unsupported;
    pattern_forcing::PatternMutationSource pattern_mutation_source = pattern_forcing::PatternMutationSource::Random;
};

// Pomocnicza metoda oceniająca, czy oczekiwany poziom został spełniony
inline bool evaluate_difficulty_contract_generic(const logic::GenericLogicCertifyResult& logic_result, int difficulty_level_required) {
    const int lvl = std::clamp(difficulty_level_required, 1, 9);
    
    if (lvl <= 1) {
        return logic_result.solved && (logic_result.used_naked_single || logic_result.used_hidden_single);
    }
    if (lvl == 2) {
        return logic_result.used_pointing_pairs || logic_result.used_box_line;
    }
    if (lvl == 3) {
        return logic_result.used_naked_pair || logic_result.used_hidden_pair ||
               logic_result.used_naked_triple || logic_result.used_hidden_triple;
    }
    if (lvl == 4) {
        return logic_result.used_naked_quad || logic_result.used_hidden_quad ||
               logic_result.used_x_wing || logic_result.used_y_wing ||
               logic_result.used_skyscraper || logic_result.used_two_string_kite ||
               logic_result.used_empty_rectangle || logic_result.used_remote_pairs;
    }
    if (lvl == 5) {
        return logic_result.used_swordfish || logic_result.used_bug_plus_one ||
               logic_result.used_finned_x_wing_sashimi || logic_result.used_simple_coloring ||
               logic_result.used_unique_rectangle || logic_result.used_xyz_wing ||
               logic_result.used_w_wing;
    }
    if (lvl == 6) {
        return logic_result.used_jellyfish || logic_result.used_x_chain ||
               logic_result.used_xy_chain || logic_result.used_wxyz_wing ||
               logic_result.used_finned_swordfish_jellyfish || logic_result.used_als_xz ||
               logic_result.used_unique_loop || logic_result.used_avoidable_rectangle ||
               logic_result.used_bivalue_oddagon || logic_result.used_ur_extended ||
               logic_result.used_hidden_ur || logic_result.used_bug_type2 ||
               logic_result.used_bug_type3 || logic_result.used_bug_type4 ||
               logic_result.used_borescoper_qiu_deadly_pattern ||
               logic_result.used_unique_rectangle || logic_result.used_bug_plus_one ||
               logic_result.used_w_wing;
    }
    if (lvl == 7) {
        return logic_result.used_medusa_3d || logic_result.used_aic || logic_result.used_grouped_aic ||
               logic_result.used_grouped_x_cycle || logic_result.used_continuous_nice_loop ||
               logic_result.used_als_xy_wing || logic_result.used_als_chain ||
               logic_result.used_sue_de_coq || logic_result.used_death_blossom ||
               logic_result.used_franken_fish || logic_result.used_mutant_fish ||
               logic_result.used_kraken_fish || logic_result.used_squirmbag ||
               logic_result.used_aligned_pair_exclusion || logic_result.used_aligned_triple_exclusion ||
               logic_result.used_als_aic;
    }
    if (lvl == 8) {
        return logic_result.used_msls || logic_result.used_exocet || logic_result.used_senior_exocet ||
               logic_result.used_sk_loop || logic_result.used_pattern_overlay_method ||
               logic_result.used_forcing_chains || logic_result.used_dynamic_forcing_chains;
    }

    return true; // lvl 9 = Backtracking (brak specyficznych wymagań dla strategii)
}

// Sprawdzenie, czy Certyfikator użył żądanej strategii.
inline bool evaluate_required_strategy_contract_generic(
    const logic::GenericLogicCertifyResult& logic_result,
    const GenerateRunConfig& cfg,
    RequiredStrategy required,
    RequiredStrategyAttemptInfo& strategy_info) {

    strategy_info = {};
    if (required == RequiredStrategy::None) {
        return true;
    }

    size_t required_slot = 0;
    if (!mcts_digger::mcts_required_strategy_slot(required, required_slot)) {
        if (required == RequiredStrategy::Backtracking) {
            strategy_info.analyzed_required_strategy = true;
            strategy_info.required_strategy_use_confirmed = true;
            strategy_info.required_strategy_hit_confirmed = true;
            strategy_info.matched_required_strategy = true;
            return true;
        }
        return false;
    }

    if (cfg.strict_canonical_strategies && !logic::GenericLogicCertify::is_full_canonical_slot(required_slot)) {
        strategy_info.analyzed_required_strategy = true;
        strategy_info.required_strategy_use_confirmed = true;
        strategy_info.required_strategy_hit_confirmed = false;
        strategy_info.matched_required_strategy = false;
        return false;
    }
    if (!cfg.allow_proxy_advanced && logic::GenericLogicCertify::is_proxy_slot(required_slot)) {
        strategy_info.analyzed_required_strategy = true;
        strategy_info.required_strategy_use_confirmed = true;
        strategy_info.required_strategy_hit_confirmed = false;
        strategy_info.matched_required_strategy = false;
        return false;
    }

    strategy_info.required_strategy_use_confirmed = logic_result.strategy_stats[required_slot].use_count > 0;
    strategy_info.required_strategy_hit_confirmed = logic_result.strategy_stats[required_slot].hit_count > 0;
    strategy_info.analyzed_required_strategy = strategy_info.required_strategy_use_confirmed;
    strategy_info.matched_required_strategy =
        strategy_info.required_strategy_use_confirmed && strategy_info.required_strategy_hit_confirmed;
    return strategy_info.matched_required_strategy;
}


// ============================================================================
// GŁÓWNA FUNKCJA KONTROLI PIPELINE'U (Wykonywana per każda próba generowania)
// ============================================================================
inline bool generate_one_generic(
    const GenerateRunConfig& cfg,
    const GenericTopology& topo,
    std::mt19937_64& rng,
    GenericPuzzleCandidate& candidate,
    RejectReason& reason,
    RequiredStrategyAttemptInfo& strategy_info,
    const core_engines::GenericSolvedKernel& solved,
    const core_engines::GenericQuickPrefilter& prefilter,
    const logic::GenericLogicCertify& logic,
    const core_engines::GenericUniquenessCounter& uniq,
    const std::atomic<bool>* force_abort_ptr = nullptr,
    bool* timed_out = nullptr,
    const std::atomic<bool>* external_cancel_ptr = nullptr,
    const std::atomic<bool>* external_pause_ptr = nullptr,
    post_processing::QualityContract* quality_contract_out = nullptr,
    post_processing::QualityMetrics* quality_metrics_out = nullptr,
    post_processing::ReplayValidationResult* replay_out = nullptr,
    AttemptPerfStats* perf_out = nullptr) {
    
    const bool has_timed_out_ptr = (timed_out != nullptr);
    const bool has_quality_contract_out = (quality_contract_out != nullptr);
    const bool has_quality_metrics_out = (quality_metrics_out != nullptr);
    const bool has_replay_out = (replay_out != nullptr);
    const bool collect_perf = (perf_out != nullptr);
    
    if (has_timed_out_ptr) *timed_out = false;
    strategy_info = {};
    if (has_quality_contract_out) *quality_contract_out = {};
    if (has_quality_metrics_out) *quality_metrics_out = {};
    if (has_replay_out) *replay_out = {};
    if (collect_perf) *perf_out = {};
    
    const bool quality_contract_enabled = cfg.enable_quality_contract;
    const bool distribution_filter_enabled = quality_contract_enabled && cfg.enable_distribution_filter;
    const bool replay_validation_enabled = quality_contract_enabled && cfg.enable_replay_validation;
    const bool need_quality_metrics = quality_contract_enabled || quality_contract_out != nullptr || quality_metrics_out != nullptr;
    const bool budget_enabled = cfg.attempt_time_budget_s > 0.0 || cfg.attempt_node_budget > 0 || force_abort_ptr != nullptr;
    const bool trace_stage_diag = cfg.fast_test_mode || cfg.required_strategy != RequiredStrategy::None;
    mcts_digger::GenericMctsBottleneckDigger::RunStats mcts_stats{};
    const uint8_t* dig_protected_cells = nullptr;
    const uint64_t* dig_allowed_masks = nullptr;
    const int* dig_anchor_idx = nullptr;
    const uint64_t* dig_anchor_masks = nullptr;
    int dig_anchor_count = 0;
    bool dig_exact_template = false;
    int dig_template_score = 0;
    int dig_best_template_score = 0;
    int dig_template_score_delta = 0;
    int dig_mutation_strength = 0;
    int dig_planner_zero_use_streak = 0;
    int dig_planner_failure_streak = 0;
    int dig_adaptive_target_strength = 0;
    bool dig_family_fallback_used = false;
    bool dig_exact_contract_met = false;
    pattern_forcing::PatternKind dig_pattern_kind = pattern_forcing::PatternKind::None;
    PatternGeneratorPolicy dig_generator_policy = PatternGeneratorPolicy::Unsupported;
    pattern_forcing::PatternMutationSource dig_mutation_source = pattern_forcing::PatternMutationSource::Random;

    auto log_stage_begin = [&](const char* stage) {
        if (!trace_stage_diag) {
            return;
        }
        log_info(
            "generator.stage",
            "stage=" + std::string(stage) +
            " phase=begin geom=" + std::to_string(cfg.box_rows) + "x" + std::to_string(cfg.box_cols) +
            " difficulty=" + std::to_string(cfg.difficulty_level_required) +
            " required=" + std::string(to_string(cfg.required_strategy)) +
            " fast_test=" + std::string(cfg.fast_test_mode ? "1" : "0"));
    };

    auto log_stage_end = [&](const char* stage, bool ok, const SearchAbortControl* stage_budget, const std::string& extra = std::string()) {
        if (!trace_stage_diag) {
            return;
        }
        std::string msg =
            "stage=" + std::string(stage) +
            " phase=end ok=" + std::string(ok ? "1" : "0");
        if (stage_budget != nullptr) {
            msg +=
                " aborted=" + std::string(stage_budget->aborted() ? "1" : "0") +
                " by_time=" + std::string(stage_budget->aborted_by_time ? "1" : "0") +
                " by_nodes=" + std::string(stage_budget->aborted_by_nodes ? "1" : "0") +
                " by_pause=" + std::string(stage_budget->aborted_by_pause ? "1" : "0") +
                " nodes=" + std::to_string(stage_budget->nodes);
        }
        if (!extra.empty()) {
            msg += " " + extra;
        }
        log_info("generator.stage", msg);
    };

    auto count_protected_cells = [&](const uint8_t* protected_cells) -> int {
        if (protected_cells == nullptr) {
            return 0;
        }
        int count = 0;
        for (int i = 0; i < topo.nn; ++i) {
            count += protected_cells[i] ? 1 : 0;
        }
        return count;
    };

    auto log_pattern_contract = [&](const char* phase, const std::string& extra = std::string()) {
        if (!trace_stage_diag) {
            return;
        }
        std::string msg =
            "phase=" + std::string(phase) +
            " required=" + std::string(to_string(cfg.required_strategy)) +
            " kind=" + std::string(pattern_forcing::pattern_kind_label(dig_pattern_kind)) +
            " policy=" + std::string(to_string(dig_generator_policy)) +
            " exact=" + std::string(dig_exact_template ? "1" : "0") +
            " family_fallback=" + std::string(dig_family_fallback_used ? "1" : "0") +
            " exact_contract=" + std::string(dig_exact_contract_met ? "1" : "0") +
            " anchors=" + std::to_string(dig_anchor_count) +
            " protected=" + std::to_string(count_protected_cells(dig_protected_cells)) +
            " score=" + std::to_string(dig_template_score) +
            " best_score=" + std::to_string(dig_best_template_score) +
            " score_delta=" + std::to_string(dig_template_score_delta) +
            " mutation=" + std::string(pattern_forcing::pattern_mutation_source_label(dig_mutation_source)) +
            " mutation_strength=" + std::to_string(dig_mutation_strength) +
            " planner_zero_use=" + std::to_string(dig_planner_zero_use_streak) +
            " planner_fail=" + std::to_string(dig_planner_failure_streak) +
            " adaptive_target=" + std::to_string(dig_adaptive_target_strength);
        if (!extra.empty()) {
            msg += " " + extra;
        }
        log_info("pattern.contract", msg);
    };

    auto log_strategy_contract = [&](const char* phase, RejectReason reject_reason, const logic::GenericLogicCertifyResult* logic_result = nullptr) {
        if (!trace_stage_diag || cfg.required_strategy == RequiredStrategy::None) {
            return;
        }
        size_t required_slot = 0;
        std::ostringstream oss;
        oss << "phase=" << phase
            << " required=" << to_string(cfg.required_strategy)
            << " reject=" << static_cast<int>(reject_reason)
            << " reqA/U/H=" << mcts_stats.required_strategy_analyzed
            << "/" << mcts_stats.required_strategy_uses
            << "/" << mcts_stats.required_strategy_hits;
        if (logic::GenericLogicCertify::slot_from_required_strategy(cfg.required_strategy, required_slot)) {
            const auto& meta = logic::GenericLogicCertify::strategy_meta_for_slot(required_slot);
            oss << " slot=" << required_slot
                << " slot_id=" << meta.id
                << " coverage=" << to_string(meta.coverage_grade)
                << " generator_policy=" << to_string(meta.generator_policy)
                << " zero_alloc=" << to_string(meta.zero_alloc_grade)
                << " audit_decision=" << to_string(meta.audit_decision);
            if (logic_result != nullptr) {
                const auto& stats = logic_result->strategy_stats[required_slot];
                oss << " logic_use/hit=" << stats.use_count
                    << "/" << stats.hit_count
                    << " solved=" << (logic_result->solved ? 1 : 0)
                    << " timed_out=" << (logic_result->timed_out ? 1 : 0)
                    << " steps=" << logic_result->steps;
            }
        }
        log_info("strategy.contract", oss.str());
    };
    
    SearchAbortControl budget;
    if (cfg.attempt_time_budget_s > 0.0) {
        budget.time_enabled = true;
        budget.deadline = std::chrono::steady_clock::now() + 
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(cfg.attempt_time_budget_s));
    }
    if (cfg.attempt_node_budget > 0) {
        budget.node_enabled = true;
        budget.node_limit = cfg.attempt_node_budget;
    }
    if (force_abort_ptr != nullptr) budget.force_abort_ptr = force_abort_ptr;
    budget.cancel_ptr = external_cancel_ptr;
    budget.pause_ptr = external_pause_ptr;
    
    SearchAbortControl* budget_ptr = budget_enabled ? &budget : nullptr;

    candidate.solution.resize(static_cast<size_t>(topo.nn), 0);
    candidate.puzzle.resize(static_cast<size_t>(topo.nn), 0);
    candidate.clues = 0;

    // ------------------------------------------------------------------------
    // ETAP 1: Generowanie pełnej, poprawnej planszy "Solved Grid"
    // ------------------------------------------------------------------------
    const auto solved_t0 = std::chrono::steady_clock::now();
    bool solved_ok = false;
    
    const PatternGeneratorPolicy required_generator_policy =
        pattern_forcing::pattern_strategy_policy(cfg.required_strategy, cfg.difficulty_level_required).generator_policy;
    const bool strict_exact_contract =
        cfg.pattern_forcing_enabled &&
        pattern_forcing::pattern_policy_requires_exact(required_generator_policy);
    dig_generator_policy = required_generator_policy;
    dig_exact_contract_met = !strict_exact_contract;

    if (cfg.pattern_forcing_enabled) {
        const int pf_tries = std::max(1, cfg.pattern_forcing_tries);
        for (int pf_try = 0; pf_try < pf_tries && !solved_ok; ++pf_try) {
            pattern_forcing::PatternSeedView pf_seed{};
            if (!pattern_forcing::build_seed(
                    topo, cfg, cfg.required_strategy, cfg.difficulty_level_required, rng, pf_seed)) {
                log_info(
                    "pattern.contract",
                    "phase=seed-miss required=" + std::string(to_string(cfg.required_strategy)) +
                    " pf_try=" + std::to_string(pf_try));
                if (strict_exact_contract) {
                    continue;
                }
                break;
            }
            if (pf_seed.seed_puzzle == nullptr || pf_seed.allowed_masks == nullptr) {
                log_info(
                    "pattern.contract",
                    "phase=seed-invalid required=" + std::string(to_string(cfg.required_strategy)) +
                    " pf_try=" + std::to_string(pf_try) +
                    " kind=" + std::string(pattern_forcing::pattern_kind_label(pf_seed.kind)));
                if (strict_exact_contract) {
                    continue;
                }
                break;
            }

            // Rozwiązanie narzuconego układu przez DLX Solver
            log_stage_begin("pattern_solve");
            solved_ok = uniq.solve_and_capture(
                *pf_seed.seed_puzzle, topo, candidate.solution, budget_ptr, pf_seed.allowed_masks);
            log_stage_end(
                "pattern_solve",
                solved_ok,
                budget_ptr,
                "pf_try=" + std::to_string(pf_try) +
                " exact=" + std::string(pf_seed.exact_template ? "1" : "0") +
                " kind=" + std::string(pattern_forcing::pattern_kind_label(pf_seed.kind)) +
                " score=" + std::to_string(pf_seed.template_score));
                
            if (solved_ok && cfg.pattern_forcing_lock_anchors && pf_seed.protected_cells != nullptr &&
                !pf_seed.protected_cells->empty()) {
                dig_protected_cells = pf_seed.protected_cells->data();
            }
            if (solved_ok && pf_seed.allowed_masks != nullptr && !pf_seed.allowed_masks->empty()) {
                dig_allowed_masks = pf_seed.allowed_masks->data();
                dig_anchor_idx = pf_seed.anchor_idx;
                dig_anchor_masks = pf_seed.anchor_masks;
                dig_anchor_count = pf_seed.anchor_count;
                dig_exact_template = pf_seed.exact_template;
                dig_family_fallback_used = pf_seed.family_fallback_used;
                dig_exact_contract_met = pf_seed.required_strategy_exact_contract_met;
                dig_template_score = pf_seed.template_score;
                dig_best_template_score = pf_seed.best_template_score;
                dig_template_score_delta = pf_seed.template_score_delta;
                dig_mutation_strength = pf_seed.mutation_strength;
                dig_planner_zero_use_streak = pf_seed.planner_zero_use_streak;
                dig_planner_failure_streak = pf_seed.planner_failure_streak;
                dig_adaptive_target_strength = pf_seed.adaptive_target_strength;
                dig_pattern_kind = pf_seed.kind;
                dig_generator_policy = pf_seed.generator_policy;
                dig_mutation_source = pf_seed.mutation_source;
            }
            if (solved_ok) {
                log_pattern_contract(
                    "seed-built",
                    "pf_try=" + std::to_string(pf_try) +
                    " allowed_masks=" + std::to_string(
                        (pf_seed.allowed_masks != nullptr) ? static_cast<int>(pf_seed.allowed_masks->size()) : 0));
            } else {
                log_info(
                    "pattern.contract",
                    "phase=seed-unsolved required=" + std::string(to_string(cfg.required_strategy)) +
                    " pf_try=" + std::to_string(pf_try) +
                    " kind=" + std::string(pattern_forcing::pattern_kind_label(pf_seed.kind)) +
                    " exact=" + std::string(pf_seed.exact_template ? "1" : "0") +
                    " score=" + std::to_string(pf_seed.template_score));
            }
            if (!solved_ok) {
                pattern_forcing::note_template_attempt_feedback(
                    cfg.required_strategy, pf_seed.kind, pf_seed.exact_template, pf_seed.template_score, 0, 0, 0);
            }
            if (budget_ptr != nullptr && budget_ptr->aborted()) break;
        }
    }

    // Fallback dla zwykłego generatora jeśli wzorzec nie jest wymagany
    if (!solved_ok && !strict_exact_contract) {
        log_stage_begin("fallback_solve");
        solved_ok = solved.generate(topo, rng, candidate.solution, budget_ptr);
        log_stage_end("fallback_solve", solved_ok, budget_ptr);
    }
    
    if (collect_perf) {
        perf_out->solved_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - solved_t0).count());
    }

    if (!solved_ok) {
        if (budget_ptr != nullptr && budget_ptr->aborted()) {
            if (budget_ptr->aborted_by_pause) {
                reason = RejectReason::None;
                return false;
            }
            if (has_timed_out_ptr) *timed_out = budget_ptr->aborted_by_time || budget_ptr->aborted_by_nodes;
        }
        reason = RejectReason::Logic;
        return false;
    }
    
    // ------------------------------------------------------------------------
    // ETAP 2: Wykopywanie dziur w planszy i ocena przez Bottleneck Digger
    // ------------------------------------------------------------------------
    const auto dig_t0 = std::chrono::steady_clock::now();
    if (cfg.mcts_digger_enabled) {
        mcts_digger::GenericMctsBottleneckDigger mcts_digger;
        candidate.puzzle.resize(candidate.solution.size());
        
        log_stage_begin("dig");
        const bool dig_ok = mcts_digger.dig_into(
            std::span<const uint16_t>(candidate.solution.data(), candidate.solution.size()),
            topo,
            cfg,
            rng,
            uniq,
            logic,
            std::span<uint16_t>(candidate.puzzle.data(), candidate.puzzle.size()),
            candidate.clues,
            dig_protected_cells,
            dig_allowed_masks, dig_anchor_idx, dig_anchor_masks, dig_anchor_count, dig_exact_template,
            budget_ptr, &mcts_stats);
        log_stage_end(
            "dig",
            dig_ok,
            budget_ptr,
            "advanced_evals=" + std::to_string(mcts_stats.advanced_evals) +
            " reqA/U/H=" + std::to_string(mcts_stats.required_strategy_analyzed) + "/" +
                std::to_string(mcts_stats.required_strategy_uses) + "/" +
                std::to_string(mcts_stats.required_strategy_hits));
            
        if (!dig_ok) {
            pattern_forcing::note_template_attempt_feedback(
                cfg.required_strategy, dig_pattern_kind, dig_exact_template, dig_template_score,
                static_cast<int>(mcts_stats.required_strategy_analyzed),
                static_cast<int>(mcts_stats.required_strategy_uses),
                static_cast<int>(mcts_stats.required_strategy_hits));
            if (budget_ptr != nullptr && budget_ptr->aborted()) {
                if (budget_ptr->aborted_by_pause) {
                    reason = RejectReason::None;
                    return false;
                }
                if (has_timed_out_ptr) *timed_out = budget_ptr->aborted_by_time || budget_ptr->aborted_by_nodes;
            }
            reason = RejectReason::Logic;
            return false;
        }
    } else {
        // Fallback dla małych plansz lub gdy użytkownik prosi o brak MCTS
        // (Do dorzucenia np. standardowy random digger - tutaj uproszczony fallback na fail, jeśli wymagane)
        reason = RejectReason::Logic;
        return false;
    }
    
    if (collect_perf) {
        perf_out->dig_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - dig_t0).count());
        perf_out->mcts_advanced_evals += static_cast<uint64_t>(std::max(0, mcts_stats.advanced_evals));
        perf_out->mcts_required_strategy_analyzed += static_cast<uint64_t>(std::max(0, mcts_stats.required_strategy_analyzed));
        perf_out->mcts_required_strategy_use += static_cast<uint64_t>(std::max(0, mcts_stats.required_strategy_uses));
        perf_out->mcts_required_strategy_hit += static_cast<uint64_t>(std::max(0, mcts_stats.required_strategy_hits));
        perf_out->pattern_template_score = dig_template_score;
        perf_out->pattern_best_template_score = dig_best_template_score;
        perf_out->pattern_exact_template = dig_exact_template;
        perf_out->pattern_family_fallback_used = dig_family_fallback_used;
        perf_out->required_strategy_exact_contract_met = dig_exact_contract_met;
        perf_out->pattern_template_score_delta = dig_template_score_delta;
        perf_out->pattern_mutation_strength = dig_mutation_strength;
        perf_out->pattern_planner_zero_use_streak = dig_planner_zero_use_streak;
        perf_out->pattern_planner_failure_streak = dig_planner_failure_streak;
        perf_out->pattern_adaptive_target_strength = dig_adaptive_target_strength;
        perf_out->pattern_template_family = dig_pattern_kind;
        perf_out->pattern_generator_policy = dig_generator_policy;
        perf_out->pattern_mutation_source = dig_mutation_source;
    }

    strategy_info.family_fallback_used = dig_family_fallback_used;
    strategy_info.required_strategy_exact_contract_met = dig_exact_contract_met;

    auto note_pattern_feedback = [&]() {
        pattern_forcing::note_template_attempt_feedback(
            cfg.required_strategy,
            dig_pattern_kind,
            dig_exact_template,
            dig_template_score,
            static_cast<int>(mcts_stats.required_strategy_analyzed),
            static_cast<int>(mcts_stats.required_strategy_uses),
            static_cast<int>(mcts_stats.required_strategy_hits));
    };

    // ------------------------------------------------------------------------
    // ETAP 3: Quick Prefilter
    // ------------------------------------------------------------------------
    const auto prefilter_t0 = std::chrono::steady_clock::now();
    log_stage_begin("prefilter");
    const bool prefilter_ok = prefilter.check(candidate.puzzle, topo, cfg.min_clues, cfg.max_clues);
    log_stage_end("prefilter", prefilter_ok, nullptr, "clues=" + std::to_string(candidate.clues));
    if (collect_perf) {
        perf_out->prefilter_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - prefilter_t0).count());
    }
    if (!prefilter_ok) {
        log_pattern_contract("prefilter-reject", "clues=" + std::to_string(candidate.clues));
        log_strategy_contract("prefilter-reject", RejectReason::Prefilter, nullptr);
        note_pattern_feedback();
        reason = RejectReason::Prefilter;
        return false;
    }
    
    // ------------------------------------------------------------------------
    // ETAP 4: Weryfikacja Jakości i Symetrii (Quality Contract)
    // ------------------------------------------------------------------------
    post_processing::QualityMetrics quality_metrics{};
    if (need_quality_metrics) {
        quality_metrics = post_processing::evaluate_quality_metrics(candidate.puzzle, topo, cfg);
        if (has_quality_metrics_out) *quality_metrics_out = quality_metrics;
        
        if (has_quality_contract_out) {
            quality_contract_out->clue_range_ok = (candidate.clues >= cfg.min_clues && candidate.clues <= cfg.max_clues);
            quality_contract_out->symmetry_ok = quality_metrics.symmetry_ok;
            quality_contract_out->distribution_balance_ok = quality_metrics.distribution_balance_ok;
            quality_contract_out->givens_entropy_ok = quality_metrics.normalized_entropy >= quality_metrics.entropy_threshold;
        }

        if (quality_contract_enabled) {
            if (!quality_metrics.symmetry_ok) {
                note_pattern_feedback();
                reason = RejectReason::DistributionBias;
                return false;
            }
            if (distribution_filter_enabled) {
                if (!(quality_metrics.normalized_entropy >= quality_metrics.entropy_threshold) || 
                    !quality_metrics.distribution_balance_ok) {
                    note_pattern_feedback();
                    reason = RejectReason::DistributionBias;
                    return false;
                }
            }
        }
    }
    
    // ------------------------------------------------------------------------
    // ETAP 5: Certyfikacja Logiczna (Rozwiązanie) i Odrzucenie zbyt prostych
    // ------------------------------------------------------------------------
    const bool capture_logic_solution = replay_validation_enabled;
    const auto logic_t0 = std::chrono::steady_clock::now();
    
    // Wywołanie głównego silnika z ewaluacją wszystkich wymaganych strategii
    log_stage_begin("logic");
    const logic::GenericLogicCertifyResult logic_result = logic.certify(candidate.puzzle, topo, budget_ptr, capture_logic_solution);
    log_stage_end(
        "logic",
        !logic_result.timed_out,
        budget_ptr,
        "timed_out=" + std::string(logic_result.timed_out ? "1" : "0") +
        " solved=" + std::string(logic_result.solved ? "1" : "0") +
        " steps=" + std::to_string(std::max(0, logic_result.steps)));
    
    if (collect_perf) {
        perf_out->logic_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - logic_t0).count());
    }
    if (logic_result.timed_out) {
        note_pattern_feedback();
        if (budget_ptr != nullptr && budget_ptr->aborted_by_pause) {
            reason = RejectReason::None;
            return false;
        }
        if (has_timed_out_ptr) *timed_out = (budget_ptr == nullptr) ? true : (budget_ptr->aborted_by_time || budget_ptr->aborted_by_nodes);
        reason = RejectReason::Logic;
        return false;
    }
    
    if (collect_perf) {
        perf_out->logic_steps = static_cast<uint64_t>(std::max(0, logic_result.steps));
        perf_out->strategy_naked_use = logic_result.strategy_stats[logic::GenericLogicCertify::SlotNakedSingle].use_count;
        perf_out->strategy_naked_hit = logic_result.strategy_stats[logic::GenericLogicCertify::SlotNakedSingle].hit_count;
        perf_out->strategy_hidden_use = logic_result.strategy_stats[logic::GenericLogicCertify::SlotHiddenSingle].use_count;
        perf_out->strategy_hidden_hit = logic_result.strategy_stats[logic::GenericLogicCertify::SlotHiddenSingle].hit_count;
    }

    if (!cfg.fast_test_mode) {
        if (!evaluate_difficulty_contract_generic(logic_result, cfg.difficulty_level_required)) {
            log_pattern_contract("difficulty-reject", "clues=" + std::to_string(candidate.clues));
            log_strategy_contract("difficulty-reject", RejectReason::Strategy, &logic_result);
            note_pattern_feedback();
            reason = RejectReason::Strategy;
            return false;
        }
    }
    const bool contract_ok = evaluate_required_strategy_contract_generic(logic_result, cfg, cfg.required_strategy, strategy_info);
    if (collect_perf) {
        size_t required_slot = 0;
        const bool has_required_slot =
            logic::GenericLogicCertify::slot_from_required_strategy(cfg.required_strategy, required_slot);
        perf_out->certifier_required_strategy_analyzed = has_required_slot ? 1ULL : 0ULL;
        perf_out->certifier_required_strategy_use =
            has_required_slot ? logic_result.strategy_stats[required_slot].use_count : 0ULL;
        perf_out->certifier_required_strategy_hit =
            has_required_slot ? logic_result.strategy_stats[required_slot].hit_count : 0ULL;
    }
    if (cfg.required_strategy != RequiredStrategy::None && !contract_ok) {
        log_pattern_contract("strategy-reject", "clues=" + std::to_string(candidate.clues));
        log_strategy_contract("strategy-reject", RejectReason::Strategy, &logic_result);
        note_pattern_feedback();
        reason = RejectReason::Strategy;
        return false;
    }
    if (cfg.strict_logical && !logic_result.solved && cfg.required_strategy != RequiredStrategy::Backtracking) {
        note_pattern_feedback();
        reason = RejectReason::Logic;
        return false;
    }
    
    // ------------------------------------------------------------------------
    // ETAP 6: Gwarancja Unikalności przez algorytm Dancing Links X (DLX)
    // ------------------------------------------------------------------------
    bool uniqueness_ok = true;
    if (cfg.require_unique) {
        auto record_uniqueness_perf = [&](const SearchAbortControl& b, uint64_t elapsed_ns) {
            if (!collect_perf) return;
            ++perf_out->uniqueness_calls;
            perf_out->uniqueness_nodes += b.nodes;
            perf_out->uniqueness_elapsed_ns += elapsed_ns;
        };
        
        SearchAbortControl uniq_budget = budget;
        SearchAbortControl* uniq_budget_ptr = budget_enabled ? &uniq_budget : nullptr;
        
        const auto uniq_t0 = std::chrono::steady_clock::now();
        // Limitujemy wyjście DLX na poziomie 2, by nie przeszukiwać całej choinki rozwiązań.
        log_stage_begin("uniqueness");
        const int solutions = uniq.count_solutions_limit2(candidate.puzzle, topo, uniq_budget_ptr);
        const auto uniq_elapsed_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - uniq_t0).count());
        log_stage_end("uniqueness", solutions == 1, uniq_budget_ptr, "solutions=" + std::to_string(solutions));
            
        record_uniqueness_perf(uniq_budget, uniq_elapsed_ns);
        
        if (solutions < 0) {
            note_pattern_feedback();
            if (uniq_budget_ptr != nullptr && uniq_budget_ptr->aborted_by_pause) {
                reason = RejectReason::None;
                return false;
            }
            if (has_timed_out_ptr) *timed_out = true;
            reason = RejectReason::Logic;
            return false;
        }
        if (solutions != 1) {
            note_pattern_feedback();
            reason = RejectReason::Uniqueness;
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // ETAP 7: Finalny Post-Processing (Podpis kryptograficzny)
    // ------------------------------------------------------------------------
    post_processing::ReplayValidationResult replay{};
    if (replay_validation_enabled) {
        replay = post_processing::run_replay_validation(candidate.puzzle, candidate.solution, topo, logic);
        
        if (has_replay_out) *replay_out = replay;
        if (!replay.ok) {
            note_pattern_feedback();
            reason = RejectReason::Replay;
            return false;
        }
    }
    
    if (has_quality_contract_out) {
        quality_contract_out->is_unique = uniqueness_ok;
        quality_contract_out->logic_replay_ok = replay.ok || !replay_validation_enabled;
    }
    
    if (has_quality_contract_out && !post_processing::quality_contract_passed(*quality_contract_out, cfg)) {
        log_pattern_contract("quality-reject", "clues=" + std::to_string(candidate.clues));
        log_strategy_contract("quality-reject", RejectReason::DistributionBias, &logic_result);
        note_pattern_feedback();
        reason = RejectReason::DistributionBias;
        return false;
    }
    
    log_pattern_contract("accepted", "clues=" + std::to_string(candidate.clues));
    log_strategy_contract("accepted", RejectReason::None, &logic_result);
    note_pattern_feedback();
    reason = RejectReason::None;
    return true; // Sukces, plansza wygenerowana i obłożona wszelkimi certyfikatami.
}

} // namespace sudoku_hpc::generator
