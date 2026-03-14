//Author copyright Marcin Matysek (Rewertyn)

#include <atomic>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <conio.h>
#include <windows.h>
#endif

#include "Sources/utils/logging.h"
#include "Sources/config/run_config.h"
#include "Sources/core/geometry.h"
#include "Sources/cli/arg_parser.h"
#include "Sources/monitor.h"
#include "Sources/generator/runtime_runner.h"
#include "Sources/maintenance/quality_benchmark.h"
#include "Sources/gui.h"

namespace sudoku_hpc {

#ifdef _WIN32
inline LONG WINAPI log_unhandled_exception(EXCEPTION_POINTERS* ptr) {
    std::ostringstream out;
    out << "unhandled_exception";
    if (ptr != nullptr && ptr->ExceptionRecord != nullptr) {
        out << " code=0x" << std::hex << static_cast<unsigned long>(ptr->ExceptionRecord->ExceptionCode)
            << " address=" << ptr->ExceptionRecord->ExceptionAddress;
    }
    log_error("main.crash", out.str());
    return EXCEPTION_CONTINUE_SEARCH;
}

inline bool can_use_cli_hotkeys() {
    return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
}

inline const char* cli_state_label(bool cancel_requested, bool paused) {
    if (cancel_requested) {
        return "cancel_requested";
    }
    if (paused) {
        return "paused";
    }
    return "running";
}

inline void print_cli_hotkeys_help() {
    std::cout << "CLI controls: [P] pause/resume, [C] cancel, [Q] cancel\n";
}

inline std::jthread start_cli_hotkeys_thread(std::atomic<bool>& cancel_flag, std::atomic<bool>& pause_flag) {
    return std::jthread([&cancel_flag, &pause_flag](std::stop_token st) {
        while (!st.stop_requested()) {
            if (_kbhit() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            int ch = _getch();
            if (ch == 0 || ch == 224) {
                (void)_getch();
                continue;
            }
            const char key = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (key == 'p') {
                const bool now_paused = !pause_flag.load(std::memory_order_relaxed);
                pause_flag.store(now_paused, std::memory_order_relaxed);
                std::cout << "\n[CLI] " << (now_paused ? "paused" : "resumed") << "\n" << std::flush;
                log_info("main.cli_hotkeys", now_paused ? "paused" : "resumed");
            } else if (key == 'c' || key == 'q') {
                pause_flag.store(false, std::memory_order_relaxed);
                cancel_flag.store(true, std::memory_order_relaxed);
                std::cout << "\n[CLI] cancel requested\n" << std::flush;
                log_info("main.cli_hotkeys", "cancel requested");
            } else if (key == 'h' || key == '?') {
                print_cli_hotkeys_help();
            }
        }
    });
}

inline std::jthread start_cli_status_thread(std::atomic<bool>& cancel_flag, std::atomic<bool>& pause_flag) {
    return std::jthread([&cancel_flag, &pause_flag](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            if (st.stop_requested()) {
                break;
            }
            const bool cancel_requested = cancel_flag.load(std::memory_order_relaxed);
            const bool paused = pause_flag.load(std::memory_order_relaxed);
            std::cout << "[CLI] state=" << cli_state_label(cancel_requested, paused) << "\n" << std::flush;
        }
    });
}
#endif

inline void install_crash_logging() {
    std::set_terminate([]() {
        log_error("main.crash", "std::terminate called");
        std::_Exit(134);
    });
    std::signal(SIGABRT, [](int sig) {
        log_error("main.crash", "signal=" + std::to_string(sig));
        std::_Exit(128 + sig);
    });
#ifdef _WIN32
    SetUnhandledExceptionFilter(log_unhandled_exception);
#endif
}

inline void handle_result(const GenerateRunResult& result, const GenerateRunConfig& cfg) {
    const auto audit_summary = logic::GenericLogicCertify::build_audit_summary();
    std::cout << "\n=== Generation Summary ===\n";
    std::cout << "Accepted: " << result.accepted << "\n";
    std::cout << "Written: " << result.written << "\n";
    std::cout << "Attempts: " << result.attempts << "\n";
    if (cfg.required_strategy != RequiredStrategy::None) {
        std::cout << "Required strategy: " << to_string(cfg.required_strategy) << "\n";
        std::cout << "Required strategy use (certifier): "
                  << result.certifier_required_strategy_use << "\n";
        std::cout << "Required strategy hit (certifier): "
                  << result.certifier_required_strategy_hit << "\n";
        std::cout << "Required strategy analyzed (certifier): "
                  << result.certifier_required_strategy_analyzed << "\n";
        std::cout << "Required strategy use/hit (certifier): "
                  << result.certifier_required_strategy_use << "/"
                  << result.certifier_required_strategy_hit << "\n";
        std::cout << "MCTS required use/hit: "
                  << result.mcts_required_strategy_use << "/"
                  << result.mcts_required_strategy_hit << "\n";
        std::cout << "MCTS required analyzed: "
                  << result.mcts_required_strategy_analyzed << "\n";
    }
    std::cout << "Rejected: " << result.rejected << "\n";
    std::cout << "Measurement profile: " << result.measurement_profile << "\n";
    std::cout << "Effective clue range: " << result.effective_min_clues << "-" << result.effective_max_clues << "\n";
    std::cout << "  - Prefilter: " << result.reject_prefilter << "\n";
    std::cout << "  - Logic: " << result.reject_logic << "\n";
    std::cout << "  - Uniqueness: " << result.reject_uniqueness << "\n";
    std::cout << "  - StrategyStageRejects: " << result.reject_strategy << "\n";
    std::cout << "  - Replay: " << result.reject_replay << "\n";
    std::cout << "  - DistributionBias: " << result.reject_distribution_bias << "\n";
    std::cout << "  - UniquenessBudget: " << result.reject_uniqueness_budget << "\n";
    std::cout << "Uniqueness calls: " << result.uniqueness_calls << "\n";
    std::cout << "Uniqueness nodes: " << result.uniqueness_nodes << "\n";
    std::cout << "Uniqueness total: " << std::fixed << std::setprecision(3) << result.uniqueness_elapsed_ms << " ms\n";
    std::cout << "Uniqueness avg: " << std::fixed << std::setprecision(3) << result.uniqueness_avg_ms << " ms/call\n";
    std::cout << "CPU backend: " << result.cpu_backend_selected << "\n";
    std::cout << "Kernel time: " << std::fixed << std::setprecision(3) << result.kernel_time_ms << " ms\n";
    std::cout << "Kernel calls: " << result.kernel_calls << "\n";
    std::cout << "Backend efficiency score: " << std::fixed << std::setprecision(3) << result.backend_efficiency_score << "\n";
    std::cout << "Asymmetry efficiency index: " << std::fixed << std::setprecision(3) << result.asymmetry_efficiency_index << "\n";
    std::cout << "Logic steps total: " << result.logic_steps_total << "\n";
    std::cout << "Naked hit/use: " << result.strategy_naked_hit << "/" << result.strategy_naked_use << "\n";
    std::cout << "Hidden hit/use: " << result.strategy_hidden_hit << "/" << result.strategy_hidden_use << "\n";
    std::cout << "MCTS advanced evals: " << result.mcts_advanced_evals << "\n";
    if (cfg.required_strategy != RequiredStrategy::None) {
        std::cout << "Certifier required analyzed/use/hit: "
                  << result.certifier_required_strategy_analyzed << "/"
                  << result.certifier_required_strategy_use << "/"
                  << result.certifier_required_strategy_hit << "\n";
    }
    std::cout << "Pattern exact template used: " << result.pattern_exact_template_used << "\n";
    std::cout << "Pattern family fallback used: " << result.pattern_family_fallback_used << "\n";
    std::cout << "Required exact contract met: " << result.required_strategy_exact_contract_met << "\n";
    std::cout << "Strategy audit canonical full: " << audit_summary.canonical_full << "/" << audit_summary.total_slots << "\n";
    std::cout << "Strategy audit tighten/rewrite/missing: "
              << audit_summary.tighten << "/" << audit_summary.rewrite << "/" << audit_summary.exact_template_missing << "\n";
    std::cout << "Strategy audit parser/certifier/generator exact/fallback/smoke: "
              << audit_summary.parser_selectable << "/"
              << audit_summary.certifier_wired << "/"
              << audit_summary.generator_exact_template_wired << "/"
              << audit_summary.generator_family_fallback_wired << "/"
              << audit_summary.smoke_profile_present << "\n";
    std::cout << "Strategy audit family-fallback-only / exact-required-missing: "
              << audit_summary.family_fallback_only << "/"
              << audit_summary.exact_required_but_missing << "\n";
    std::cout << "VIP score: " << std::fixed << std::setprecision(3) << result.vip_score << "\n";
    std::cout << "VIP grade: " << result.vip_grade << "\n";
    std::cout << "VIP contract: " << (result.vip_contract_ok ? "ok" : "fail") << "\n";
    std::cout << "VIP contract reason: " << result.vip_contract_fail_reason << "\n";
    std::cout << "Premium signature: " << result.premium_signature << "\n";
    std::cout << "Premium signature v2: " << result.premium_signature_v2 << "\n";
    std::cout << "Time: " << std::fixed << std::setprecision(2) << result.elapsed_s << "s\n";
    std::cout << "Rate: " << std::fixed << std::setprecision(2) << result.accepted_per_sec << " puzzles/s\n";

    if (cfg.pause_on_exit_windows) {
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
    }
}

inline void print_production_help(std::ostream& out) {
    out << "Sudoku Generator (production)\n";
    out << "Author copyright Marcin Matysek (Rewertyn)\n";
    out << "Seed type: uint64_t (unsigned 64-bit)\n";
    out << "Usage:\n";
    out << "  sudoku_gen.exe [options]\n\n";
    out << "Common options:\n";
    out << "  --help, -h                      Show this help\n";
    out << "  --box-rows <int>                Box rows\n";
    out << "  --box-cols <int>                Box cols\n";
    out << "  --difficulty <1..9>             Difficulty level\n";
    out << "  --required-strategy <name>      Required strategy (normalized token)\n";
    out << "  --target <uint64>               Target puzzles to generate\n";
    out << "  --threads <int>                 Worker threads (0=auto)\n";
    out << "  --seed <uint64>                 RNG seed (0=random)\n";
    out << "  --output-folder <path>          Output directory\n";
    out << "  --output-file <name>            Output batch file name\n";
    out << "  --single-file-only              Disable per-puzzle files\n";
    out << "  --pattern-forcing               Enable Pattern Forcing\n";
    out << "  --mcts-digger                   Enable MCTS bottleneck digger\n";
    out << "  --mcts-profile <auto|p7|p8|off> Tuning profile for digger scoring\n";
    out << "  --strict-canonical-strategies   Require canonical (non-proxy) required strategy hits\n";
    out << "  --no-proxy-advanced             Reject proxy-only advanced strategy confirmation\n";
    out << "  --allow-proxy-advanced          Allow proxy/hybrid advanced strategy confirmation\n";
    out << "  --max-pattern-depth <0..8>      Cap advanced pattern depth (0=auto)\n";
    out << "  --fast-test                     Fast smoke mode (relaxed contracts, short budgets)\n";
    out << "  --max-total-time-s <uint64>     Global runtime timeout (0=none)\n";
    out << "  --run-quality-benchmark <file>  Write strategy audit report (.txt + .csv)\n";
    out << "  --list-geometries               Print supported geometries\n";
    out << "  --validate-geometry             Validate current --box-rows/--box-cols\n";
    out << "  --validate-geometry-catalog     Validate full geometry catalog\n";
    out << "  --gui                           Force GUI mode (Windows)\n";
    out << "  --cli                           Force CLI mode (Windows)\n\n";
    out << "Examples:\n";
    out << "  sudoku_gen.exe --box-rows 3 --box-cols 3 --difficulty 1 --target 100 --threads 16\n";
    out << "  sudoku_gen.exe --list-geometries\n";
    out << "  sudoku_gen.exe --validate-geometry --box-rows 4 --box-cols 3\n";
}

} // namespace sudoku_hpc

