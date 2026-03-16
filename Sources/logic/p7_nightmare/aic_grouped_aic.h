// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: aic_grouped_aic.h (Level 7 - Nightmare)
// Description: Direct Alternating Inference Chains (AIC) and Grouped AIC
// on strong/weak candidate graphs per digit, zero-allocation.
// Zaktualizowano o poprawne wsparcie dla Węzłów Grupowych (Grouped Nodes).
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
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

inline int aic_collect_neighbors(
    const shared::ExactPatternScratchpad& sp,
    int u,
    int edge_type, // 1=strong, 0=weak-only
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
        if (aic_is_strong_neighbor(sp, u, v)) continue;
        out[cnt++] = v;
    }
    return cnt;
}

// Eliminacja kandydatów widzących łącznie dwa podane węzły. 
// W przypadku węzłów grupowych (Grouped Nodes), komórka musi widzieć WSZYSTKIE komórki z każdego węzła.
inline ApplyResult aic_eliminate_common_peers(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int a_node,
    int b_node) {
    
    for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
        const int idx = sp.dyn_digit_cells[i];
        
        // Nie eliminujemy kandydatów, którzy są częścią samych węzłów krańcowych
        bool in_a = false, in_b = false;
        for (int p = sp.adj_offsets[a_node]; p < sp.adj_offsets[a_node + 1]; ++p) {
            if (sp.adj_flat[p] == idx) { in_a = true; break; }
        }
        if (in_a) continue;
        
        for (int p = sp.adj_offsets[b_node]; p < sp.adj_offsets[b_node + 1]; ++p) {
            if (sp.adj_flat[p] == idx) { in_b = true; break; }
        }
        if (in_b) continue;

        // Komórka docelowa (Target) musi widzieć każdy element grupy A i każdy element grupy B
        if (!shared::node_sees_cell(st, sp, a_node, idx)) continue;
        if (!shared::node_sees_cell(st, sp, b_node, idx)) continue;
        
        const ApplyResult er = st.eliminate(idx, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

// Eliminacja kandydata z konkretnego węzła startowego. W przypadku węzłów grupowych, 
// eliminacja zostaje zaaplikowana na wszystkie komórki wchodzące w skład tego węzła.
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
        const ApplyResult er = st.eliminate(cell, bit);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) final_res = ApplyResult::Progress;
    }
    return final_res;
}

// Główny rdzeń przeszukiwania naprzemiennego łańcucha (AIC i Grouped AIC).
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
            // Pomijamy węzeł, jeśli któraś z jego komórek została już wyznaczona (rozwiązana)
            bool any_solved = false;
            for (int p = sp.adj_offsets[start]; p < sp.adj_offsets[start + 1]; ++p) {
                if (st.board->values[sp.adj_flat[p]] != 0) { any_solved = true; break; }
            }
            if (any_solved) continue;

            for (int first_type = 1; first_type >= 0; --first_type) {
                if (first_type == 0 && !allow_weak_start) continue;

                std::fill_n(vis_even, sp.dyn_node_count, 0);
                std::fill_n(vis_odd, sp.dyn_node_count, 0);

                int qh = 0;
                int qt = 0;

                const int first_cnt = aic_collect_neighbors(sp, start, first_type, neighbors);
                for (int i = 0; i < first_cnt; ++i) {
                    if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
                    const int v = neighbors[i];
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
                        const int nd = dep + 1;

                        int* const vis = (next_type == 0) ? vis_even : vis_odd;
                        int* const vis_other = (next_type == 0) ? vis_odd : vis_even;
                        
                        // Zjawisko sprzeczności parzystości w łańcuchu dla tego samego węzła.
                        // Oznacza, że założenie o węźle startowym było z gruntu błędne.
                        if (vis_other[v] != 0) {
                            if (first_type == 0) {
                                // Założenie: start=TRUE prowadzi do sprzeczności -> start=FALSE
                                const ApplyResult er = aic_eliminate_start_candidate(st, sp, bit, start);
                                if (er == ApplyResult::Contradiction) return er;
                                if (er == ApplyResult::Progress) {
                                    used_flag = true;
                                    return er;
                                }
                            } else {
                                // Założenie: start=FALSE prowadzi do sprzeczności -> start=TRUE
                                // Jeżeli start to pojedyncza komórka (nie grupa), to eliminujemy w niej wszystkie inne bity
                                if (sp.adj_offsets[start + 1] - sp.adj_offsets[start] == 1) {
                                    const int cell = sp.adj_flat[sp.adj_offsets[start]];
                                    uint64_t other_cands = st.cands[cell] & ~bit;
                                    if (other_cands != 0ULL) {
                                        const ApplyResult er = st.eliminate(cell, other_cands);
                                        if (er == ApplyResult::Contradiction) return er;
                                        if (er == ApplyResult::Progress) {
                                            used_flag = true;
                                            return er;
                                        }
                                    }
                                }
                            }
                        }

                        if (v == start) {
                            // Cykle zamykające się wokół startu są certyfikowane w zewnętrznych
                            // algorytmach Continuous Nice Loop, tutaj omijamy, żeby uniknąć hałasu.
                            continue;
                        }

                        // Eliminacje na przecięciu oddziaływań krańcowych węzłów łańcucha AIC.
                        // Łańcuchy o nieparzystej długości zaczynające się silnym ogniwem - krańce są ujemne.
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