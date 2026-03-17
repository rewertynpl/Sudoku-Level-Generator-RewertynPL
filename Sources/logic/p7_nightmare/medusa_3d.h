// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: medusa_3d.h (Level 7 - Nightmare)
// Description: 3D Medusa-oriented chain coloring pass focused on boards with
// rich bivalue structure (zero-allocation).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../../config/bit_utils.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline int medusa_find_node_for_digit(
    const shared::ExactPatternScratchpad& sp,
    int base_node,
    uint64_t bit) {
    if (base_node < 0 || base_node >= sp.medusa_node_count) return -1;
    if (sp.medusa_node_bit[base_node] == bit) return base_node;
    const int next = base_node + 1;
    if (next < sp.medusa_node_count && sp.medusa_node_cell[next] == sp.medusa_node_cell[base_node] &&
        sp.medusa_node_bit[next] == bit) {
        return next;
    }
    return -1;
}

inline int medusa_find_node(
    const shared::ExactPatternScratchpad& sp,
    int cell,
    uint64_t bit) {
    for (int i = 0; i < sp.medusa_node_count; ++i) {
        if (sp.medusa_node_cell[i] == cell && sp.medusa_node_bit[i] == bit) return i;
    }
    return -1;
}

inline int medusa_get_or_add_node(
    shared::ExactPatternScratchpad& sp,
    int cell,
    uint64_t bit) {
    const int existing = medusa_find_node(sp, cell, bit);
    if (existing >= 0) return existing;
    if (sp.medusa_node_count >= shared::ExactPatternScratchpad::MAX_MEDUSA_NODES) return -1;
    const int node = sp.medusa_node_count++;
    sp.medusa_node_cell[node] = cell;
    sp.medusa_node_bit[node] = bit;
    sp.medusa_degree[node] = 0;
    sp.medusa_color[node] = 0;
    return node;
}

inline void medusa_add_edge(shared::ExactPatternScratchpad& sp, int u, int v) {
    if (u < 0 || v < 0 || u == v) return;
    for (int e = 0; e < sp.medusa_edge_count; ++e) {
        const int a = sp.medusa_edge_u[e];
        const int b = sp.medusa_edge_v[e];
        if ((a == u && b == v) || (a == v && b == u)) return;
    }
    if (sp.medusa_edge_count >= shared::ExactPatternScratchpad::MAX_MEDUSA_EDGES) return;
    sp.medusa_edge_u[sp.medusa_edge_count] = u;
    sp.medusa_edge_v[sp.medusa_edge_count] = v;
    ++sp.medusa_edge_count;
}

