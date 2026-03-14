// ============================================================================
// SUDOKU HPC - MCTS DIGGER
// Module: mcts_node.h
// Description: Thread-local MCTS scratch with flat buffers sized for N <= 64.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace sudoku_hpc::mcts_digger {

struct MctsNodeScratch {
    static constexpr int MAX_N = 64;
    static constexpr int MAX_NN = MAX_N * MAX_N;

    int prepared_nn = 0;

    // Fixed SoA storage removes heap churn from the digger hot path.
    std::array<double, MAX_NN> reward_sum{};
    std::array<double, MAX_NN> prior_bonus{};
    std::array<uint32_t, MAX_NN> visits{};
    std::array<int, MAX_NN> active_cells{};
    std::array<int, MAX_NN> active_pos{};

    int active_count = 0;
    uint64_t total_visits = 0;

    void ensure(int nn) {
        if (prepared_nn == nn) {
            return;
        }
        if (nn < 0) {
            nn = 0;
        }
        if (nn > MAX_NN) {
            nn = MAX_NN;
        }
        std::fill_n(reward_sum.data(), nn, 0.0);
        std::fill_n(prior_bonus.data(), nn, 0.0);
        std::fill_n(visits.data(), nn, 0U);
        std::fill_n(active_cells.data(), nn, -1);
        std::fill_n(active_pos.data(), nn, -1);
        prepared_nn = nn;
        active_count = 0;
        total_visits = 0;
    }

    void reset(int nn) {
        ensure(nn);
        std::fill_n(reward_sum.data(), prepared_nn, 0.0);
        std::fill_n(prior_bonus.data(), prepared_nn, 0.0);
        std::fill_n(visits.data(), prepared_nn, 0U);
        std::fill_n(active_cells.data(), prepared_nn, -1);
        std::fill_n(active_pos.data(), prepared_nn, -1);
        active_count = 0;
        total_visits = 0;
    }

    void activate(int cell) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        if (active_pos[static_cast<size_t>(cell)] >= 0) {
            return;
        }

        const int pos = active_count++;
        active_cells[static_cast<size_t>(pos)] = cell;
        active_pos[static_cast<size_t>(cell)] = pos;
    }

    void disable(int cell) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        const int pos = active_pos[static_cast<size_t>(cell)];
        if (pos < 0 || pos >= active_count) {
            return;
        }

        const int last_pos = active_count - 1;
        const int last_cell = active_cells[static_cast<size_t>(last_pos)];

        active_cells[static_cast<size_t>(pos)] = last_cell;
        if (last_cell >= 0) {
            active_pos[static_cast<size_t>(last_cell)] = pos;
        }

        active_cells[static_cast<size_t>(last_pos)] = -1;
        active_pos[static_cast<size_t>(cell)] = -1;
        --active_count;
    }

    void update(int cell, double reward) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        reward_sum[static_cast<size_t>(cell)] += reward;
        visits[static_cast<size_t>(cell)] += 1U;
        ++total_visits;
    }

    void set_prior(int cell, double bonus) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        prior_bonus[static_cast<size_t>(cell)] = bonus;
    }
};

inline MctsNodeScratch& tls_mcts_node_scratch() {
    thread_local MctsNodeScratch s;
    return s;
}

} // namespace sudoku_hpc::mcts_digger
