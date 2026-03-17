// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: link_graph_builder.h
// Opis: System wyznaczania i utrzymywania sieci powiązań pomiędzy
//       komórkami w relacjach typu "Strong" oraz "Weak" per konkretna cyfra.
//       Kluczowe dla metod X-Cycles, Grouped X-Cycles, AIC i Forcing Chains.
//       Wersja poprawiona dla geometrii asymetrycznych oraz grouped nodes
//       bez alokacji dynamicznych (scratchpad / stack only).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <bit>
#include <cstdint>

#include "exact_pattern_scratchpad.h"
#include "../../core/candidate_state.h"

namespace sudoku_hpc::logic::shared {

inline bool dyn_has_strong_edge(const ExactPatternScratchpad& sp, int u, int v) {
    for (int i = 0; i < sp.dyn_strong_edge_count; ++i) {
        const int a = sp.dyn_strong_edge_u[i];
        const int b = sp.dyn_strong_edge_v[i];
        if ((a == u && b == v) || (a == v && b == u)) return true;
    }
    return false;
}

inline bool dyn_has_weak_edge(const ExactPatternScratchpad& sp, int u, int v) {
    for (int i = 0; i < sp.dyn_weak_edge_count; ++i) {
        const int a = sp.dyn_weak_edge_u[i];
        const int b = sp.dyn_weak_edge_v[i];
        if ((a == u && b == v) || (a == v && b == u)) return true;
    }
    return false;
}

inline void dyn_add_strong_edge(ExactPatternScratchpad& sp, int u, int v) {
    if (u < 0 || v < 0 || u == v) return;
    if (dyn_has_strong_edge(sp, u, v)) return;
    if (sp.dyn_strong_edge_count >= (ExactPatternScratchpad::MAX_HOUSES * 2)) return;

    sp.dyn_strong_edge_u[sp.dyn_strong_edge_count] = u;
    sp.dyn_strong_edge_v[sp.dyn_strong_edge_count] = v;
    ++sp.dyn_strong_edge_count;
}

inline void dyn_add_weak_edge(ExactPatternScratchpad& sp, int u, int v) {
    if (u < 0 || v < 0 || u == v) return;
    if (dyn_has_strong_edge(sp, u, v)) return;
    if (dyn_has_weak_edge(sp, u, v)) return;
    if (sp.dyn_weak_edge_count >= ExactPatternScratchpad::MAX_LINK_EDGES) return;

    sp.dyn_weak_edge_u[sp.dyn_weak_edge_count] = u;
    sp.dyn_weak_edge_v[sp.dyn_weak_edge_count] = v;
    ++sp.dyn_weak_edge_count;
}

inline int node_cell_begin(const ExactPatternScratchpad& sp, int node) {
    return sp.adj_offsets[node];
}

inline int node_cell_end(const ExactPatternScratchpad& sp, int node) {
    return sp.adj_offsets[node + 1];
}

inline int node_cell_count(const ExactPatternScratchpad& sp, int node) {
    return node_cell_end(sp, node) - node_cell_begin(sp, node);
}

inline bool nodes_intersect(const ExactPatternScratchpad& sp, int u, int v) {
    for (int i = node_cell_begin(sp, u); i < node_cell_end(sp, u); ++i) {
        const int cu = sp.adj_flat[i];
        for (int j = node_cell_begin(sp, v); j < node_cell_end(sp, v); ++j) {
            if (cu == sp.adj_flat[j]) return true;
        }
    }
    return false;
}

inline bool node_is_subset_of_house(const CandidateState& st,
                                    const ExactPatternScratchpad& sp,
                                    int node,
                                    int house_id,
                                    int n) {
    for (int i = node_cell_begin(sp, node); i < node_cell_end(sp, node); ++i) {
        const int c = sp.adj_flat[i];
        const int row_h = st.topo->cell_row[c];
        const int col_h = n + st.topo->cell_col[c];
        const int box_h = (n << 1) + st.topo->cell_box[c];
        if (house_id != row_h && house_id != col_h && house_id != box_h) {
            return false;
        }
    }
    return true;
}

// Czy wskazana komórka widzi wszystkie komórki danego grouped node.
inline bool node_sees_cell(const CandidateState& st,
                           const ExactPatternScratchpad& sp,
                           int node,
                           int cell) {
    for (int i = node_cell_begin(sp, node); i < node_cell_end(sp, node); ++i) {
        if (!st.is_peer(cell, sp.adj_flat[i])) return false;
    }
    return true;
}

// Konstruuje grouped link graph dla konkretnej cyfry.
// Węzły:
//  - singletony dla każdej komórki z cyfrą
//  - grouped nodes dla przecięć house-house (row-box, col-box, box-row, box-col)
// Krawędzie strong:
//  - standardowe "dokładnie 2 kandydaty w domku"
//  - grouped strong dla "dokładnie 2 regiony" w przecięciu domków
// Krawędzie weak:
//  - rozłączne węzły należące do wspólnego domku.
inline bool build_grouped_link_graph_for_digit(const CandidateState& st,
                                               int digit,
                                               ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (digit < 1 || digit > n) return false;

    const uint64_t bit = (1ULL << (digit - 1));
    sp.reset_dynamic_graph(nn);
    sp.edge_count = 0;

    auto get_or_create_group = [&](const int* cells, int count) -> int {
        if (count <= 0) return -1;
        if (count == 1) return sp.dyn_cell_to_node[cells[0]];

        for (int node = sp.dyn_digit_cell_count; node < sp.dyn_node_count; ++node) {
            const int begin = sp.adj_offsets[node];
            const int end = sp.adj_offsets[node + 1];
            if (end - begin != count) continue;
            bool same = true;
            for (int k = 0; k < count; ++k) {
                if (sp.adj_flat[begin + k] != cells[k]) {
                    same = false;
                    break;
                }
            }
            if (same) return node;
        }

        if (sp.dyn_node_count >= ExactPatternScratchpad::MAX_NN) return -1;
        if (sp.edge_count + count > ExactPatternScratchpad::MAX_ADJ) return -1;

        const int node = sp.dyn_node_count++;
        sp.dyn_node_to_cell[node] = cells[0];
        sp.adj_offsets[node] = sp.edge_count;
        for (int k = 0; k < count; ++k) {
            sp.adj_flat[sp.edge_count++] = cells[k];
        }
        sp.adj_offsets[node + 1] = sp.edge_count;
        return node;
    };

    // Singleton nodes dla wszystkich kandydatów danej cyfry.
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if ((st.cands[idx] & bit) == 0ULL) continue;
        if (sp.dyn_node_count >= ExactPatternScratchpad::MAX_NN) break;
        if (sp.edge_count + 1 > ExactPatternScratchpad::MAX_ADJ) break;

        const int node = sp.dyn_node_count++;
        sp.dyn_cell_to_node[idx] = node;
        sp.dyn_node_to_cell[node] = idx;
        sp.dyn_digit_cells[sp.dyn_digit_cell_count++] = idx;
        sp.adj_offsets[node] = sp.edge_count;
        sp.adj_flat[sp.edge_count++] = idx;
        sp.adj_offsets[node + 1] = sp.edge_count;
    }

