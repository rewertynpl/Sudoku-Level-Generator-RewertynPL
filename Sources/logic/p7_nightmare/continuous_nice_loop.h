// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: continuous_nice_loop.h (Level 7 - Nightmare)
// Description: Direct alternating strong/weak loop search for true continuous
// nice loops. Discontinuous loop inferences are intentionally excluded.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool cnl_is_strong_neighbor(const shared::ExactPatternScratchpad& sp, int u, int v) {
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

inline int cnl_collect_neighbors(
    const shared::ExactPatternScratchpad& sp,
    int u,
    int edge_type,
    int* out) {
    int cnt = 0;
    if (edge_type == 1) {
        const int p0 = sp.dyn_strong_offsets[u];
        const int p1 = sp.dyn_strong_offsets[u + 1];
        for (int p = p0; p < p1; ++p) {
            out[cnt++] = sp.dyn_strong_adj[p];
        }
        return cnt;
    }

    const int p0 = sp.dyn_weak_offsets[u];
    const int p1 = sp.dyn_weak_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        const int v = sp.dyn_weak_adj[p];
        if (cnl_is_strong_neighbor(sp, u, v)) continue;
        out[cnt++] = v;
    }
    return cnt;
}

inline ApplyResult cnl_prune_weak_edge(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int a_cell,
    int b_cell) {
    for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
        const int idx = sp.dyn_digit_cells[i];
        if (idx == a_cell || idx == b_cell) continue;
        if (!st.is_peer(idx, a_cell) || !st.is_peer(idx, b_cell)) continue;
        const ApplyResult er = st.eliminate(idx, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_continuous_nice_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool any_progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int neighbors[256]{};
    int cycle_cells[64]{};
    int cycle_edge_types[64]{};
    int backtrack_idx[64]{};

    int* const vis_even = sp.visited;
    int* const vis_odd = sp.bfs_depth;
    int* const queue_state = sp.bfs_queue;
    int* const queue_parent = sp.chain_cell;
    int* const queue_depth = sp.chain_parent;

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 3 || sp.dyn_strong_edge_count == 0) continue;

        const int max_depth = std::clamp(10 + (st.board->empty_cells / std::max(1, n)) + (n / 3), 12, 28);

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            const int start_cell = sp.dyn_node_to_cell[start];
            if (st.board->values[start_cell] != 0) continue;

            for (int first_type = 0; first_type <= 1; ++first_type) {
                std::fill_n(vis_even, sp.dyn_node_count, 0);
                std::fill_n(vis_odd, sp.dyn_node_count, 0);

                int qh = 0;
                int qt = 0;

                const int first_cnt = cnl_collect_neighbors(sp, start, first_type, neighbors);
                for (int i = 0; i < first_cnt; ++i) {
                    if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
                    const int v = neighbors[i];
                    queue_state[qt] = (v << 1) | first_type;
                    queue_parent[qt] = -1;
                    queue_depth[qt] = 1;
                    if (first_type == 0) vis_even[v] = 1;
                    else vis_odd[v] = 1;
                    ++qt;
                }

                bool inferred = false;
                while (qh < qt && !inferred) {
                    const int state_idx = qh;
                    const int state = queue_state[qh];
                    const int dep = queue_depth[qh];
                    ++qh;

                    const int u = (state >> 1);
                    const int last_type = (state & 1);
                    const int next_type = 1 - last_type;
                    if (dep >= max_depth) continue;

                    const int next_cnt = cnl_collect_neighbors(sp, u, next_type, neighbors);
                    for (int i = 0; i < next_cnt; ++i) {
                        const int v = neighbors[i];
                        if (v == start) {
                            if (next_type != first_type && dep >= 3) {
                                int bt_count = 0;
                                for (int cur = state_idx; cur >= 0 && bt_count < 64; cur = queue_parent[cur]) {
                                    backtrack_idx[bt_count++] = cur;
                                }

                                int node_count = 1;
                                int edge_count = 0;
                                cycle_cells[0] = start_cell;
                                for (int k = bt_count - 1; k >= 0; --k) {
                                    const int packed = queue_state[backtrack_idx[k]];
                                    cycle_edge_types[edge_count++] = (packed & 1);
                                    cycle_cells[node_count++] = sp.dyn_node_to_cell[packed >> 1];
                                }
                                cycle_edge_types[edge_count++] = next_type;

                                for (int e = 0; e < edge_count && !inferred; ++e) {
                                    if (cycle_edge_types[e] != 0) continue;
                                    const int a_cell = cycle_cells[e];
                                    const int b_cell = (e + 1 < node_count) ? cycle_cells[e + 1] : start_cell;
                                    const ApplyResult er = cnl_prune_weak_edge(st, sp, bit, a_cell, b_cell);
                                    if (er == ApplyResult::Contradiction) {
                                        s.elapsed_ns += get_current_time_ns() - t0;
                                        return er;
                                    }
                                    if (er == ApplyResult::Progress) {
                                        any_progress = true;
                                        inferred = true;
                                    }
                                }
                            }
                            continue;
                        }

                        int* vis = (next_type == 0) ? vis_even : vis_odd;
                        if (vis[v] != 0) continue;
                        vis[v] = 1;
                        if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
                        queue_state[qt] = (v << 1) | next_type;
                        queue_parent[qt] = state_idx;
                        queue_depth[qt] = dep + 1;
                        ++qt;
                    }
                }
            }
        }
    }

    if (any_progress) {
        ++s.hit_count;
        r.used_continuous_nice_loop = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
