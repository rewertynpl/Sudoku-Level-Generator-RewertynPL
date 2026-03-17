// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: aic_grouped_aic.h (Level 7 - Nightmare)
// Description: Direct Alternating Inference Chains (AIC) and Grouped AIC
// on strong/weak candidate graphs per digit, zero-allocation.
// Updated for safer grouped-node semantics, asymmetrical geometries and
// scratchpad-only traversal.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline uint64_t get_current_time_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline bool aic_is_strong_neighbor(const shared::ExactPatternScratchpad& sp, int u, int v) {
    if (u < 0 || v < 0 || u >= sp.dyn_node_count || v >= sp.dyn_node_count) return false;
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

inline bool aic_node_contains_cell(const shared::ExactPatternScratchpad& sp, int node, int cell) {
    if (node < 0 || node >= sp.dyn_node_count) return false;
    const int p0 = sp.adj_offsets[node];
    const int p1 = sp.adj_offsets[node + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.adj_flat[p] == cell) return true;
    }
    return false;
}

inline bool aic_node_active_for_digit(
    const CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int node) {
    if (node < 0 || node >= sp.dyn_node_count) return false;
    const int p0 = sp.adj_offsets[node];
    const int p1 = sp.adj_offsets[node + 1];
    if (p0 >= p1) return false;
    for (int p = p0; p < p1; ++p) {
        const int cell = sp.adj_flat[p];
        if (cell < 0 || cell >= st.topo->nn) continue;
        if (st.board->values[cell] != 0) continue;
        if ((st.cands[cell] & bit) != 0ULL) return true;
    }
    return false;
}

