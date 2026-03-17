//Author copyright Marcin Matysek (Rewertyn)
#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../config/run_config.h"
#include "../core/geometry.h"
#include "../monitor.h"
#include "../utils/logging.h"
#include "../generator/generator_facade.h"
#include "../generator/post_processing/vip_scoring.h"

namespace sudoku_hpc {

inline void accumulate_reject_reason(GenerateRunResult& r, RejectReason reason, bool timed_out) {
    if (timed_out) {
        ++r.reject_uniqueness_budget;
    }
    switch (reason) {
        case RejectReason::Prefilter: ++r.reject_prefilter; break;
        case RejectReason::Logic: ++r.reject_logic; break;
        case RejectReason::Uniqueness: ++r.reject_uniqueness; break;
        case RejectReason::Strategy: ++r.reject_strategy; break;
        case RejectReason::ExactNoProgress: ++r.reject_exact_no_progress; break;
        case RejectReason::Replay: ++r.reject_replay; break;
        case RejectReason::DistributionBias: ++r.reject_distribution_bias; break;
        case RejectReason::UniquenessBudget: ++r.reject_uniqueness_budget; break;
        case RejectReason::None: break;
    }
}

inline void accumulate_reject_reason_atomic(
    RejectReason reason,
    bool timed_out,
    std::atomic<uint64_t>& rejected,
    std::atomic<uint64_t>& reject_prefilter,
    std::atomic<uint64_t>& reject_logic,
    std::atomic<uint64_t>& reject_uniqueness,
    std::atomic<uint64_t>& reject_strategy,
    std::atomic<uint64_t>& reject_exact_no_progress,
    std::atomic<uint64_t>& reject_replay,
    std::atomic<uint64_t>& reject_distribution_bias,
    std::atomic<uint64_t>& reject_uniqueness_budget) {
    rejected.fetch_add(1, std::memory_order_relaxed);
    if (timed_out) {
        reject_uniqueness_budget.fetch_add(1, std::memory_order_relaxed);
    }
    switch (reason) {
        case RejectReason::Prefilter: reject_prefilter.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::Logic: reject_logic.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::Uniqueness: reject_uniqueness.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::Strategy: reject_strategy.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::ExactNoProgress: reject_exact_no_progress.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::Replay: reject_replay.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::DistributionBias: reject_distribution_bias.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::UniquenessBudget: reject_uniqueness_budget.fetch_add(1, std::memory_order_relaxed); break;
        case RejectReason::None: break;
    }
}

inline const char* reject_reason_label(RejectReason reason) {
    switch (reason) {
        case RejectReason::None: return "none";
        case RejectReason::Prefilter: return "prefilter";
        case RejectReason::Logic: return "logic";
        case RejectReason::Uniqueness: return "uniqueness";
        case RejectReason::Strategy: return "strategy";
        case RejectReason::ExactNoProgress: return "exact_no_progress";
        case RejectReason::Replay: return "replay";
        case RejectReason::DistributionBias: return "distribution_bias";
        case RejectReason::UniquenessBudget: return "uniqueness_budget";
    }
    return "unknown";
}

inline std::string cfg_diag_label(const GenerateRunConfig& cfg) {
    return
        "geom=" + std::to_string(cfg.box_rows) + "x" + std::to_string(cfg.box_cols) +
        " difficulty=" + std::to_string(cfg.difficulty_level_required) +
        " required=" + to_string(cfg.required_strategy) +
        " threads=" + std::to_string(cfg.threads) +
        " target=" + std::to_string(cfg.target_puzzles) +
        " fast_test=" + std::string(cfg.fast_test_mode ? "1" : "0") +
        " pattern_forcing=" + std::string(cfg.pattern_forcing_enabled ? "1" : "0") +
        " mcts=" + std::string(cfg.mcts_digger_enabled ? "1" : "0") +
        " max_attempts=" + std::to_string(cfg.max_attempts) +
        " max_total_time_s=" + std::to_string(cfg.max_total_time_s) +
        " attempt_time_budget_s=" + std::to_string(cfg.attempt_time_budget_s) +
        " attempt_node_budget=" + std::to_string(cfg.attempt_node_budget) +
        " reseed_interval_s=" + std::to_string(cfg.reseed_interval_s) +
        " force_new_seed_per_attempt=" + std::string(cfg.force_new_seed_per_attempt ? "1" : "0");
}

inline uint64_t splitmix64_next(uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline std::string detect_measurement_profile(const GenerateRunConfig& cfg) {
    if (cfg.fast_test_mode && cfg.difficulty_level_required >= 8) {
        return "relaxed-clue-fast-test";
    }
    return "strict-contract";
}

inline bool should_trace_attempt_diag(
    const GenerateRunConfig& cfg,
    uint64_t local_attempts,
    bool ok,
    RejectReason reason,
    bool timed_out) {
    if (local_attempts <= 3) return true;
    if (ok) return cfg.required_strategy != RequiredStrategy::None;
    if (timed_out) return true;
    return reason == RejectReason::Logic ||
           reason == RejectReason::Strategy ||
           reason == RejectReason::ExactNoProgress ||
           reason == RejectReason::Replay ||
           reason == RejectReason::DistributionBias;
}

inline GenerateRunResult run_generic_sudoku(
    const GenerateRunConfig& cfg,
    ConsoleStatsMonitor* monitor = nullptr,
    std::atomic<bool>* cancel_flag = nullptr,
    std::atomic<bool>* pause_flag = nullptr,
    std::function<void(uint64_t, uint64_t)> on_progress = nullptr,
    std::function<void(const std::string&)> on_log = nullptr) {

    using namespace std::chrono;

    GenerateRunResult result{};
    result.cpu_backend_selected = cfg.cpu_backend;
    result.measurement_profile = detect_measurement_profile(cfg);

    GenericTopology topo;
    std::string topo_err;
    if (!build_generic_topology(cfg.box_rows, cfg.box_cols, topo, &topo_err)) {
        log_error("runner", "invalid geometry: " + topo_err);
        if (on_log) on_log("invalid geometry: " + topo_err);
        result.reject_logic = 1;
        result.rejected = 1;
        return result;
    }

    const int n = topo.n;
    const int nn = topo.nn;
    GenerateRunConfig run_cfg = cfg;
    const bool auto_clue_range_requested =
        (run_cfg.min_clues <= 0 || run_cfg.max_clues <= 0 || run_cfg.max_clues < run_cfg.min_clues);
    const std::string measurement_profile = detect_measurement_profile(run_cfg);

    const AutoClueWindowPolicy effective_policy = run_cfg.split_auto_clue_policy
        ? AutoClueWindowPolicy::Generator
        : AutoClueWindowPolicy::Shared;

    ClueRange shared_auto_range =
        resolve_auto_clue_range(run_cfg.box_rows, run_cfg.box_cols, run_cfg.difficulty_level_required, run_cfg.required_strategy, AutoClueWindowPolicy::Shared);
    ClueRange generator_auto_range =
        resolve_auto_clue_range(run_cfg.box_rows, run_cfg.box_cols, run_cfg.difficulty_level_required, run_cfg.required_strategy, effective_policy);
    ClueRange certifier_auto_range =
        resolve_auto_clue_range(run_cfg.box_rows, run_cfg.box_cols, run_cfg.difficulty_level_required, run_cfg.required_strategy,
            run_cfg.split_auto_clue_policy ? AutoClueWindowPolicy::Certifier : AutoClueWindowPolicy::Shared);

    if (run_cfg.min_clues <= 0 || run_cfg.max_clues <= 0 || run_cfg.max_clues < run_cfg.min_clues) {
        if (run_cfg.min_clues <= 0) run_cfg.min_clues = generator_auto_range.min_clues;
        if (run_cfg.max_clues <= 0) run_cfg.max_clues = generator_auto_range.max_clues;
        if (run_cfg.max_clues < run_cfg.min_clues) run_cfg.max_clues = run_cfg.min_clues;
    }
    if (auto_clue_range_requested && run_cfg.difficulty_level_required >= 9) {
        const int relaxed_min = std::max(run_cfg.min_clues, std::max(4, n));
        const int relaxed_max = std::max(relaxed_min, static_cast<int>(0.70 * static_cast<double>(nn)));
        run_cfg.min_clues = relaxed_min;
        run_cfg.max_clues = relaxed_max;
        generator_auto_range.min_clues = std::max(generator_auto_range.min_clues, relaxed_min);
        generator_auto_range.max_clues = std::max(generator_auto_range.max_clues, relaxed_max);
        if (generator_auto_range.max_clues < generator_auto_range.min_clues) generator_auto_range.max_clues = generator_auto_range.min_clues;
    }
    run_cfg.min_clues = std::clamp(run_cfg.min_clues, 0, nn);
    run_cfg.max_clues = std::clamp(run_cfg.max_clues, run_cfg.min_clues, nn);
    shared_auto_range.min_clues = std::clamp(shared_auto_range.min_clues, 0, nn);
    shared_auto_range.max_clues = std::clamp(shared_auto_range.max_clues, shared_auto_range.min_clues, nn);
    generator_auto_range.min_clues = std::clamp(generator_auto_range.min_clues, 0, nn);
    generator_auto_range.max_clues = std::clamp(generator_auto_range.max_clues, generator_auto_range.min_clues, nn);
    certifier_auto_range.min_clues = std::clamp(certifier_auto_range.min_clues, 0, nn);
    certifier_auto_range.max_clues = std::clamp(certifier_auto_range.max_clues, certifier_auto_range.min_clues, nn);

    result.effective_min_clues = run_cfg.min_clues;
    result.effective_max_clues = run_cfg.max_clues;
    result.effective_shared_min_clues = shared_auto_range.min_clues;
    result.effective_shared_max_clues = shared_auto_range.max_clues;
    result.effective_generator_min_clues = generator_auto_range.min_clues;
    result.effective_generator_max_clues = generator_auto_range.max_clues;
    result.effective_certifier_min_clues = certifier_auto_range.min_clues;
    result.effective_certifier_max_clues = certifier_auto_range.max_clues;

    const bool wants_p7_plus = run_cfg.difficulty_level_required >= 7;
    const bool wants_p8 = run_cfg.difficulty_level_required >= 8;
    const bool is_large_geometry = (n >= 16);
    const bool is_16x16_geometry = (n == 16);
    const bool is_25x25_plus_geometry = (n >= 25);
    const bool is_36x36_plus_geometry = (n >= 36);
    const bool is_25x25_plus_p8_geometry = is_25x25_plus_geometry && wants_p8;

    if (!run_cfg.fast_test_mode) {
        if (run_cfg.attempt_node_budget == 0) {
            const uint64_t suggested = suggest_attempt_node_budget(
                run_cfg.box_rows,
                run_cfg.box_cols,
                std::max(1, run_cfg.difficulty_level_required));
            run_cfg.attempt_node_budget = std::max<uint64_t>(30'000ULL, suggested / (wants_p7_plus ? 6ULL : 4ULL));
        }

        if (run_cfg.difficulty_level_required >= 6 && !run_cfg.pattern_forcing_enabled) {
            run_cfg.pattern_forcing_enabled = true;
        }
        if (run_cfg.pattern_forcing_enabled) {
            run_cfg.pattern_forcing_tries = std::max(run_cfg.pattern_forcing_tries, wants_p8 ? 12 : (wants_p7_plus ? 8 : 6));
            if (run_cfg.pattern_forcing_anchor_count <= 0) {
                run_cfg.pattern_forcing_anchor_count = wants_p8 ? 8 : (wants_p7_plus ? 6 : 4);
            }
        }
        if (run_cfg.mcts_tuning_profile == "auto") {
            run_cfg.mcts_tuning_profile = wants_p8 ? "p8" : (wants_p7_plus ? "p7" : "auto");
        }
    }

    if (run_cfg.fast_test_mode) {
        // Fast smoke profile: bounded runtime and relaxed heavy verification stages.
        run_cfg.enable_quality_contract = false;
        run_cfg.enable_distribution_filter = false;
        run_cfg.enable_replay_validation = false;
        run_cfg.require_unique = false;
        run_cfg.strict_logical = false;
        run_cfg.strict_canonical_strategies = false;
        run_cfg.allow_proxy_advanced = true;

        if (run_cfg.max_attempts == 0) {
            if (is_25x25_plus_p8_geometry) {
                run_cfg.max_attempts = is_36x36_plus_geometry
                    ? std::max<uint64_t>(128ULL, run_cfg.target_puzzles * 128ULL)
                    : std::max<uint64_t>(96ULL, run_cfg.target_puzzles * 96ULL);
            } else if (is_36x36_plus_geometry) {
                run_cfg.max_attempts = std::max<uint64_t>(96ULL, run_cfg.target_puzzles * 96ULL);
            } else if (is_large_geometry) {
                run_cfg.max_attempts = std::max<uint64_t>(64ULL, run_cfg.target_puzzles * 64ULL);
            } else {
                run_cfg.max_attempts = std::max<uint64_t>(32ULL, run_cfg.target_puzzles * 32ULL);
            }
        }
        if (run_cfg.max_total_time_s == 0) {
            if (is_25x25_plus_p8_geometry) {
                run_cfg.max_total_time_s = is_36x36_plus_geometry ? 40ULL : 30ULL;
            } else if (is_36x36_plus_geometry) {
                run_cfg.max_total_time_s = 32ULL;
            } else if (is_25x25_plus_geometry) {
                run_cfg.max_total_time_s = 24ULL;
            } else if (is_large_geometry) {
                run_cfg.max_total_time_s = 16ULL;
            } else {
                run_cfg.max_total_time_s = 20ULL;
            }
        }
        if (run_cfg.attempt_time_budget_s <= 0.0) {
            if (is_25x25_plus_p8_geometry) {
                run_cfg.attempt_time_budget_s = is_36x36_plus_geometry ? 10.0 : 8.0;
            } else if (is_36x36_plus_geometry) {
                run_cfg.attempt_time_budget_s = 8.0;
            } else if (is_25x25_plus_geometry) {
                run_cfg.attempt_time_budget_s = 6.0;
            } else if (is_large_geometry) {
                run_cfg.attempt_time_budget_s = 5.0;
            } else {
                run_cfg.attempt_time_budget_s = (run_cfg.difficulty_level_required >= 7) ? 1.2 : 0.7;
            }
        }
        if (run_cfg.attempt_node_budget == 0) {
            if (is_large_geometry) {
                const uint64_t suggested = suggest_attempt_node_budget(
                    run_cfg.box_rows,
                    run_cfg.box_cols,
                    std::max(1, run_cfg.difficulty_level_required));
                if (is_25x25_plus_p8_geometry) {
                    run_cfg.attempt_node_budget = is_36x36_plus_geometry
                        ? std::clamp<uint64_t>(
                            std::max<uint64_t>(900'000ULL, suggested / 2ULL),
                            900'000ULL,
                            3'000'000ULL)
                        : std::clamp<uint64_t>(
                            std::max<uint64_t>(600'000ULL, suggested / 3ULL),
                            600'000ULL,
                            2'000'000ULL);
                } else if (is_36x36_plus_geometry) {
                    run_cfg.attempt_node_budget = std::clamp<uint64_t>(
                        std::max<uint64_t>(500'000ULL, suggested / 4ULL),
                        500'000ULL,
                        2'000'000ULL);
                } else if (is_25x25_plus_geometry) {
                    run_cfg.attempt_node_budget = std::clamp<uint64_t>(
                        std::max<uint64_t>(350'000ULL, suggested / 5ULL),
                        350'000ULL,
                        1'500'000ULL);
                } else {
                    run_cfg.attempt_node_budget = 250'000ULL;
                }
            } else {
                const uint64_t suggested = suggest_attempt_node_budget(
                    run_cfg.box_rows,
                    run_cfg.box_cols,
                    std::max(1, run_cfg.difficulty_level_required));
                run_cfg.attempt_node_budget = std::max<uint64_t>(20'000ULL, suggested / 8ULL);
            }
        }

        if (is_25x25_plus_p8_geometry) {
            if (!run_cfg.pattern_forcing_enabled) {
                run_cfg.pattern_forcing_enabled = true;
            }
            run_cfg.pattern_forcing_tries = std::max(run_cfg.pattern_forcing_tries, is_36x36_plus_geometry ? 24 : 18);
            if (run_cfg.pattern_forcing_anchor_count <= 0) {
                run_cfg.pattern_forcing_anchor_count = is_36x36_plus_geometry ? 18 : 14;
            }
            run_cfg.pattern_forcing_lock_anchors = true;
            if (run_cfg.mcts_tuning_profile == "auto") {
                run_cfg.mcts_tuning_profile = "p8";
            }
        }
    }

    if (run_cfg.attempt_time_budget_s <= 0.0) {
        run_cfg.attempt_time_budget_s = 0.0;
    }

    const int hw = std::max(1u, std::thread::hardware_concurrency());
    const int worker_count = std::max(1, run_cfg.threads <= 0 ? hw : run_cfg.threads);

    std::filesystem::create_directories(run_cfg.output_folder);
    const std::filesystem::path output_path = std::filesystem::path(run_cfg.output_folder) / run_cfg.output_file;
    std::ofstream batch_out(output_path, std::ios::out | std::ios::app);
    if (!batch_out) {
        log_error("runner", "cannot open output file: " + output_path.string());
        if (on_log) on_log("cannot open output file: " + output_path.string());
        result.reject_logic = 1;
        result.rejected = 1;
        return result;
    }

    if (monitor != nullptr) {
        monitor->set_target(run_cfg.target_puzzles);
        monitor->set_active_workers(worker_count);
        monitor->set_grid_info(run_cfg.box_rows, run_cfg.box_cols, run_cfg.difficulty_level_required);
        monitor->set_background_status("runtime initialized");
    }

    log_info(
        "runner",
        "config " + cfg_diag_label(run_cfg) +
        " measurement_profile=" + measurement_profile +
        " clue_range=" + std::to_string(run_cfg.min_clues) + "-" + std::to_string(run_cfg.max_clues) +
        " goldilocks_shared=" + std::to_string(shared_auto_range.min_clues) + "-" + std::to_string(shared_auto_range.max_clues) +
        " goldilocks_generator=" + std::to_string(generator_auto_range.min_clues) + "-" + std::to_string(generator_auto_range.max_clues) +
        " goldilocks_certifier=" + std::to_string(certifier_auto_range.min_clues) + "-" + std::to_string(certifier_auto_range.max_clues));

    std::mutex write_mu;

    std::atomic<uint64_t> accepted{0};
    std::atomic<uint64_t> written{0};
    std::atomic<uint64_t> attempts{0};
    std::atomic<uint64_t> rejected{0};
    std::atomic<uint64_t> reject_prefilter{0};
    std::atomic<uint64_t> reject_logic{0};
    std::atomic<uint64_t> reject_uniqueness{0};
    std::atomic<uint64_t> reject_strategy{0};
    std::atomic<uint64_t> reject_exact_no_progress{0};
    std::atomic<uint64_t> reject_replay{0};
    std::atomic<uint64_t> reject_distribution_bias{0};
    std::atomic<uint64_t> reject_uniqueness_budget{0};

    std::atomic<uint64_t> uniqueness_calls{0};
    std::atomic<uint64_t> uniqueness_nodes{0};
    std::atomic<uint64_t> uniqueness_elapsed_ns{0};
    std::atomic<uint64_t> logic_steps_total{0};
    std::atomic<uint64_t> strategy_naked_use{0};
    std::atomic<uint64_t> strategy_naked_hit{0};
    std::atomic<uint64_t> strategy_hidden_use{0};
    std::atomic<uint64_t> strategy_hidden_hit{0};
    std::atomic<uint64_t> mcts_advanced_evals{0};
    std::atomic<uint64_t> certifier_required_strategy_analyzed{0};
    std::atomic<uint64_t> certifier_required_slot_entered{0};
    std::atomic<uint64_t> certifier_required_strategy_use{0};
    std::atomic<uint64_t> certifier_required_strategy_hit{0};
    std::atomic<uint64_t> required_strategy_certified_exact{0};
    std::atomic<uint64_t> mcts_required_strategy_analyzed{0};
    std::atomic<uint64_t> mcts_required_strategy_use{0};
    std::atomic<uint64_t> mcts_required_strategy_hit{0};
    std::atomic<uint64_t> pattern_exact_template_used{0};
    std::atomic<uint64_t> pattern_family_fallback_used{0};
    std::atomic<uint64_t> required_strategy_exact_contract_met{0};
    std::atomic<uint64_t> required_zero_use_streak_max{0};
    std::atomic<int> best_template_score{0};
    std::atomic<uint64_t> kernel_elapsed_ns{0};
    std::atomic<uint64_t> kernel_calls{0};

    const auto t0 = steady_clock::now();

    auto is_cancelled = [&]() -> bool {
        return (cancel_flag != nullptr) && cancel_flag->load(std::memory_order_relaxed);
    };

    auto is_paused = [&]() -> bool {
        return (pause_flag != nullptr) && pause_flag->load(std::memory_order_relaxed);
    };

    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(worker_count));

    for (int worker_idx = 0; worker_idx < worker_count; ++worker_idx) {
        workers.emplace_back([&, worker_idx]() {
            uint64_t local_attempts = 0;
            uint64_t local_written = 0;
            uint64_t local_required_analyzed = 0;
            uint64_t local_required_use = 0;
            uint64_t local_required_hit = 0;
            uint64_t local_required_zero_use_streak = 0;
            uint64_t local_required_zero_use_streak_max = 0;
            int local_best_template_score = 0;
            int local_last_template_score_delta = 0;
            int local_last_mutation_strength = 0;
            int local_planner_zero_use_streak = 0;
            int local_planner_failure_streak = 0;
            int local_adaptive_target_strength = 0;
            pattern_forcing::PatternKind local_template_family = pattern_forcing::PatternKind::None;
            pattern_forcing::PatternMutationSource local_mutation_source = pattern_forcing::PatternMutationSource::Random;
            uint64_t current_attempt_seed = 0;

            try {
                const uint64_t base_seed = (run_cfg.seed == 0)
                    ? static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                    : run_cfg.seed;
                uint64_t worker_seed_state =
                    base_seed ^
                    (0x9E3779B97F4A7C15ULL + static_cast<uint64_t>(worker_idx) * 0x100000001B3ULL);
                current_attempt_seed = splitmix64_next(worker_seed_state);
                std::mt19937_64 rng(current_attempt_seed);
                auto last_reseed_tp = steady_clock::now();

                core_engines::GenericSolvedKernel solved(
                    core_engines::GenericSolvedKernel::backend_from_string(run_cfg.cpu_backend));
                core_engines::GenericQuickPrefilter prefilter;
                logic::GenericLogicCertify logic;
                core_engines::GenericUniquenessCounter uniq;

                log_info(
                    "runner.worker.start",
                    "worker=" + std::to_string(worker_idx) +
                    " base_seed=" + std::to_string(base_seed) +
                    " initial_attempt_seed=" + std::to_string(current_attempt_seed) +
                    " " + cfg_diag_label(run_cfg));

                while (true) {
                if (is_cancelled()) {
                    break;
                }

                if (run_cfg.max_total_time_s > 0) {
                    const auto elapsed = duration_cast<seconds>(steady_clock::now() - t0).count();
                    if (elapsed >= static_cast<long long>(run_cfg.max_total_time_s)) {
                        break;
                    }
                }

                while (is_paused() && !is_cancelled()) {
                    std::this_thread::sleep_for(milliseconds(20));
                }

                const uint64_t current_accepted = accepted.load(std::memory_order_relaxed);
                if (current_accepted >= run_cfg.target_puzzles) {
                    break;
                }

                if (run_cfg.max_attempts > 0) {
                    const uint64_t current_attempts = attempts.load(std::memory_order_relaxed);
                    if (current_attempts >= run_cfg.max_attempts) {
                        break;
                    }
                }

                ++local_attempts;
                attempts.fetch_add(1, std::memory_order_relaxed);

                const auto now_tp = steady_clock::now();
                bool reseeded = false;
                if (run_cfg.force_new_seed_per_attempt) {
                    current_attempt_seed = splitmix64_next(worker_seed_state);
                    rng.seed(current_attempt_seed);
                    last_reseed_tp = now_tp;
                    reseeded = true;
                } else if (run_cfg.reseed_interval_s > 0 &&
                           duration_cast<seconds>(now_tp - last_reseed_tp).count() >=
                               static_cast<long long>(run_cfg.reseed_interval_s)) {
                    current_attempt_seed = splitmix64_next(worker_seed_state);
                    rng.seed(current_attempt_seed);
                    last_reseed_tp = now_tp;
                    reseeded = true;
                }
                if (reseeded && local_attempts <= 3) {
                    log_info(
                        "runner.worker.reseed",
                        "worker=" + std::to_string(worker_idx) +
                        " attempt=" + std::to_string(local_attempts) +
                        " seed=" + std::to_string(current_attempt_seed));
                }

                generator::GenericPuzzleCandidate candidate;
                RejectReason reason = RejectReason::None;
                RequiredStrategyAttemptInfo strategy_info{};
                generator::AttemptPerfStats perf{};
                bool timed_out = false;

                const bool ok = generator::generate_one_generic(
                    run_cfg,
                    topo,
                    rng,
                    candidate,
                    reason,
                    strategy_info,
                    solved,
                    prefilter,
                    logic,
                    uniq,
                    nullptr,
                    &timed_out,
                    cancel_flag,
                    pause_flag,
                    nullptr,
                    nullptr,
                    nullptr,
                    &perf);

                kernel_elapsed_ns.fetch_add(perf.solved_elapsed_ns + perf.dig_elapsed_ns, std::memory_order_relaxed);
                kernel_calls.fetch_add(1, std::memory_order_relaxed);

                uniqueness_calls.fetch_add(perf.uniqueness_calls, std::memory_order_relaxed);
                uniqueness_nodes.fetch_add(perf.uniqueness_nodes, std::memory_order_relaxed);
                uniqueness_elapsed_ns.fetch_add(perf.uniqueness_elapsed_ns, std::memory_order_relaxed);
                logic_steps_total.fetch_add(perf.logic_steps, std::memory_order_relaxed);
                strategy_naked_use.fetch_add(perf.strategy_naked_use, std::memory_order_relaxed);
                strategy_naked_hit.fetch_add(perf.strategy_naked_hit, std::memory_order_relaxed);
                strategy_hidden_use.fetch_add(perf.strategy_hidden_use, std::memory_order_relaxed);
                strategy_hidden_hit.fetch_add(perf.strategy_hidden_hit, std::memory_order_relaxed);
                mcts_advanced_evals.fetch_add(perf.mcts_advanced_evals, std::memory_order_relaxed);
                certifier_required_strategy_analyzed.fetch_add(perf.certifier_required_strategy_analyzed, std::memory_order_relaxed);
                certifier_required_slot_entered.fetch_add(perf.certifier_required_slot_entered, std::memory_order_relaxed);
                certifier_required_strategy_use.fetch_add(perf.certifier_required_strategy_use, std::memory_order_relaxed);
                certifier_required_strategy_hit.fetch_add(perf.certifier_required_strategy_hit, std::memory_order_relaxed);
                required_strategy_certified_exact.fetch_add(perf.required_strategy_certified_exact, std::memory_order_relaxed);
                mcts_required_strategy_analyzed.fetch_add(perf.mcts_required_strategy_analyzed, std::memory_order_relaxed);
                mcts_required_strategy_use.fetch_add(perf.mcts_required_strategy_use, std::memory_order_relaxed);
                mcts_required_strategy_hit.fetch_add(perf.mcts_required_strategy_hit, std::memory_order_relaxed);
                if (perf.pattern_exact_template) {
                    pattern_exact_template_used.fetch_add(1, std::memory_order_relaxed);
                }
                if (perf.pattern_family_fallback_used) {
                    pattern_family_fallback_used.fetch_add(1, std::memory_order_relaxed);
                }
                if (perf.required_strategy_exact_contract_met) {
                    required_strategy_exact_contract_met.fetch_add(1, std::memory_order_relaxed);
                }
                local_required_analyzed += perf.mcts_required_strategy_analyzed;
                local_required_use += perf.mcts_required_strategy_use;
                local_required_hit += perf.mcts_required_strategy_hit;
                local_best_template_score = std::max(local_best_template_score, perf.pattern_best_template_score);
                local_last_template_score_delta = perf.pattern_template_score_delta;
                local_last_mutation_strength = perf.pattern_mutation_strength;
                local_planner_zero_use_streak = perf.pattern_planner_zero_use_streak;
                local_planner_failure_streak = perf.pattern_planner_failure_streak;
                local_adaptive_target_strength = perf.pattern_adaptive_target_strength;
                local_template_family = perf.pattern_template_family;
                local_mutation_source = perf.pattern_mutation_source;
                if (perf.mcts_required_strategy_analyzed > 0 && perf.mcts_required_strategy_use == 0) {
                    ++local_required_zero_use_streak;
                    local_required_zero_use_streak_max = std::max(local_required_zero_use_streak_max, local_required_zero_use_streak);
                } else if (perf.mcts_required_strategy_use > 0) {
                    local_required_zero_use_streak = 0;
                }
                {
                    uint64_t prev = required_zero_use_streak_max.load(std::memory_order_relaxed);
                    while (prev < local_required_zero_use_streak_max &&
                           !required_zero_use_streak_max.compare_exchange_weak(
                               prev, local_required_zero_use_streak_max,
                               std::memory_order_relaxed, std::memory_order_relaxed)) {}
                }
                {
                    int prev = best_template_score.load(std::memory_order_relaxed);
                    while (prev < local_best_template_score &&
                           !best_template_score.compare_exchange_weak(
                               prev, local_best_template_score,
                               std::memory_order_relaxed, std::memory_order_relaxed)) {}
                }

                if (ok) {
                    uint64_t accepted_idx = 0;
                    bool slot_acquired = false;
                    while (true) {
                        uint64_t cur = accepted.load(std::memory_order_relaxed);
                        if (cur >= run_cfg.target_puzzles) {
                            slot_acquired = false;
                            break;
                        }
                        if (accepted.compare_exchange_weak(cur, cur + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                            accepted_idx = cur + 1;
                            slot_acquired = true;
                            break;
                        }
                    }
                    if (!slot_acquired) {
                        continue;
                    }

                    const std::string line = generator::serialize_line_generic(
                        current_attempt_seed,
                        run_cfg,
                        candidate,
                        topo.nn);

                    {
                        std::lock_guard<std::mutex> lock(write_mu);
                        batch_out << line << '\n';
                        if (run_cfg.write_individual_files) {
                            const std::filesystem::path file_path =
                                std::filesystem::path(run_cfg.output_folder) /
                                ("sudoku_" + std::to_string(accepted_idx) + ".txt");
                            std::ofstream one(file_path, std::ios::out | std::ios::trunc);
                            if (one) {
                                one << line << '\n';
                            }
                        }
                    }

                    ++local_written;
                    written.fetch_add(1, std::memory_order_relaxed);

                    if (on_progress) {
                        on_progress(accepted_idx, run_cfg.target_puzzles);
                    }

                    if (on_log && (accepted_idx % 10ULL == 0ULL || accepted_idx == run_cfg.target_puzzles)) {
                        log_info(
                            "runner.worker.accept_cb",
                            "worker=" + std::to_string(worker_idx) +
                            " accepted_idx=" + std::to_string(accepted_idx) +
                            " phase=begin");
                        on_log("accepted=" + std::to_string(accepted_idx) + "/" + std::to_string(run_cfg.target_puzzles));
                        log_info(
                            "runner.worker.accept_cb",
                            "worker=" + std::to_string(worker_idx) +
                            " accepted_idx=" + std::to_string(accepted_idx) +
                            " phase=end");
                    }

                    if (should_trace_attempt_diag(run_cfg, local_attempts, true, reason, timed_out)) {
                        log_info(
                            "runner.worker.accept",
                            "worker=" + std::to_string(worker_idx) +
                            " attempt=" + std::to_string(local_attempts) +
                            " seed=" + std::to_string(current_attempt_seed) +
                            " accepted_idx=" + std::to_string(accepted_idx) +
                            " clues=" + std::to_string(candidate.clues) +
                            " reqA/U/H=" + std::to_string(perf.mcts_required_strategy_analyzed) + "/" +
                                std::to_string(perf.mcts_required_strategy_use) + "/" +
                                std::to_string(perf.mcts_required_strategy_hit) +
                            " solved_ms=" + std::to_string(static_cast<double>(perf.solved_elapsed_ns) / 1e6) +
                            " dig_ms=" + std::to_string(static_cast<double>(perf.dig_elapsed_ns) / 1e6) +
                            " logic_ms=" + std::to_string(static_cast<double>(perf.logic_elapsed_ns) / 1e6) +
                            " uniq_ms=" + std::to_string(static_cast<double>(perf.uniqueness_elapsed_ns) / 1e6) +
                            " template_family=" + std::string(pattern_forcing::pattern_kind_label(local_template_family)) +
                            " generator_policy=" + std::string(to_string(perf.pattern_generator_policy)) +
                            " exact_contract=" + std::string(perf.required_strategy_exact_contract_met ? "1" : "0") +
                            " family_fallback=" + std::string(perf.pattern_family_fallback_used ? "1" : "0") +
                            " mutation_source=" + std::string(pattern_forcing::pattern_mutation_source_label(local_mutation_source)) +
                            " template_score=" + std::to_string(perf.pattern_template_score) +
                            " best_template_score=" + std::to_string(perf.pattern_best_template_score));
                    }
                } else {
                    accumulate_reject_reason_atomic(
                        reason,
                        timed_out,
                        rejected,
                        reject_prefilter,
                        reject_logic,
                        reject_uniqueness,
                        reject_strategy,
                        reject_exact_no_progress,
                        reject_replay,
                        reject_distribution_bias,
                        reject_uniqueness_budget);

                    if (should_trace_attempt_diag(run_cfg, local_attempts, false, reason, timed_out)) {
                        log_warn(
                            "runner.worker.reject",
                            "worker=" + std::to_string(worker_idx) +
                            " attempt=" + std::to_string(local_attempts) +
                            " seed=" + std::to_string(current_attempt_seed) +
                            " reason=" + std::string(reject_reason_label(reason)) +
                            " timed_out=" + std::string(timed_out ? "1" : "0") +
                            " matched_required=" + std::string(strategy_info.matched_required_strategy ? "1" : "0") +
                            " slot_entered=" + std::string(strategy_info.required_slot_entered ? "1" : "0") +
                            " certified_exact=" + std::string(strategy_info.required_strategy_certified_exact ? "1" : "0") +
                            " exact_contract=" + std::string(strategy_info.required_strategy_exact_contract_met ? "1" : "0") +
                            " exact_no_progress=" + std::string(strategy_info.exact_no_progress ? "1" : "0") +
                            " family_fallback=" + std::string(strategy_info.family_fallback_used ? "1" : "0") +
                            " req_confirm_use=" + std::string(strategy_info.required_strategy_use_confirmed ? "1" : "0") +
                            " req_confirm_hit=" + std::string(strategy_info.required_strategy_hit_confirmed ? "1" : "0") +
                            " reqA/U/H=" + std::to_string(perf.mcts_required_strategy_analyzed) + "/" +
                                std::to_string(perf.mcts_required_strategy_use) + "/" +
                                std::to_string(perf.mcts_required_strategy_hit) +
                            " solved_ms=" + std::to_string(static_cast<double>(perf.solved_elapsed_ns) / 1e6) +
                            " dig_ms=" + std::to_string(static_cast<double>(perf.dig_elapsed_ns) / 1e6) +
                            " prefilter_ms=" + std::to_string(static_cast<double>(perf.prefilter_elapsed_ns) / 1e6) +
                            " logic_ms=" + std::to_string(static_cast<double>(perf.logic_elapsed_ns) / 1e6) +
                            " uniq_ms=" + std::to_string(static_cast<double>(perf.uniqueness_elapsed_ns) / 1e6) +
                            " template_family=" + std::string(pattern_forcing::pattern_kind_label(local_template_family)) +
                            " generator_policy=" + std::string(to_string(perf.pattern_generator_policy)) +
                            " mutation_source=" + std::string(pattern_forcing::pattern_mutation_source_label(local_mutation_source)) +
                            " template_score=" + std::to_string(perf.pattern_template_score) +
                            " best_template_score=" + std::to_string(perf.pattern_best_template_score) +
                            " template_score_delta=" + std::to_string(perf.pattern_template_score_delta) +
                            " mutation_strength=" + std::to_string(perf.pattern_mutation_strength));
                    }
                }

                if (monitor != nullptr && ((local_attempts % 16ULL) == 0ULL || local_written > 0)) {
                    monitor->set_attempts(attempts.load(std::memory_order_relaxed));
                    monitor->set_accepted(accepted.load(std::memory_order_relaxed));
                    monitor->set_written(written.load(std::memory_order_relaxed));
                    monitor->set_rejected(rejected.load(std::memory_order_relaxed));
                    monitor->set_analyzed_required_strategy(mcts_required_strategy_analyzed.load(std::memory_order_relaxed));
                    monitor->set_required_strategy_hits(mcts_required_strategy_hit.load(std::memory_order_relaxed));

                    WorkerRow row{};
                    row.worker = "worker_" + std::to_string(worker_idx);
                    row.clues = candidate.clues;
                    row.seed = current_attempt_seed;
                    row.applied = local_attempts;
                    row.status = is_paused() ? "paused" : "running";
                    row.reseed_interval_s = run_cfg.reseed_interval_s;
                    row.attempt_time_budget_s = run_cfg.attempt_time_budget_s;
                    row.attempt_node_budget = run_cfg.attempt_node_budget;
                    row.stage_solved_ms = static_cast<double>(perf.solved_elapsed_ns) / 1e6;
                    row.stage_dig_ms = static_cast<double>(perf.dig_elapsed_ns) / 1e6;
                    row.stage_prefilter_ms = static_cast<double>(perf.prefilter_elapsed_ns) / 1e6;
                    row.stage_logic_ms = static_cast<double>(perf.logic_elapsed_ns) / 1e6;
                    row.stage_uniqueness_ms = static_cast<double>(perf.uniqueness_elapsed_ns) / 1e6;
                    row.required_strategy_analyzed = local_required_analyzed;
                    row.required_strategy_use = local_required_use;
                    row.required_strategy_hit = local_required_hit;
                    monitor->set_worker_row(static_cast<size_t>(worker_idx), row);
                }
                }
            } catch (const std::exception& ex) {
                if (cancel_flag != nullptr) {
                    cancel_flag->store(true, std::memory_order_relaxed);
                }
                log_error(
                    "runner.worker.exception",
                    "worker=" + std::to_string(worker_idx) +
                    " attempt=" + std::to_string(local_attempts) +
                    " what=" + ex.what());
            } catch (...) {
                if (cancel_flag != nullptr) {
                    cancel_flag->store(true, std::memory_order_relaxed);
                }
                log_error(
                    "runner.worker.exception",
                    "worker=" + std::to_string(worker_idx) +
                    " attempt=" + std::to_string(local_attempts) +
                    " what=unknown");
            }

            if (monitor != nullptr) {
                WorkerRow row{};
                row.worker = "worker_" + std::to_string(worker_idx);
                row.seed = current_attempt_seed;
                row.applied = local_attempts;
                row.status = "done";
                row.required_strategy_analyzed = local_required_analyzed;
                row.required_strategy_use = local_required_use;
                row.required_strategy_hit = local_required_hit;
                monitor->set_worker_row(static_cast<size_t>(worker_idx), row);
            }

            log_info(
                "runner.worker",
                "worker=" + std::to_string(worker_idx) +
                " last_seed=" + std::to_string(current_attempt_seed) +
                " attempts=" + std::to_string(local_attempts) +
                " written=" + std::to_string(local_written) +
                " required_analyzed=" + std::to_string(local_required_analyzed) +
                " required_use=" + std::to_string(local_required_use) +
                " required_hit=" + std::to_string(local_required_hit) +
                " required_zero_use_streak=" + std::to_string(local_required_zero_use_streak) +
                " required_zero_use_streak_max=" + std::to_string(local_required_zero_use_streak_max) +
                " best_template_score=" + std::to_string(local_best_template_score) +
                " template_family=" + std::string(pattern_forcing::pattern_kind_label(local_template_family)) +
                " mutation_source=" + std::string(pattern_forcing::pattern_mutation_source_label(local_mutation_source)) +
                " template_score_delta=" + std::to_string(local_last_template_score_delta) +
                " mutation_strength=" + std::to_string(local_last_mutation_strength) +
                " planner_zero_use_streak=" + std::to_string(local_planner_zero_use_streak) +
                " planner_failure_streak=" + std::to_string(local_planner_failure_streak) +
                " adaptive_target_strength=" + std::to_string(local_adaptive_target_strength));
        });
    }

    log_info("runner", "joining workers begin count=" + std::to_string(workers.size()));
    for (size_t i = 0; i < workers.size(); ++i) {
        auto& t = workers[i];
        if (t.joinable()) {
            log_info("runner", "joining worker index=" + std::to_string(i));
            t.join();
            log_info("runner", "joined worker index=" + std::to_string(i));
        }
    }

    log_info("runner", "all workers joined");

    result.accepted = accepted.load(std::memory_order_relaxed);
    result.written = written.load(std::memory_order_relaxed);
    result.attempts = attempts.load(std::memory_order_relaxed);
    result.rejected = rejected.load(std::memory_order_relaxed);
    result.reject_prefilter = reject_prefilter.load(std::memory_order_relaxed);
    result.reject_logic = reject_logic.load(std::memory_order_relaxed);
    result.reject_uniqueness = reject_uniqueness.load(std::memory_order_relaxed);
    result.reject_strategy = reject_strategy.load(std::memory_order_relaxed);
    result.reject_exact_no_progress = reject_exact_no_progress.load(std::memory_order_relaxed);
    result.reject_replay = reject_replay.load(std::memory_order_relaxed);
    result.reject_distribution_bias = reject_distribution_bias.load(std::memory_order_relaxed);
    result.reject_uniqueness_budget = reject_uniqueness_budget.load(std::memory_order_relaxed);

    result.uniqueness_calls = uniqueness_calls.load(std::memory_order_relaxed);
    result.uniqueness_nodes = uniqueness_nodes.load(std::memory_order_relaxed);
    result.uniqueness_elapsed_ms = static_cast<double>(uniqueness_elapsed_ns.load(std::memory_order_relaxed)) / 1e6;
    result.uniqueness_avg_ms = (result.uniqueness_calls > 0)
        ? (result.uniqueness_elapsed_ms / static_cast<double>(result.uniqueness_calls))
        : 0.0;

    result.kernel_calls = kernel_calls.load(std::memory_order_relaxed);
    result.kernel_time_ms = static_cast<double>(kernel_elapsed_ns.load(std::memory_order_relaxed)) / 1e6;
    result.logic_steps_total = logic_steps_total.load(std::memory_order_relaxed);
    result.strategy_naked_use = strategy_naked_use.load(std::memory_order_relaxed);
    result.strategy_naked_hit = strategy_naked_hit.load(std::memory_order_relaxed);
    result.strategy_hidden_use = strategy_hidden_use.load(std::memory_order_relaxed);
    result.strategy_hidden_hit = strategy_hidden_hit.load(std::memory_order_relaxed);
    result.mcts_advanced_evals = mcts_advanced_evals.load(std::memory_order_relaxed);
    result.certifier_required_strategy_analyzed = certifier_required_strategy_analyzed.load(std::memory_order_relaxed);
    result.certifier_required_slot_entered = certifier_required_slot_entered.load(std::memory_order_relaxed);
    result.certifier_required_strategy_use = certifier_required_strategy_use.load(std::memory_order_relaxed);
    result.certifier_required_strategy_hit = certifier_required_strategy_hit.load(std::memory_order_relaxed);
    result.required_strategy_certified_exact = required_strategy_certified_exact.load(std::memory_order_relaxed);
    result.mcts_required_strategy_analyzed = mcts_required_strategy_analyzed.load(std::memory_order_relaxed);
    result.mcts_required_strategy_use = mcts_required_strategy_use.load(std::memory_order_relaxed);
    result.mcts_required_strategy_hit = mcts_required_strategy_hit.load(std::memory_order_relaxed);
    result.pattern_exact_template_used = pattern_exact_template_used.load(std::memory_order_relaxed);
    result.pattern_family_fallback_used = pattern_family_fallback_used.load(std::memory_order_relaxed);
    result.required_strategy_exact_contract_met = required_strategy_exact_contract_met.load(std::memory_order_relaxed);

    const double asymmetry_ratio = static_cast<double>(std::max(run_cfg.box_rows, run_cfg.box_cols)) /
                                   static_cast<double>(std::max(1, std::min(run_cfg.box_rows, run_cfg.box_cols)));
    result.asymmetry_efficiency_index = asymmetry_ratio;
    result.backend_efficiency_score = (result.kernel_time_ms > 0.0)
        ? static_cast<double>(result.accepted) / (result.kernel_time_ms / 1000.0)
        : 0.0;

    const auto elapsed = duration_cast<duration<double>>(steady_clock::now() - t0).count();
    result.elapsed_s = elapsed;
    result.accepted_per_sec = (elapsed > 0.0) ? static_cast<double>(result.accepted) / elapsed : 0.0;

    const auto vip_target = post_processing::resolve_vip_grade_target_for_geometry(run_cfg);
    result.vip_score = post_processing::compute_vip_score(result, run_cfg, asymmetry_ratio);
    result.vip_grade = post_processing::vip_grade_from_score(result.vip_score);
    result.vip_contract_ok = post_processing::vip_contract_passed(result.vip_score, vip_target);
    result.vip_contract_fail_reason = result.vip_contract_ok ? "" : ("required=" + vip_target + ", actual=" + result.vip_grade);

    const std::string sig_raw =
        std::to_string(result.accepted) + ":" +
        std::to_string(result.written) + ":" +
        std::to_string(result.attempts) + ":" +
        std::to_string(result.uniqueness_nodes) + ":" +
        std::to_string(run_cfg.box_rows) + "x" + std::to_string(run_cfg.box_cols);
    const size_t h1 = std::hash<std::string>{}(sig_raw);
    const size_t h2 = std::hash<std::string>{}(sig_raw + ":v2");
    result.premium_signature = std::to_string(static_cast<unsigned long long>(h1));
    result.premium_signature_v2 = std::to_string(static_cast<unsigned long long>(h2));

    if (monitor != nullptr) {
        monitor->set_attempts(result.attempts);
        monitor->set_accepted(result.accepted);
        monitor->set_written(result.written);
        monitor->set_rejected(result.rejected);
        monitor->set_analyzed_required_strategy(result.mcts_required_strategy_analyzed);
        monitor->set_required_strategy_hits(result.mcts_required_strategy_hit);
        monitor->set_background_status("done accepted=" + std::to_string(result.accepted) + " written=" + std::to_string(result.written));
    }

    if (result.accepted != result.written) {
        log_warn(
            "runner",
            "accepted_written_mismatch accepted=" + std::to_string(result.accepted) +
            " written=" + std::to_string(result.written));
    }

    log_info(
        "runner",
        "done accepted=" + std::to_string(result.accepted) +
        " written=" + std::to_string(result.written) +
        " attempts=" + std::to_string(result.attempts) +
        " cert_required_analyzed=" + std::to_string(result.certifier_required_strategy_analyzed) +
        " cert_required_slot_entered=" + std::to_string(result.certifier_required_slot_entered) +
        " cert_required_use=" + std::to_string(result.certifier_required_strategy_use) +
        " cert_required_hit=" + std::to_string(result.certifier_required_strategy_hit) +
        " required_certified_exact=" + std::to_string(result.required_strategy_certified_exact) +
        " mcts_required_analyzed=" + std::to_string(result.mcts_required_strategy_analyzed) +
        " mcts_required_use=" + std::to_string(result.mcts_required_strategy_use) +
        " mcts_required_hit=" + std::to_string(result.mcts_required_strategy_hit) +
        " exact_template_used=" + std::to_string(result.pattern_exact_template_used) +
        " family_fallback_used=" + std::to_string(result.pattern_family_fallback_used) +
        " exact_contract_met=" + std::to_string(result.required_strategy_exact_contract_met) +
        " reject_exact_no_progress=" + std::to_string(result.reject_exact_no_progress) +
        " required_zero_use_streak_max=" + std::to_string(required_zero_use_streak_max.load(std::memory_order_relaxed)) +
        " best_template_score=" + std::to_string(best_template_score.load(std::memory_order_relaxed)));

    return result;
}

} // namespace sudoku_hpc
