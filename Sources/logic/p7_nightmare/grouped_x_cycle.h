// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: grouped_x_cycle.h (Level 7 - Nightmare)
// Description: Grouped X-Cycle eliminations over grouped strong/weak link
// graphs per digit. Zero-allocation, asymmetry-safe, and compatible with
// grouped nodes stored in ExactPatternScratchpad.
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

inline bool gx_is_weak_neighbor(const shared::ExactPatternScratchpad& sp, int u, int v) {
    const int p0 = sp.dyn_weak_offsets[u];
    const int p1 = sp.dyn_weak_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_weak_adj[p] == v) return true;
    }
    return false;
}

inline bool gx_node_active_for_digit(
    const CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    int node,
    uint64_t bit) {

    const int p0 = sp.adj_offsets[node];
    const int p1 = sp.adj_offsets[node + 1];
    for (int p = p0; p < p1; ++p) {
        const int cell = sp.adj_flat[p];
        if (st.board->values[cell] == 0 && (st.cands[cell] & bit) != 0ULL) {
            return true;
        }
    }
    return false;
}

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
        if (st.board->values[cell] != 0) continue;
        if ((st.cands[cell] & bit) == 0ULL) continue;

        const ApplyResult er = st.eliminate(cell, bit);
        if (er == ApplyResult::Contradiction) return er;
        if (er == ApplyResult::Progress) final_res = ApplyResult::Progress;
    }
    return final_res;
}

inline bool gx_component_has_weak_same_color(
    const shared::ExactPatternScratchpad& sp,
    const int* color,
    const int* comp_nodes,
    int comp_size,
    bool weak_same_color[2]) {

    bool any = false;
    weak_same_color[0] = false;
    weak_same_color[1] = false;

    for (int i = 0; i < comp_size; ++i) {
        const int u = comp_nodes[i];
        const int wu0 = sp.dyn_weak_offsets[u];
        const int wu1 = sp.dyn_weak_offsets[u + 1];
        for (int p = wu0; p < wu1; ++p) {
            const int v = sp.dyn_weak_adj[p];
            if (u >= v) continue;
            if (color[v] < 0) continue;
            if (color[u] != color[v]) continue;
            weak_same_color[color[u]] = true;
            any = true;
        }
    }
    return any;
}

inline ApplyResult gx_trap_eliminate_external_targets(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    const int* color,
    const int* comp_nodes,
    int comp_size,
    uint64_t bit,
    bool& any_progress) {

    for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
        const int t_cell = sp.dyn_digit_cells[i];
        if (st.board->values[t_cell] != 0) continue;
        if ((st.cands[t_cell] & bit) == 0ULL) continue;

        const int owner = sp.dyn_cell_to_node[t_cell];
        if (owner >= 0 && color[owner] >= 0) continue;

        bool sees0 = false;
        bool sees1 = false;
        for (int k = 0; k < comp_size; ++k) {
            const int u = comp_nodes[k];
            if (!shared::node_sees_cell(st, sp, u, t_cell)) continue;
            if (color[u] == 0) sees0 = true;
            else if (color[u] == 1) sees1 = true;
            if (sees0 && sees1) break;
        }
        if (!(sees0 && sees1)) continue;

        const ApplyResult er = st.eliminate(t_cell, bit);
        if (er == ApplyResult::Contradiction) return er;
        any_progress = any_progress || (er == ApplyResult::Progress);
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_grouped_x_cycle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool any_progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int* const color = sp.visited;          // -1: outside current component, 0/1: colored
    int* const queue = sp.bfs_queue;        // BFS queue over node ids
    int* const comp_nodes = sp.chain_cell;  // component nodes
    int* const comp_mark = sp.bfs_depth;    // per-component visitation stamp (0/1 here)

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 4 || sp.dyn_strong_edge_count < 2) continue;

        std::fill_n(color, sp.dyn_node_count, -1);

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            if (color[start] != -1) continue;
            if (sp.dyn_strong_degree[start] == 0) continue;
            if (!gx_node_active_for_digit(st, sp, start, bit)) continue;

            std::fill_n(comp_mark, sp.dyn_node_count, 0);
            int qh = 0;
            int qt = 0;
            int comp_size = 0;
            bool strong_same_color[2] = {false, false};
            bool component_has_cycle = false;

            color[start] = 0;
            comp_mark[start] = 1;
            if (qt < shared::ExactPatternScratchpad::MAX_BFS) {
                queue[qt++] = start;
            }

            while (qh < qt) {
                const int u = queue[qh++];
                if (comp_size < shared::ExactPatternScratchpad::MAX_CHAIN) {
                    comp_nodes[comp_size++] = u;
                }

                const int c_u = color[u];
                const int next_color = 1 - c_u;
                const int p0 = sp.dyn_strong_offsets[u];
                const int p1 = sp.dyn_strong_offsets[u + 1];
                for (int p = p0; p < p1; ++p) {
                    const int v = sp.dyn_strong_adj[p];
                    if (!gx_node_active_for_digit(st, sp, v, bit)) continue;

                    if (color[v] == -1) {
                        color[v] = next_color;
                        if (!comp_mark[v] && qt < shared::ExactPatternScratchpad::MAX_BFS) {
                            comp_mark[v] = 1;
                            queue[qt++] = v;
                        }
                    } else if (color[v] == c_u) {
                        strong_same_color[c_u] = true;
                        component_has_cycle = true;
                    } else {
                        component_has_cycle = true;
                    }
                }
            }

            if (comp_size < 2) continue;

            int strong_edges_twice = 0;
            int weak_edges_twice = 0;
            for (int i = 0; i < comp_size; ++i) {
                const int u = comp_nodes[i];
                strong_edges_twice += sp.dyn_strong_degree[u];
                weak_edges_twice += sp.dyn_weak_degree[u];
            }
            if ((strong_edges_twice >> 1) >= comp_size) component_has_cycle = true;
            if ((weak_edges_twice >> 1) >= comp_size) component_has_cycle = true;

            bool weak_same_color[2] = {false, false};
            gx_component_has_weak_same_color(sp, color, comp_nodes, comp_size, weak_same_color);

            if (!component_has_cycle && !strong_same_color[0] && !strong_same_color[1] &&
                !weak_same_color[0] && !weak_same_color[1]) {
                continue;
            }

            for (int bad_color = 0; bad_color <= 1; ++bad_color) {
                if (!strong_same_color[bad_color] && !weak_same_color[bad_color]) continue;
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

            const ApplyResult trap = gx_trap_eliminate_external_targets(
                st, sp, color, comp_nodes, comp_size, bit, any_progress);
            if (trap == ApplyResult::Contradiction) {
                s.elapsed_ns += get_current_time_ns() - t0;
                return trap;
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