inline bool build_medusa_bivalue_graph(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    int& bivalue_count) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;

    sp.medusa_node_count = 0;
    sp.medusa_edge_count = 0;
    bivalue_count = 0;

    std::fill_n(sp.cell_to_node, nn, -1);
    std::fill_n(sp.medusa_offsets, shared::ExactPatternScratchpad::MAX_MEDUSA_NODES + 1, 0);
    std::fill_n(sp.medusa_degree, shared::ExactPatternScratchpad::MAX_MEDUSA_NODES, 0);
    std::fill_n(sp.medusa_cursor, shared::ExactPatternScratchpad::MAX_MEDUSA_NODES, 0);
    std::fill_n(sp.medusa_color, shared::ExactPatternScratchpad::MAX_MEDUSA_NODES, 0);

    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        uint64_t mask = st.cands[idx];
        if (std::popcount(mask) != 2) continue;

        const uint64_t b1 = config::bit_lsb(mask);
        mask = config::bit_clear_lsb_u64(mask);
        const uint64_t b2 = config::bit_lsb(mask);
        if (b1 == 0ULL || b2 == 0ULL) continue;

        const int n1 = medusa_get_or_add_node(sp, idx, b1);
        const int n2 = medusa_get_or_add_node(sp, idx, b2);
        if (n1 < 0 || n2 < 0) return false;

        sp.cell_to_node[idx] = std::min(n1, n2);
        ++bivalue_count;
        medusa_add_edge(sp, n1, n2);
    }

    const int bivalue_cap = std::min(1536, std::max(256, n * 32));
    if (bivalue_count < 3 || bivalue_count > bivalue_cap) return false;

    int house_cells[64]{};
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        for (int h = 0; h < house_count; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                if (cnt < 64) house_cells[cnt] = idx;
                ++cnt;
                if (cnt > 2) break;
            }
            if (cnt != 2) continue;

            const int u = medusa_get_or_add_node(sp, house_cells[0], bit);
            const int v = medusa_get_or_add_node(sp, house_cells[1], bit);
            if (u < 0 || v < 0) return false;
            medusa_add_edge(sp, u, v);
        }
    }

    if (sp.medusa_edge_count == 0 || sp.medusa_node_count < 2) return false;

    for (int e = 0; e < sp.medusa_edge_count; ++e) {
        ++sp.medusa_degree[sp.medusa_edge_u[e]];
        ++sp.medusa_degree[sp.medusa_edge_v[e]];
    }

    int total = 0;
    for (int i = 0; i < sp.medusa_node_count; ++i) {
        sp.medusa_offsets[i] = total;
        total += sp.medusa_degree[i];
    }
    sp.medusa_offsets[sp.medusa_node_count] = total;
    if (total > shared::ExactPatternScratchpad::MAX_MEDUSA_ADJ) return false;

    for (int i = 0; i < sp.medusa_node_count; ++i) {
        sp.medusa_cursor[i] = sp.medusa_offsets[i];
    }
    for (int e = 0; e < sp.medusa_edge_count; ++e) {
        const int u = sp.medusa_edge_u[e];
        const int v = sp.medusa_edge_v[e];
        sp.medusa_adj[sp.medusa_cursor[u]++] = v;
        sp.medusa_adj[sp.medusa_cursor[v]++] = u;
    }
    return true;
}

