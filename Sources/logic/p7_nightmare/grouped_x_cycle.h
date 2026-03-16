// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: grouped_x_cycle.h (Level 7 - Nightmare)
// Description: Direct grouped X-Cycle style elimination on strong/weak graph
// per digit (zero-allocation). Zaktualizowano o pełne i poprawne wsparcie 
// dla Węzłów Grupowych (Grouped Nodes) korzystających z tablicy adj_flat.
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

inline bool gx_is_strong_neighbor(const shared::ExactPatternScratchpad& sp, int u, int v) {
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

// Funkcja pomocnicza: Eliminuje kandydata ze wszystkich komórek wchodzących w skład węzła grupowego.
inline ApplyResult gxc_eliminate_node(
    CandidateState& st, 
    const shared::ExactPatternScratchpad& sp, 
    int node, 
    uint64_t bit) {
    
    ApplyResult final_res = ApplyResult::NoProgress;
    const int p0 = sp.adj_offsets[node];
    const int p1 = sp.adj_offsets[node + 1];
    
    for (int p = p0; p < p1; ++p) {
        const int cell = sp.adj_flat[p];
        const ApplyResult er = st.eliminate(cell, bit);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) final_res = ApplyResult::Progress;
    }
    return final_res;
}

inline ApplyResult apply_grouped_x_cycle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool any_progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int* const color = sp.visited;       // -1/0/1 for current digit graph nodes
    int* const queue = sp.bfs_queue;     // BFS queue over node ids
    int* const comp_nodes = sp.chain_cell;
    int* const comp_mark = sp.bfs_depth; // marks node in current component

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 4 || sp.dyn_strong_edge_count < 2) continue;

        std::fill_n(color, sp.dyn_node_count, -1);

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            // Pomijamy węzeł, jeśli jakakolwiek jego komórka jest już wypełniona wartością
            bool any_solved = false;
            for (int p = sp.adj_offsets[start]; p < sp.adj_offsets[start + 1]; ++p) {
                if (st.board->values[sp.adj_flat[p]] != 0) { 
                    any_solved = true; 
                    break; 
                }
            }
            if (any_solved) continue;

            if (sp.dyn_strong_degree[start] == 0 || color[start] != -1) continue;

            std::fill_n(comp_mark, sp.dyn_node_count, 0);
            int qh = 0;
            int qt = 0;
            int comp_size = 0;
            bool color_conflict[2] = {false, false};

            color[start] = 0;
            if (qt < shared::ExactPatternScratchpad::MAX_BFS) {
                queue[qt++] = start;
            }
            comp_mark[start] = 1;

            // Color component by strong links.
            while (qh < qt) {
                const int u = queue[qh++];
                if (comp_size < shared::ExactPatternScratchpad::MAX_CHAIN) {
                    comp_nodes[comp_size++] = u;
                }

                const int c_u = color[u];
                const int opp = 1 - c_u;
                const int p0 = sp.dyn_strong_offsets[u];
                const int p1 = sp.dyn_strong_offsets[u + 1];
                for (int p = p0; p < p1; ++p) {
                    const int v = sp.dyn_strong_adj[p];
                    if (color[v] == -1) {
                        color[v] = opp;
                        if (!comp_mark[v]) {
                            comp_mark[v] = 1;
                            if (qt < shared::ExactPatternScratchpad::MAX_BFS) {
                                queue[qt++] = v;
                            }
                        }
                    } else if (color[v] == c_u) {
                        color_conflict[c_u] = true;
                    }
                }
            }

            // Need a loop-like component.
            int comp_strong_edges_twice = 0;
            for (int i = 0; i < comp_size; ++i) {
                const int u = comp_nodes[i];
                comp_strong_edges_twice += sp.dyn_strong_degree[u];
            }
            const bool has_cycle = (comp_strong_edges_twice / 2) >= comp_size;
            if (!has_cycle && !color_conflict[0] && !color_conflict[1]) continue;

            // Wrap-style elimination: same-color contradiction (dwa silne powiązania schodzą się z tym samym kolorem)
            for (int bad_color = 0; bad_color <= 1; ++bad_color) {
                if (!color_conflict[bad_color]) continue;
                for (int i = 0; i < comp_size; ++i) {
                    const int u = comp_nodes[i];
                    if (color[u] != bad_color) continue;
                    
                    const ApplyResult er = gxc_eliminate_node(st, sp, u, bit);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += get_current_time_ns() - t0;
                        return er;
                    }
                    any_progress = any_progress || (er == ApplyResult::Progress);
                }
            }

            // Additional wrap signal: weak edge between same color nodes.
            bool weak_same_color[2] = {false, false};
            for (int i = 0; i < comp_size; ++i) {
                const int u = comp_nodes[i];
                const int wu0 = sp.dyn_weak_offsets[u];
                const int wu1 = sp.dyn_weak_offsets[u + 1];
                for (int p = wu0; p < wu1; ++p) {
                    const int v = sp.dyn_weak_adj[p];
                    if (!comp_mark[v] || u >= v) continue;
                    if (gx_is_strong_neighbor(sp, u, v)) continue; // strong already handled
                    if (color[u] == color[v]) {
                        weak_same_color[color[u]] = true;
                    }
                }
            }
            for (int bad_color = 0; bad_color <= 1; ++bad_color) {
                if (!weak_same_color[bad_color]) continue;
                for (int i = 0; i < comp_size; ++i) {
                    const int u = comp_nodes[i];
                    if (color[u] != bad_color) continue;
                    
                    const ApplyResult er = gxc_eliminate_node(st, sp, u, bit);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += get_current_time_ns() - t0;
                        return er;
                    }
                    any_progress = any_progress || (er == ApplyResult::Progress);
                }
            }

            // Trap-style elimination: external node seeing both colors.
            // Komórka musi widzieć CAŁĄ GRUPĘ reprezentującą węzeł w przypadku węzłów grupowych.
            for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
                const int t_cell = sp.dyn_digit_cells[i];
                const int t_node = sp.dyn_cell_to_node[t_cell];
                if (t_node >= 0 && comp_mark[t_node]) continue;

                bool sees0 = false;
                bool sees1 = false;
                for (int k = 0; k < comp_size; ++k) {
                    const int u = comp_nodes[k];
                    // Zmiana: Musi widzieć wszystkie komórki grupy u
                    if (!shared::node_sees_cell(st, sp, u, t_cell)) continue;
                    
                    if (color[u] == 0) sees0 = true;
                    else sees1 = true;
                    
                    if (sees0 && sees1) break;
                }
                if (!(sees0 && sees1)) continue;

                const ApplyResult er = st.eliminate(t_cell, bit);
                if (er == ApplyResult::Contradiction) {
                    s.elapsed_ns += get_current_time_ns() - t0;
                    return er;
                }
                any_progress = any_progress || (er == ApplyResult::Progress);
            }
        }
    }

    if (any_progress) {
        ++s.hit_count;
        r.used_grouped_x_cycle = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare