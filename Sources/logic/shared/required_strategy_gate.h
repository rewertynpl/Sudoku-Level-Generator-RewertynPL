#pragma once

#include "../../config/run_config.h"

namespace sudoku_hpc::logic::shared {

inline thread_local RequiredStrategy tls_required_exact_strategy = RequiredStrategy::None;

inline RequiredStrategy current_required_exact_strategy() {
    return tls_required_exact_strategy;
}

inline bool required_exact_strategy_active(RequiredStrategy rs) {
    return tls_required_exact_strategy == rs && rs != RequiredStrategy::None;
}

struct RequiredExactStrategyScope {
    explicit RequiredExactStrategyScope(RequiredStrategy rs)
        : previous(tls_required_exact_strategy) {
        tls_required_exact_strategy = strategy_requires_exact_only(rs) ? rs : RequiredStrategy::None;
    }

    ~RequiredExactStrategyScope() {
        tls_required_exact_strategy = previous;
    }

    RequiredExactStrategyScope(const RequiredExactStrategyScope&) = delete;
    RequiredExactStrategyScope& operator=(const RequiredExactStrategyScope&) = delete;

private:
    RequiredStrategy previous;
};

} // namespace sudoku_hpc::logic::shared
