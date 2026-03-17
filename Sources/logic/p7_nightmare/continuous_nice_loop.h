// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: continuous_nice_loop.h (Level 7 - Nightmare)
// Description: Direct alternating strong/weak loop search for true continuous
// nice loops. Discontinuous loop inferences are intentionally excluded.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool cnl_has_strong_edge(const shared::ExactPatternScratchpad& sp, const int u, const int v) {
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

inline bool cnl_has_weak_edge(const shared::ExactPatternScratchpad& sp, const int u, const int v) {
    const int p0 = sp.dyn_weak_offsets[u];
    const int p1 = sp.dyn_weak_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_weak_adj[p] == v) return true;
    }
    return false;
}

inline int cnl_collect_neighbors(
    const shared::ExactPatternScratchpad& sp,
    const int u,
    const int edge_type,
    int* const out) {
    int cnt = 0;
    if (edge_type == 1) {
        const int p0 = sp.dyn_strong_offsets[u];
        const int p1 = sp.dyn_strong_offsets[u + 1];
        for (int p = p0; p < p1; ++p) {
            const int v = sp.dyn_strong_adj[p];
            bool dup = false;
            for (int i = 0; i < cnt; ++i) {
                if (out[i] == v) {
                    dup = true;
                    break;
                }
            }
            if (!dup) out[cnt++] = v;
        }
        return cnt;
    }

    const int p0 = sp.dyn_weak_offsets[u];
    const int p1 = sp.dyn_weak_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        const int v = sp.dyn_weak_adj[p];
        if (cnl_has_strong_edge(sp, u, v)) continue;
        bool dup = false;
        for (int i = 0; i < cnt; ++i) {
            if (out[i] == v) {
                dup = true;
                break;
            }
        }
        if (!dup) out[cnt++] = v;
    }
    return cnt;
}

inline bool cnl_cycle_nodes_are_unique(const int* const nodes, const int cnt) {
    for (int i = 0; i < cnt; ++i) {
        for (int j = i + 1; j < cnt; ++j) {
            if (nodes[i] == nodes[j]) return false;
        }
    }
    return true;
}

inline bool cnl_validate_cycle(
    const shared::ExactPatternScratchpad& sp,
    const int* const cycle_nodes,
    const int* const cycle_edge_types,
    const int edge_count,
    const int start_node,
    const int first_type) {
    if (edge_count < 4) return false;
    if (cycle_nodes[0] != start_node) return false;
    if (!cnl_cycle_nodes_are_unique(cycle_nodes, edge_count)) return false;

    for (int e = 0; e < edge_count; ++e) {
        if (cycle_edge_types[e] != (e & 1 ? 1 - first_type : first_type)) return false;
        const int u = cycle_nodes[e];
        const int v = cycle_nodes[(e + 1) % edge_count];
        if (cycle_edge_types[e] == 1) {
            if (!cnl_has_strong_edge(sp, u, v)) return false;
        } else {
            if (!cnl_has_weak_edge(sp, u, v) || cnl_has_strong_edge(sp, u, v)) return false;
        }
    }
    return true;
}

inline ApplyResult cnl_prune_weak_edge(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    const uint64_t bit,
    const int a_node,
    const int b_node) {
    const int a_cell = sp.dyn_node_to_cell[a_node];
    const int b_cell = sp.dyn_node_to_cell[b_node];
    if (a_cell < 0 || b_cell < 0 || a_cell == b_cell) return ApplyResult::NoProgress;
    if (!cnl_has_weak_edge(sp, a_node, b_node) || cnl_has_strong_edge(sp, a_node, b_node)) {
        return ApplyResult::NoProgress;
    }

    ApplyResult best = ApplyResult::NoProgress;
    for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
        const int idx = sp.dyn_digit_cells[i];
        if (idx == a_cell || idx == b_cell) continue;
        if (st.board->values[idx] != 0) continue;
        if ((st.cands[idx] & bit) == 0ULL) continue;
        if (!st.is_peer(idx, a_cell) || !st.is_peer(idx, b_cell)) continue;
        const ApplyResult er = st.eliminate(idx, bit);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) best = ApplyResult::Progress;
    }
    return best;
}

inline ApplyResult apply_continuous_nice_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool any_progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int neighbors[256]{};
    int cycle_nodes[64]{};
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
        if (sp.dyn_node_count < 4 || sp.dyn_strong_edge_count == 0 || sp.dyn_weak_edge_count == 0) continue;

        const int empties = st.board->empty_cells;
        const int max_depth = std::clamp(8 + (empties / std::max(1, n)) + (n / 4), 10, 24);

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            const int start_cell = sp.dyn_node_to_cell[start];
            if (start_cell < 0 || st.board->values[start_cell] != 0) continue;

            for (int first_type = 0; first_type <= 1; ++first_type) {
                std::fill_n(vis_even, sp.dyn_node_count, 0);
                std::fill_n(vis_odd, sp.dyn_node_count, 0);

                int qh = 0;
                int qt = 0;
                const int first_cnt = cnl_collect_neighbors(sp, start, first_type, neighbors);
                for (int i = 0; i < first_cnt; ++i) {
                    const int v = neighbors[i];
                    if (v == start) continue;
                    if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
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
                    const int packed_state = queue_state[qh];
                    const int dep = queue_depth[qh];
                    ++qh;

                    const int u = (packed_state >> 1);
                    const int last_type = (packed_state & 1);
                    const int next_type = 1 - last_type;
                    if (dep >= max_depth) continue;

                    const int next_cnt = cnl_collect_neighbors(sp, u, next_type, neighbors);
                    for (int i = 0; i < next_cnt; ++i) {
                        const int v = neighbors[i];
                        if (v == start) {
                            if (dep < 3 || next_type == last_type) continue;

                            int bt_count = 0;
                            for (int cur = state_idx; cur >= 0 && bt_count < 63; cur = queue_parent[cur]) {
                                backtrack_idx[bt_count++] = cur;
                            }
                            if (bt_count < 3) continue;

                            int node_count = 1;
                            int edge_count = 0;
                            cycle_nodes[0] = start;
                            for (int k = bt_count - 1; k >= 0; --k) {
                                const int packed = queue_state[backtrack_idx[k]];
                                cycle_edge_types[edge_count++] = (packed & 1);
                                cycle_nodes[node_count++] = (packed >> 1);
                            }
                            cycle_edge_types[edge_count++] = next_type;
                            if (edge_count != node_count) continue;
                            if (!cnl_validate_cycle(sp, cycle_nodes, cycle_edge_types, edge_count, start, first_type)) continue;

                            for (int e = 0; e < edge_count && !inferred; ++e) {
                                if (cycle_edge_types[e] != 0) continue;
                                const int a_node = cycle_nodes[e];
                                const int b_node = cycle_nodes[(e + 1) % edge_count];
                                const ApplyResult er = cnl_prune_weak_edge(st, sp, bit, a_node, b_node);
                                if (er == ApplyResult::Contradiction) {
                                    s.elapsed_ns += get_current_time_ns() - t0;
                                    return er;
                                }
                                if (er == ApplyResult::Progress) {
                                    any_progress = true;
                                    inferred = true;
                                }
                            }
                            continue;
                        }

                        int* const vis = (next_type == 0) ? vis_even : vis_odd;
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