inline ApplyResult medusa_eliminate_color(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    int color) {
    for (int node = 0; node < sp.medusa_node_count; ++node) {
        if (sp.medusa_color[node] != color) continue;
        const ApplyResult er = st.eliminate(sp.medusa_node_cell[node], sp.medusa_node_bit[node]);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult medusa_eliminate_other_bits_in_cell(
    CandidateState& st,
    int cell,
    uint64_t keep_bit) {
    uint64_t drop = st.cands[cell] & ~keep_bit;
    while (drop != 0ULL) {
        const uint64_t bit = config::bit_lsb(drop);
        drop = config::bit_clear_lsb_u64(drop);
        const ApplyResult er = st.eliminate(cell, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline bool medusa_candidate_conflicts(
    const CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    int node,
    int cell,
    uint64_t bit) {
    const int src_cell = sp.medusa_node_cell[node];
    const uint64_t src_bit = sp.medusa_node_bit[node];
    if (src_cell == cell) {
        return src_bit != bit;
    }
    return src_bit == bit && st.is_peer(cell, src_cell);
}

inline bool medusa_candidate_conflicts_with_color(
    const CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    const int* component_nodes,
    int comp_size,
    int color,
    int cell,
    uint64_t bit) {
    for (int i = 0; i < comp_size; ++i) {
        const int node = component_nodes[i];
        if (sp.medusa_color[node] != color) continue;
        if (medusa_candidate_conflicts(st, sp, node, cell, bit)) return true;
    }
    return false;
}

inline ApplyResult medusa_component_same_color_contradiction(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    const int* component_nodes,
    int comp_size) {
    for (int i = 0; i < comp_size; ++i) {
        const int u = component_nodes[i];
        const int cell_u = sp.medusa_node_cell[u];
        const uint64_t bit_u = sp.medusa_node_bit[u];
        for (int j = i + 1; j < comp_size; ++j) {
            const int v = component_nodes[j];
            if (sp.medusa_color[u] != sp.medusa_color[v]) continue;
            if (cell_u == sp.medusa_node_cell[v] && bit_u != sp.medusa_node_bit[v]) {
                return medusa_eliminate_color(st, sp, sp.medusa_color[u]);
            }
            if (bit_u == sp.medusa_node_bit[v] && st.is_peer(cell_u, sp.medusa_node_cell[v])) {
                return medusa_eliminate_color(st, sp, sp.medusa_color[u]);
            }
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult medusa_component_pass(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    int start_node) {
    int qh = 0;
    int qt = 0;
    int comp_size = 0;

    sp.bfs_queue[qt++] = start_node;
    sp.medusa_color[start_node] = 1;

    while (qh < qt) {
        const int u = sp.bfs_queue[qh++];
        sp.bfs_parent[comp_size++] = u;

        const int p0 = sp.medusa_offsets[u];
        const int p1 = sp.medusa_offsets[u + 1];
        for (int p = p0; p < p1; ++p) {
            const int v = sp.medusa_adj[p];
            if (sp.medusa_color[v] == 0) {
                sp.medusa_color[v] = -sp.medusa_color[u];
                if (qt >= shared::ExactPatternScratchpad::MAX_BFS) return ApplyResult::NoProgress;
                sp.bfs_queue[qt++] = v;
            }
        }
    }

    {
        const ApplyResult er = medusa_component_same_color_contradiction(st, sp, sp.bfs_parent, comp_size);
        if (er != ApplyResult::NoProgress) return er;
    }

    const int nn = st.topo->nn;
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;

    for (int color : {1, -1}) {
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            uint64_t m = st.cands[idx];
            if (std::popcount(m) <= 1) continue;

            bool any_conflict = false;
            bool all_conflict = true;
            while (m != 0ULL) {
                const uint64_t bit = config::bit_lsb(m);
                m = config::bit_clear_lsb_u64(m);
                const bool conflict = medusa_candidate_conflicts_with_color(
                    st, sp, sp.bfs_parent, comp_size, color, idx, bit);
                any_conflict = any_conflict || conflict;
                if (!conflict) {
                    all_conflict = false;
                    break;
                }
            }
            if (any_conflict && all_conflict) {
                return medusa_eliminate_color(st, sp, color);
            }
        }

        for (int h = 0; h < house_count; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            for (int d = 1; d <= st.topo->n; ++d) {
                const uint64_t bit = (1ULL << (d - 1));
                bool any_place = false;
                bool all_conflict = true;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                    any_place = true;
                    if (!medusa_candidate_conflicts_with_color(
                            st, sp, sp.bfs_parent, comp_size, color, idx, bit)) {
                        all_conflict = false;
                        break;
                    }
                }
                if (any_place && all_conflict) {
                    return medusa_eliminate_color(st, sp, color);
                }
            }
        }
    }

    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        const uint64_t cell_mask = st.cands[idx];
        if (std::popcount(cell_mask) <= 1) continue;

        uint64_t pos_bits = 0ULL;
        uint64_t neg_bits = 0ULL;
        for (int i = 0; i < comp_size; ++i) {
            const int node = sp.bfs_parent[i];
            if (sp.medusa_node_cell[node] != idx) continue;
            if (sp.medusa_color[node] > 0) pos_bits |= sp.medusa_node_bit[node];
            if (sp.medusa_color[node] < 0) neg_bits |= sp.medusa_node_bit[node];
        }

        if (std::popcount(pos_bits) == 1) {
            uint64_t other = cell_mask & ~pos_bits;
            bool all_see_neg = (other != 0ULL);
            while (other != 0ULL) {
                const uint64_t bit = config::bit_lsb(other);
                other = config::bit_clear_lsb_u64(other);
                if (!medusa_candidate_conflicts_with_color(st, sp, sp.bfs_parent, comp_size, -1, idx, bit)) {
                    all_see_neg = false;
                    break;
                }
            }
            if (all_see_neg) return medusa_eliminate_other_bits_in_cell(st, idx, pos_bits);
        }

        if (std::popcount(neg_bits) == 1) {
            uint64_t other = cell_mask & ~neg_bits;
            bool all_see_pos = (other != 0ULL);
            while (other != 0ULL) {
                const uint64_t bit = config::bit_lsb(other);
                other = config::bit_clear_lsb_u64(other);
                if (!medusa_candidate_conflicts_with_color(st, sp, sp.bfs_parent, comp_size, 1, idx, bit)) {
                    all_see_pos = false;
                    break;
                }
            }
            if (all_see_pos) return medusa_eliminate_other_bits_in_cell(st, idx, neg_bits);
        }
    }

    for (int h = 0; h < house_count; ++h) {
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
        for (int d = 1; d <= st.topo->n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int colored_pos = -1;
            int colored_neg = -1;
            int colored_pos_count = 0;
            int colored_neg_count = 0;
            int place_count = 0;

            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                ++place_count;
                for (int i = 0; i < comp_size; ++i) {
                    const int node = sp.bfs_parent[i];
                    if (sp.medusa_node_cell[node] != idx || sp.medusa_node_bit[node] != bit) continue;
                    if (sp.medusa_color[node] > 0) {
                        if (colored_pos != idx) {
                            colored_pos = idx;
                            ++colored_pos_count;
                        }
                    } else if (sp.medusa_color[node] < 0) {
                        if (colored_neg != idx) {
                            colored_neg = idx;
                            ++colored_neg_count;
                        }
                    }
                }
            }
            if (place_count <= 1) continue;

            if (colored_pos_count == 1 && colored_pos >= 0) {
                bool forces_pos = true;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (idx == colored_pos || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                    if (!medusa_candidate_conflicts_with_color(st, sp, sp.bfs_parent, comp_size, -1, idx, bit)) {
                        forces_pos = false;
                        break;
                    }
                }
                if (forces_pos) {
                    for (int p = p0; p < p1; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (idx == colored_pos || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                        const ApplyResult er = st.eliminate(idx, bit);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }

            if (colored_neg_count == 1 && colored_neg >= 0) {
                bool forces_neg = true;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (idx == colored_neg || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                    if (!medusa_candidate_conflicts_with_color(st, sp, sp.bfs_parent, comp_size, 1, idx, bit)) {
                        forces_neg = false;
                        break;
                    }
                }
                if (forces_neg) {
                    for (int p = p0; p < p1; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (idx == colored_neg || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                        const ApplyResult er = st.eliminate(idx, bit);
                        if (er != ApplyResult::NoProgress) return er;
                    }
                }
            }
        }
    }

    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        uint64_t m = st.cands[idx];
        while (m != 0ULL) {
            const uint64_t bit = config::bit_lsb(m);
            m = config::bit_clear_lsb_u64(m);
            bool sees_pos = false;
            bool sees_neg = false;
            for (int i = 0; i < comp_size; ++i) {
                const int node = sp.bfs_parent[i];
                if (!medusa_candidate_conflicts(st, sp, node, idx, bit)) continue;
                sees_pos = sees_pos || (sp.medusa_color[node] > 0);
                sees_neg = sees_neg || (sp.medusa_color[node] < 0);
                if (sees_pos && sees_neg) {
                    return st.eliminate(idx, bit);
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_medusa_3d(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    const int min_givens = std::max(4, n / 2);
    if (n > 64 || st.board->empty_cells > (nn - min_givens)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    auto& sp = shared::exact_pattern_scratchpad();
    int bivalue_count = 0;
    if (!build_medusa_bivalue_graph(st, sp, bivalue_count)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    for (int node = 0; node < sp.medusa_node_count; ++node) {
        if (sp.medusa_color[node] != 0) continue;
        const ApplyResult ar = medusa_component_pass(st, sp, node);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += get_current_time_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_medusa_3d = true;
            s.elapsed_ns += get_current_time_ns() - t0;
            return ar;
        }
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