int main(int argc, char** argv) {
    using namespace sudoku_hpc;

    install_crash_logging();

    try {
        log_info("main", "program start");
        std::cout << "Debug log file: " << debug_logger().path() << "\n";
        std::cout << "Author copyright Marcin Matysek (Rewertyn)\n";
        std::cout << "Seed type: uint64_t\n";

        if (has_arg(argc, argv, "--help") || has_arg(argc, argv, "-h") || has_arg(argc, argv, "/?")) {
            print_production_help(std::cout);
            return 0;
        }

#ifdef _WIN32
        const bool force_gui = has_arg(argc, argv, "--gui");
        const bool force_cli = has_arg(argc, argv, "--cli");
        const bool force_console = has_arg(argc, argv, "--force-console");
        if ((argc == 1 || force_gui) && !force_cli) {
            log_info("main", "starting GUI mode");
            if (force_console) {
                ensure_console_attached();
            }
            return run_gui_winapi(GetModuleHandleW(nullptr));
        }
#endif

        ParseArgsResult parse_result = parse_args(argc, argv);
        GenerateRunConfig cfg = parse_result.cfg;
        log_info(
            "main",
            "parsed_args geom=" + std::to_string(cfg.box_rows) + "x" + std::to_string(cfg.box_cols) +
            " difficulty=" + std::to_string(cfg.difficulty_level_required) +
            " required=" + to_string(cfg.required_strategy) +
            " target=" + std::to_string(cfg.target_puzzles) +
            " threads=" + std::to_string(cfg.threads) +
            " fast_test=" + std::string(cfg.fast_test_mode ? "1" : "0"));

        if (parse_result.list_geometries) {
            std::cout << supported_geometries_text();
            return 0;
        }
        if (parse_result.validate_geometry) {
            const bool ok = print_geometry_validation(cfg.box_rows, cfg.box_cols, std::cout);
            return ok ? 0 : 1;
        }
        if (parse_result.validate_geometry_catalog) {
            const bool ok = print_geometry_catalog_validation(std::cout);
            return ok ? 0 : 1;
        }

        if (parse_result.explain_profile) {
            std::cout << explain_generation_profile_text(cfg);
            return 0;
        }

        if (parse_result.run_quality_benchmark) {
            std::string csv_path;
            const bool ok = maintenance::write_quality_benchmark_report(
                parse_result.quality_benchmark_report,
                parse_result.quality_benchmark_max_cases,
                &csv_path);
            if (!ok) {
                std::cerr << "Failed to write quality benchmark report: "
                          << parse_result.quality_benchmark_report << "\n";
                return 1;
            }
            std::cout << "Quality benchmark text report: " << parse_result.quality_benchmark_report << "\n";
            std::cout << "Quality benchmark CSV report: " << csv_path << "\n";
            return 0;
        }

        if (parse_result.run_regression_tests ||
            parse_result.run_geometry_gate ||
            parse_result.run_pre_difficulty_gate ||
            parse_result.run_asym_pair_benchmark ||
            parse_result.run_vip_benchmark ||
            parse_result.run_vip_gate ||
            cfg.stage_start || cfg.stage_end || cfg.perf_ab_suite ||
            parse_result.benchmark_mode) {
            std::cout << "Selected maintenance mode is not wired in this runtime build yet. "
                      << "Running normal generation with provided config.\n";
        }

        std::atomic<bool> cancel_flag{false};
        std::atomic<bool> pause_flag{false};

#ifdef _WIN32
        std::jthread cli_hotkeys_thread;
        std::jthread cli_status_thread;
        if (can_use_cli_hotkeys()) {
            print_cli_hotkeys_help();
            cli_hotkeys_thread = start_cli_hotkeys_thread(cancel_flag, pause_flag);
            cli_status_thread = start_cli_status_thread(cancel_flag, pause_flag);
        }
#endif

        ConsoleStatsMonitor monitor;
        monitor.start_ui_thread(5000);

        log_info("main", "run_generic_sudoku begin");
        std::mutex console_mu;
        auto result = run_generic_sudoku(
            cfg,
            &monitor,
            &cancel_flag,
            &pause_flag,
            nullptr,
            [&](const std::string& msg) {
                std::lock_guard<std::mutex> lock(console_mu);
                std::cout << msg << "\n";
                std::cout.flush();
            });
        log_info("main", "run_generic_sudoku end");

        log_info("main", "monitor stop begin");
        monitor.stop_ui_thread();
        log_info("main", "monitor stop end");

#ifdef _WIN32
        if (cli_hotkeys_thread.joinable()) {
            log_info("main", "cli_hotkeys join begin");
            cli_hotkeys_thread.request_stop();
            cli_hotkeys_thread.join();
            log_info("main", "cli_hotkeys join end");
        }
        if (cli_status_thread.joinable()) {
            log_info("main", "cli_status join begin");
            cli_status_thread.request_stop();
            cli_status_thread.join();
            log_info("main", "cli_status join end");
        }
#endif

        log_info("main", "handle_result begin");
        handle_result(result, cfg);
        log_info("main", "handle_result end");
        log_info("main", "program end ok");
        return 0;
    } catch (const std::exception& ex) {
        log_error("main.exception", ex.what());
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        log_error("main.exception", "unknown");
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