    if (sp.dyn_digit_cell_count < 2) return false;

    // ------------------------------------------------------------------------
    // Krok 1: standardowe strong-linki w domkach z dokładnie dwoma kandydatami.
    // ------------------------------------------------------------------------
    for (int r = 0; r < n; ++r) {
        int cells[2];
        int cnt = 0;
        for (int c = 0; c < n && cnt <= 2; ++c) {
            const int idx = r * n + c;
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                if (cnt < 2) cells[cnt] = idx;
                ++cnt;
            }
        }
        if (cnt == 2) {
            dyn_add_strong_edge(sp, sp.dyn_cell_to_node[cells[0]], sp.dyn_cell_to_node[cells[1]]);
        }
    }

    for (int c = 0; c < n; ++c) {
        int cells[2];
        int cnt = 0;
        for (int r = 0; r < n && cnt <= 2; ++r) {
            const int idx = r * n + c;
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                if (cnt < 2) cells[cnt] = idx;
                ++cnt;
            }
        }
        if (cnt == 2) {
            dyn_add_strong_edge(sp, sp.dyn_cell_to_node[cells[0]], sp.dyn_cell_to_node[cells[1]]);
        }
    }

    for (int b = 0; b < n; ++b) {
        const int br0 = (b / st.topo->box_cols_count) * st.topo->box_rows;
        const int bc0 = (b % st.topo->box_cols_count) * st.topo->box_cols;
        int cells[2];
        int cnt = 0;
        for (int dr = 0; dr < st.topo->box_rows && cnt <= 2; ++dr) {
            for (int dc = 0; dc < st.topo->box_cols && cnt <= 2; ++dc) {
                const int idx = (br0 + dr) * n + (bc0 + dc);
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    if (cnt < 2) cells[cnt] = idx;
                    ++cnt;
                }
            }
        }
        if (cnt == 2) {
            dyn_add_strong_edge(sp, sp.dyn_cell_to_node[cells[0]], sp.dyn_cell_to_node[cells[1]]);
        }
    }

    // ------------------------------------------------------------------------
    // Krok 2: grouped strong-linki dla przecięć domków.
    // Działa poprawnie także dla box_rows != box_cols.
    // ------------------------------------------------------------------------

    // Rząd podzielony przez bloki kolumnowe (row-box).
    for (int r = 0; r < n; ++r) {
        int regions[64];
        int region_cnt = 0;
        int total_cnt = 0;
        for (int bc = 0; bc < st.topo->box_cols_count; ++bc) {
            int cells[64];
            int cnt = 0;
            const int c0 = bc * st.topo->box_cols;
            const int c1 = c0 + st.topo->box_cols;
            for (int c = c0; c < c1; ++c) {
                const int idx = r * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            total_cnt += cnt;
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) regions[region_cnt++] = node;
            }
        }
        if (region_cnt == 2 && total_cnt > 2) {
            dyn_add_strong_edge(sp, regions[0], regions[1]);
        }
    }

    // Kolumna podzielona przez bloki wierszowe (col-box).
    for (int c = 0; c < n; ++c) {
        int regions[64];
        int region_cnt = 0;
        int total_cnt = 0;
        for (int br = 0; br < st.topo->box_rows_count; ++br) {
            int cells[64];
            int cnt = 0;
            const int r0 = br * st.topo->box_rows;
            const int r1 = r0 + st.topo->box_rows;
            for (int r = r0; r < r1; ++r) {
                const int idx = r * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            total_cnt += cnt;
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) regions[region_cnt++] = node;
            }
        }
        if (region_cnt == 2 && total_cnt > 2) {
            dyn_add_strong_edge(sp, regions[0], regions[1]);
        }
    }

    // Blok podzielony przez wiersze (box-row) i kolumny (box-col).
    for (int b = 0; b < n; ++b) {
        const int br = b / st.topo->box_cols_count;
        const int bc = b % st.topo->box_cols_count;
        const int r0 = br * st.topo->box_rows;
        const int c0 = bc * st.topo->box_cols;

        int row_regions[64];
        int row_region_cnt = 0;
        int row_total_cnt = 0;
        for (int dr = 0; dr < st.topo->box_rows; ++dr) {
            int cells[64];
            int cnt = 0;
            const int r = r0 + dr;
            for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                const int idx = r * n + (c0 + dc);
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            row_total_cnt += cnt;
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) row_regions[row_region_cnt++] = node;
            }
        }
        if (row_region_cnt == 2 && row_total_cnt > 2) {
            dyn_add_strong_edge(sp, row_regions[0], row_regions[1]);
        }

        int col_regions[64];
        int col_region_cnt = 0;
        int col_total_cnt = 0;
        for (int dc = 0; dc < st.topo->box_cols; ++dc) {
            int cells[64];
            int cnt = 0;
            const int c = c0 + dc;
            for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                const int idx = (r0 + dr) * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            col_total_cnt += cnt;
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) col_regions[col_region_cnt++] = node;
            }
        }
        if (col_region_cnt == 2 && col_total_cnt > 2) {
            dyn_add_strong_edge(sp, col_regions[0], col_regions[1]);
        }
    }

    // ------------------------------------------------------------------------
    // Krok 3: house membership każdego węzła.
    // Węzeł grupowy może należeć do 1 albo 2 domków (np. wspólny rząd i box).
    // ------------------------------------------------------------------------
    struct NodeHouses {
        int h[3];
        int count;
    };
    NodeHouses nh[ExactPatternScratchpad::MAX_NN]{};

    for (int node = 0; node < sp.dyn_node_count; ++node) {
        const int lead = sp.adj_flat[sp.adj_offsets[node]];
        const int candidate_houses[3] = {
            st.topo->cell_row[lead],
            n + st.topo->cell_col[lead],
            (n << 1) + st.topo->cell_box[lead]
        };
        int hc = 0;
        for (int k = 0; k < 3; ++k) {
            const int house_id = candidate_houses[k];
            if (node_is_subset_of_house(st, sp, node, house_id, n)) {
                nh[node].h[hc++] = house_id;
            }
        }
        nh[node].count = hc;
    }

    // ------------------------------------------------------------------------
    // Krok 4: weak-linki pomiędzy rozłącznymi węzłami w tym samym domku.
    // ------------------------------------------------------------------------
    for (int u = 0; u < sp.dyn_node_count; ++u) {
        for (int v = u + 1; v < sp.dyn_node_count; ++v) {
            if (nodes_intersect(sp, u, v)) continue;

            bool shared_house = false;
            for (int i = 0; i < nh[u].count && !shared_house; ++i) {
                for (int j = 0; j < nh[v].count; ++j) {
                    if (nh[u].h[i] == nh[v].h[j]) {
                        shared_house = true;
                        break;
                    }
                }
            }
            if (!shared_house) continue;

            dyn_add_weak_edge(sp, u, v);
        }
    }

    if (sp.dyn_strong_edge_count == 0 && sp.dyn_weak_edge_count == 0) {
        return false;
    }

    // ------------------------------------------------------------------------
    // Krok 5: transformacja do CSR dla BFS / chain walkers.
    // Strong adjacency zawiera tylko strong links.
    // Weak adjacency zawiera zarówno strong, jak i weak, bo strong implikuje weak.
    // ------------------------------------------------------------------------
    sp.reset_dynamic_adjacency(sp.dyn_node_count);

    for (int e = 0; e < sp.dyn_strong_edge_count; ++e) {
        const int u = sp.dyn_strong_edge_u[e];
        const int v = sp.dyn_strong_edge_v[e];
        ++sp.dyn_strong_degree[u];
        ++sp.dyn_strong_degree[v];
        ++sp.dyn_weak_degree[u];
        ++sp.dyn_weak_degree[v];
    }
    for (int e = 0; e < sp.dyn_weak_edge_count; ++e) {
        const int u = sp.dyn_weak_edge_u[e];
        const int v = sp.dyn_weak_edge_v[e];
        ++sp.dyn_weak_degree[u];
        ++sp.dyn_weak_degree[v];
    }

    int strong_total = 0;
    int weak_total = 0;
    for (int i = 0; i < sp.dyn_node_count; ++i) {
        sp.dyn_strong_offsets[i] = strong_total;
        sp.dyn_weak_offsets[i] = weak_total;
        strong_total += sp.dyn_strong_degree[i];
        weak_total += sp.dyn_weak_degree[i];
    }
    sp.dyn_strong_offsets[sp.dyn_node_count] = strong_total;
    sp.dyn_weak_offsets[sp.dyn_node_count] = weak_total;

    if (strong_total > ExactPatternScratchpad::MAX_ADJ ||
        weak_total > ExactPatternScratchpad::MAX_ADJ) {
        return false;
    }

    for (int i = 0; i < sp.dyn_node_count; ++i) {
        sp.dyn_strong_cursor[i] = sp.dyn_strong_offsets[i];
        sp.dyn_weak_cursor[i] = sp.dyn_weak_offsets[i];
    }

    for (int e = 0; e < sp.dyn_strong_edge_count; ++e) {
        const int u = sp.dyn_strong_edge_u[e];
        const int v = sp.dyn_strong_edge_v[e];
        sp.dyn_strong_adj[sp.dyn_strong_cursor[u]++] = v;
        sp.dyn_strong_adj[sp.dyn_strong_cursor[v]++] = u;
        sp.dyn_weak_adj[sp.dyn_weak_cursor[u]++] = v;
        sp.dyn_weak_adj[sp.dyn_weak_cursor[v]++] = u;
    }
    for (int e = 0; e < sp.dyn_weak_edge_count; ++e) {
        const int u = sp.dyn_weak_edge_u[e];
        const int v = sp.dyn_weak_edge_v[e];
        sp.dyn_weak_adj[sp.dyn_weak_cursor[u]++] = v;
        sp.dyn_weak_adj[sp.dyn_weak_cursor[v]++] = u;
    }

    return true;
}

} // namespace sudoku_hpc::logic::shared
