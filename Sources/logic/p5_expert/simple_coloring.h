//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"

namespace sudoku_hpc::logic::p5_expert {

inline ApplyResult apply_simple_coloring(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool any_progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int* color = sp.visited;
    int* queue = sp.bfs_queue;
    int* comp_nodes = sp.chain_cell;
    int row_color0[64]{};
    int row_color1[64]{};
    int col_color0[64]{};
    int col_color1[64]{};
    int box_color0[64]{};
    int box_color1[64]{};

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_strong_edge_count == 0) continue;

        std::fill_n(color, sp.dyn_node_count, -1);

        for (int start_node = 0; start_node < sp.dyn_node_count; ++start_node) {
            if (sp.dyn_strong_degree[start_node] == 0) continue;
            if (color[start_node] != -1) continue;

            int qh = 0;
            int qt = 0;
            int comp_size = 0;
            bool conflict0 = false;
            bool conflict1 = false;

            color[start_node] = 0;
            queue[qt++] = start_node;

            while (qh < qt) {
                const int u = queue[qh++];
                if (comp_size < shared::ExactPatternScratchpad::MAX_CHAIN) {
                    comp_nodes[comp_size++] = u;
                }

                const int my_color = color[u];
                const int next_color = 1 - my_color;
                const int off0 = sp.dyn_strong_offsets[u];
                const int off1 = sp.dyn_strong_offsets[u + 1];

                for (int e = off0; e < off1; ++e) {
                    const int v = sp.dyn_strong_adj[e];
                    if (color[v] == -1) {
                        color[v] = next_color;
                        if (qt < shared::ExactPatternScratchpad::MAX_BFS) {
                            queue[qt++] = v;
                        }
                    } else if (color[v] == my_color) {
                        if (my_color == 0) conflict0 = true;
                        else conflict1 = true;
                    }
                }
            }

            std::fill_n(row_color0, n, -1);
            std::fill_n(row_color1, n, -1);
            std::fill_n(col_color0, n, -1);
            std::fill_n(col_color1, n, -1);
            std::fill_n(box_color0, n, -1);
            std::fill_n(box_color1, n, -1);

            for (int i = 0; i < comp_size; ++i) {
                const int u = comp_nodes[i];
                const int cell = sp.dyn_node_to_cell[u];
                const int rr = st.topo->cell_row[cell];
                const int cc = st.topo->cell_col[cell];
                const int bb = st.topo->cell_box[cell];

                if (color[u] == 0) {
                    if (row_color0[rr] >= 0 || col_color0[cc] >= 0 || box_color0[bb] >= 0) {
                        conflict0 = true;
                    } else {
                        row_color0[rr] = cell;
                        col_color0[cc] = cell;
                        box_color0[bb] = cell;
                    }
                } else {
                    if (row_color1[rr] >= 0 || col_color1[cc] >= 0 || box_color1[bb] >= 0) {
                        conflict1 = true;
                    } else {
                        row_color1[rr] = cell;
                        col_color1[cc] = cell;
                        box_color1[bb] = cell;
                    }
                }
            }

            // Color wrap: if a color appears twice in one house, that color is false.
            if (conflict0 || conflict1) {
                const int bad_color = conflict0 ? 0 : 1;
                for (int i = 0; i < comp_size; ++i) {
                    const int u = comp_nodes[i];
                    if (color[u] != bad_color) continue;
                    const int cell = sp.dyn_node_to_cell[u];
                    const ApplyResult er = st.eliminate(cell, bit);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return er;
                    }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_simple_coloring = true;
                        any_progress = true;
                    }
                }
                continue;
            }

            // Color trap: a cell seeing both colors cannot contain the digit.
            int c0_count = 0;
            int c1_count = 0;
            int* const c0_cells = sp.bfs_depth;
            int* const c1_cells = sp.bfs_parent;
            for (int i = 0; i < comp_size; ++i) {
                const int u = comp_nodes[i];
                if (color[u] == 0) c0_cells[c0_count++] = sp.dyn_node_to_cell[u];
                else c1_cells[c1_count++] = sp.dyn_node_to_cell[u];
            }

            for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
                const int t_cell = sp.dyn_digit_cells[i];
                const int t_node = sp.dyn_cell_to_node[t_cell];
                if (t_node >= 0 && color[t_node] != -1) continue;

                bool sees0 = false;
                for (int k = 0; k < c0_count; ++k) {
                    if (st.is_peer(t_cell, c0_cells[k])) {
                        sees0 = true;
                        break;
                    }
                }
                if (!sees0) continue;

                bool sees1 = false;
                for (int k = 0; k < c1_count; ++k) {
                    if (st.is_peer(t_cell, c1_cells[k])) {
                        sees1 = true;
                        break;
                    }
                }
                if (!sees1) continue;

                const ApplyResult er = st.eliminate(t_cell, bit);
                if (er == ApplyResult::Contradiction) {
                    s.elapsed_ns += st.now_ns() - t0;
                    return er;
                }
                if (er == ApplyResult::Progress) {
                    ++s.hit_count;
                    r.used_simple_coloring = true;
                    any_progress = true;
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return any_progress ? ApplyResult::Progress : ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p5_expert
