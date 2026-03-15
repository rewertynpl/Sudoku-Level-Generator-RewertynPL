#pragma once

#include <algorithm>

#include "../../config/run_config.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline int p8_min_empty_floor(RequiredStrategy required, int n, int nn) {
    const int global_floor = std::max(nn / 3, std::max(4, n));
    switch (required) {
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
            return std::max(global_floor, nn - 3 * n);
        case RequiredStrategy::MSLS:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::SKLoop:
            return std::max(global_floor, nn - 4 * n);
        default:
            return global_floor;
    }
}

inline bool p8_board_too_dense(RequiredStrategy required, int n, int nn, int empty_cells) {
    return empty_cells < p8_min_empty_floor(required, n, nn);
}

} // namespace sudoku_hpc::logic::p8_theoretical