inline int aic_collect_neighbors(
    const shared::ExactPatternScratchpad& sp,
    int u,
    int edge_type, // 1=strong, 0=weak-only
    int* out) {
    if (u < 0 || u >= sp.dyn_node_count) return 0;

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
        if (aic_is_strong_neighbor(sp, u, v)) continue;
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

// Eliminate the digit from cells seeing both terminal nodes.
// For grouped nodes the target must see all cells belonging to each node.
inline ApplyResult aic_eliminate_common_peers(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int a_node,
    int b_node) {

    for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
        const int idx = sp.dyn_digit_cells[i];
        if (idx < 0 || idx >= st.topo->nn) continue;
        if ((st.cands[idx] & bit) == 0ULL) continue;
        if (st.board->values[idx] != 0) continue;

        if (aic_node_contains_cell(sp, a_node, idx)) continue;
        if (aic_node_contains_cell(sp, b_node, idx)) continue;

        if (!shared::node_sees_cell(st, sp, a_node, idx)) continue;
        if (!shared::node_sees_cell(st, sp, b_node, idx)) continue;

        const ApplyResult er = st.eliminate(idx, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

// Eliminate the digit from all cells of the start node.
inline ApplyResult aic_eliminate_start_candidate(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int start_node) {

    ApplyResult final_res = ApplyResult::NoProgress;
    const int p0 = sp.adj_offsets[start_node];
    const int p1 = sp.adj_offsets[start_node + 1];

    for (int p = p0; p < p1; ++p) {
        const int cell = sp.adj_flat[p];
        if (cell < 0 || cell >= st.topo->nn) continue;
        if (st.board->values[cell] != 0) continue;
        if ((st.cands[cell] & bit) == 0ULL) continue;

        const ApplyResult er = st.eliminate(cell, bit);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) final_res = ApplyResult::Progress;
    }
    return final_res;
}

inline ApplyResult aic_force_singleton_start_true(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int start_node) {
    const int p0 = sp.adj_offsets[start_node];
    const int p1 = sp.adj_offsets[start_node + 1];
    if (p1 - p0 != 1) return ApplyResult::NoProgress;

    const int cell = sp.adj_flat[p0];
    if (cell < 0 || cell >= st.topo->nn) return ApplyResult::NoProgress;
    if (st.board->values[cell] != 0) return ApplyResult::NoProgress;
    if ((st.cands[cell] & bit) == 0ULL) return ApplyResult::NoProgress;

    const uint64_t other_cands = st.cands[cell] & ~bit;
    if (other_cands == 0ULL) return ApplyResult::NoProgress;
    return st.eliminate(cell, other_cands);
}

// Main alternating-chain search core used by both AIC and Grouped AIC.
inline ApplyResult alternating_chain_core(
    CandidateState& st,
    int depth_cap,
    bool allow_weak_start,
    bool& used_flag) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;
    int neighbors[256]{};

    int* const vis_even = sp.visited;
    int* const vis_odd = sp.bfs_depth;
    int* const queue_state = sp.bfs_queue;
    int* const queue_depth = sp.bfs_parent;

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 4 || sp.dyn_strong_edge_count == 0) continue;

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            if (!aic_node_active_for_digit(st, sp, bit, start)) continue;

            for (int first_type = 1; first_type >= 0; --first_type) {
                if (first_type == 0 && !allow_weak_start) continue;

                std::fill_n(vis_even, sp.dyn_node_count, 0);
                std::fill_n(vis_odd, sp.dyn_node_count, 0);

                int qh = 0;
                int qt = 0;

                const int first_cnt = aic_collect_neighbors(sp, start, first_type, neighbors);
                for (int i = 0; i < first_cnt; ++i) {
                    const int v = neighbors[i];
                    if (!aic_node_active_for_digit(st, sp, bit, v)) continue;
                    if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
                    queue_state[qt] = (v << 1) | first_type;
                    queue_depth[qt] = 1;
                    ++qt;
                    if (first_type == 0) vis_even[v] = 1;
                    else vis_odd[v] = 1;
                }

                while (qh < qt) {
                    const int state = queue_state[qh];
                    const int dep = queue_depth[qh];
                    ++qh;

                    const int u = (state >> 1);
                    const int last_type = (state & 1);
                    const int next_type = 1 - last_type;
                    if (dep >= depth_cap) continue;

                    const int next_cnt = aic_collect_neighbors(sp, u, next_type, neighbors);
                    for (int i = 0; i < next_cnt; ++i) {
                        const int v = neighbors[i];
                        if (!aic_node_active_for_digit(st, sp, bit, v)) continue;
                        const int nd = dep + 1;

                        int* const vis = (next_type == 0) ? vis_even : vis_odd;
                        int* const vis_other = (next_type == 0) ? vis_odd : vis_even;

                        if (vis_other[v] != 0) {
                            if (first_type == 0) {
                                const ApplyResult er = aic_eliminate_start_candidate(st, sp, bit, start);
                                if (er == ApplyResult::Contradiction) return er;
                                if (er == ApplyResult::Progress) {
                                    used_flag = true;
                                    return er;
                                }
                            } else {
                                const ApplyResult er = aic_force_singleton_start_true(st, sp, bit, start);
                                if (er == ApplyResult::Contradiction) return er;
                                if (er == ApplyResult::Progress) {
                                    used_flag = true;
                                    return er;
                                }
                            }
                        }

                        if (v == start) {
                            // Continuous nice loop style closures are certified elsewhere.
                            continue;
                        }

                        // Odd-length chain starting with strong link: endpoints are negative,
                        // so any candidate seeing both endpoints can be eliminated.
                        if (first_type == 1 && nd >= 3 && (nd & 1) == 1) {
                            const ApplyResult er = aic_eliminate_common_peers(st, sp, bit, start, v);
                            if (er == ApplyResult::Contradiction) return er;
                            if (er == ApplyResult::Progress) {
                                used_flag = true;
                                return er;
                            }
                        }

                        if (vis[v] != 0) continue;
                        vis[v] = 1;
                        if (qt >= shared::ExactPatternScratchpad::MAX_BFS) continue;
                        queue_state[qt] = (v << 1) | next_type;
                        queue_depth[qt] = nd;
                        ++qt;
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

// Lightweight implication driver used across P7/P8.
inline ApplyResult bounded_implication_core(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    int max_iters,
    bool allow_weak_start,
    bool& used_flag) {
    (void)r;
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int depth_cap = std::clamp(max_iters, 6, 28);
    const ApplyResult ar = alternating_chain_core(st, depth_cap, allow_weak_start, used_flag);
    s.elapsed_ns += get_current_time_ns() - t0;
    return ar;
}

inline ApplyResult bounded_implication_core(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    int max_iters,
    bool& used_flag) {
    return bounded_implication_core(st, s, r, max_iters, false, used_flag);
}

// Backward-compatible alias for older modules.
inline ApplyResult bounded_implication_proxy(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    int max_iters,
    bool& used_flag) {
    return bounded_implication_core(st, s, r, max_iters, false, used_flag);
}

inline ApplyResult apply_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    bool used = false;
    const int depth_cap = std::clamp(8 + st.topo->n / 3, 8, 16);
    const ApplyResult res = bounded_implication_core(st, s, r, depth_cap, false, used);
    if (res == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_aic = true;
    }
    return res;
}

inline ApplyResult apply_grouped_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    bool used = false;
    const int depth_cap = std::clamp(12 + st.topo->n / 2, 12, 24);
    const ApplyResult res = bounded_implication_core(st, s, r, depth_cap, true, used);
    if (res == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_grouped_aic = true;
    }
    return res;
}

} // namespace sudoku_hpc::logic::p7_nightmare
