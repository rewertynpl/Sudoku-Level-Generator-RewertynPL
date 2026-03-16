// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: link_graph_builder.h
// Opis: System wyznaczania i utrzymywania sieci powiązań pomiędzy
//       komórkami w relacjach typu "Strong" oraz "Weak" per konkretna cyfra.
//       Kluczowe dla metod X-Cycles i Forcing Chains.
//       Zaktualizowano do pełnej obsługi węzłów grupowych (Grouped Nodes)
//       używających tablicy adj_flat ze Scratchpada (Zero-Allocation).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

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

inline void dyn_add_strong_edge(ExactPatternScratchpad& sp, int u, int v) {
    if (u == v) return;
    if (dyn_has_strong_edge(sp, u, v)) return;
    if (sp.dyn_strong_edge_count >= (ExactPatternScratchpad::MAX_HOUSES * 2)) return;
    
    sp.dyn_strong_edge_u[sp.dyn_strong_edge_count] = u;
    sp.dyn_strong_edge_v[sp.dyn_strong_edge_count] = v;
    ++sp.dyn_strong_edge_count;
}

inline void dyn_add_weak_edge(ExactPatternScratchpad& sp, int u, int v) {
    if (u == v) return;
    if (dyn_has_strong_edge(sp, u, v)) return;
    if (sp.dyn_weak_edge_count >= ExactPatternScratchpad::MAX_LINK_EDGES) return;
    
    sp.dyn_weak_edge_u[sp.dyn_weak_edge_count] = u;
    sp.dyn_weak_edge_v[sp.dyn_weak_edge_count] = v;
    ++sp.dyn_weak_edge_count;
}

// Funkcja pomocnicza: czy komórka widzi WSZYSTKIE komórki danego węzła (Group Node)?
inline bool node_sees_cell(const CandidateState& st, const ExactPatternScratchpad& sp, int node, int cell) {
    const int p0 = sp.adj_offsets[node];
    const int p1 = sp.adj_offsets[node + 1];
    for (int i = p0; i < p1; ++i) {
        if (!st.is_peer(cell, sp.adj_flat[i])) return false;
    }
    return true;
}

// Konstruuje cały graf wzajemnych oddziaływań dla konkretnej cyfry w jednym przebiegu.
inline bool build_grouped_link_graph_for_digit(const CandidateState& st, int digit, ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    const uint64_t bit = (1ULL << (digit - 1));
    
    sp.reset_dynamic_graph(nn);
    sp.edge_count = 0; // Wykorzystujemy edge_count z ExactPatternScratchpad jako kursor dla adj_flat

    // ========================================================================
    // Krok 1: Tworzenie Węzłów (Pojedynczych i Grupowych)
    // ========================================================================
    auto get_or_create_group = [&](const int* cells, int count) -> int {
        if (count == 0) return -1;
        if (count == 1) return sp.dyn_cell_to_node[cells[0]]; // Singleton to po prostu komórka
        
        // Szukamy, czy grupa o takich samych komórkach już istnieje
        // (Zaczynamy od końca pojedynczych komórek)
        for (int i = sp.dyn_digit_cell_count; i < sp.dyn_node_count; ++i) {
            const int c = sp.adj_offsets[i + 1] - sp.adj_offsets[i];
            if (c == count) {
                bool match = true;
                for (int k = 0; k < count; ++k) {
                    if (sp.adj_flat[sp.adj_offsets[i] + k] != cells[k]) {
                        match = false; 
                        break;
                    }
                }
                if (match) return i;
            }
        }
        
        // Tworzenie nowego węzła grupowego
        if (sp.dyn_node_count >= ExactPatternScratchpad::MAX_NN) return -1;
        if (sp.edge_count + count > ExactPatternScratchpad::MAX_ADJ) return -1;
        
        const int node = sp.dyn_node_count++;
        sp.dyn_node_to_cell[node] = cells[0]; // Reprezentant (Lead Cell) dla kompatybilności wstecznej
        
        sp.adj_offsets[node] = sp.edge_count;
        for (int k = 0; k < count; ++k) {
            sp.adj_flat[sp.edge_count++] = cells[k];
        }
        sp.adj_offsets[node + 1] = sp.edge_count;
        return node;
    };

    // Najpierw rejestrujemy wszystkie pojedyncze komórki jako podstawowe węzły
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
    
    if (sp.dyn_node_count < 2) return false;

    // ========================================================================
    // Krok 2: Poszukiwanie "Strong Links" z uwzględnieniem Węzłów Grupowych
    // ========================================================================
    
    // 2a. Silne powiązania w Rzędach (Przecięcia Rząd-Blok)
    for (int r = 0; r < n; ++r) {
        int regions[64];
        int region_cnt = 0;
        for (int bc = 0; bc < st.topo->box_cols_count; ++bc) {
            int cells[64];
            int cnt = 0;
            for (int c = bc * st.topo->box_cols; c < (bc + 1) * st.topo->box_cols; ++c) {
                const int idx = r * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) regions[region_cnt++] = node;
            }
        }
        // Jeśli w danym rzędzie kandydaci są zamknięci tylko w 2 regionach (blokach)
        if (region_cnt == 2) {
            dyn_add_strong_edge(sp, regions[0], regions[1]);
        }
    }

    // 2b. Silne powiązania w Kolumnach (Przecięcia Kolumna-Blok)
    for (int c = 0; c < n; ++c) {
        int regions[64];
        int region_cnt = 0;
        for (int br = 0; br < st.topo->box_rows_count; ++br) {
            int cells[64];
            int cnt = 0;
            for (int r = br * st.topo->box_rows; r < (br + 1) * st.topo->box_rows; ++r) {
                const int idx = r * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) regions[region_cnt++] = node;
            }
        }
        // Jeśli w danej kolumnie kandydaci są zamknięci tylko w 2 regionach (blokach)
        if (region_cnt == 2) {
            dyn_add_strong_edge(sp, regions[0], regions[1]);
        }
    }

    // 2c. Silne powiązania w Blokach (Przecięcia Blok-Rząd oraz Blok-Kolumna)
    for (int b = 0; b < n; ++b) {
        const int br = b / st.topo->box_cols_count;
        const int bc = b % st.topo->box_cols_count;
        
        // Podział bloku na rzędy
        int row_regions[64];
        int row_region_cnt = 0;
        for (int dr = 0; dr < st.topo->box_rows; ++dr) {
            int cells[64];
            int cnt = 0;
            const int r = br * st.topo->box_rows + dr;
            for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                const int c = bc * st.topo->box_cols + dc;
                const int idx = r * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) row_regions[row_region_cnt++] = node;
            }
        }
        if (row_region_cnt == 2) {
            dyn_add_strong_edge(sp, row_regions[0], row_regions[1]);
        }
        
        // Podział bloku na kolumny
        int col_regions[64];
        int col_region_cnt = 0;
        for (int dc = 0; dc < st.topo->box_cols; ++dc) {
            int cells[64];
            int cnt = 0;
            const int c = bc * st.topo->box_cols + dc;
            for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                const int r = br * st.topo->box_rows + dr;
                const int idx = r * n + c;
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    cells[cnt++] = idx;
                }
            }
            if (cnt > 0) {
                const int node = get_or_create_group(cells, cnt);
                if (node >= 0) col_regions[col_region_cnt++] = node;
            }
        }
        if (col_region_cnt == 2) {
            dyn_add_strong_edge(sp, col_regions[0], col_regions[1]);
        }
    }

    // ========================================================================
    // Krok 3: Dodanie "Weak Links" dla Węzłów będących w tym samym "domku"
    // ========================================================================
    struct NodeHouses {
        int h[3];
        int count;
    };
    NodeHouses nh[ExactPatternScratchpad::MAX_NN];
    
    for (int i = 0; i < sp.dyn_node_count; ++i) {
        const int c0 = sp.adj_flat[sp.adj_offsets[i]];
        const int c0_h[3] = {
            st.topo->cell_row[c0],
            n + st.topo->cell_col[c0],
            2 * n + st.topo->cell_box[c0]
        };
        int hc = 0;
        for (int k = 0; k < 3; ++k) {
            const int house = c0_h[k];
            bool all_in = true;
            for (int p = sp.adj_offsets[i] + 1; p < sp.adj_offsets[i+1]; ++p) {
                const int c = sp.adj_flat[p];
                const int ch[3] = {st.topo->cell_row[c], n + st.topo->cell_col[c], 2 * n + st.topo->cell_box[c]};
                if (house != ch[0] && house != ch[1] && house != ch[2]) {
                    all_in = false; 
                    break;
                }
            }
            if (all_in) {
                nh[i].h[hc++] = house;
            }
        }
        nh[i].count = hc;
    }

    for (int u = 0; u < sp.dyn_node_count; ++u) {
        for (int v = u + 1; v < sp.dyn_node_count; ++v) {
            bool shared_house = false;
            for (int i = 0; i < nh[u].count; ++i) {
                for (int j = 0; j < nh[v].count; ++j) {
                    if (nh[u].h[i] == nh[v].h[j]) {
                        shared_house = true; 
                        break;
                    }
                }
                if (shared_house) break;
            }
            
            if (shared_house) {
                // Muszą być rozłączne (Disjoint), by tworzyć Weak Link
                bool intersect = false;
                for (int i = sp.adj_offsets[u]; i < sp.adj_offsets[u+1]; ++i) {
                    const int cu = sp.adj_flat[i];
                    for (int j = sp.adj_offsets[v]; j < sp.adj_offsets[v+1]; ++j) {
                        if (cu == sp.adj_flat[j]) { 
                            intersect = true; 
                            break; 
                        }
                    }
                    if (intersect) break;
                }
                
                if (!intersect) {
                    dyn_add_weak_edge(sp, u, v);
                }
            }
        }
    }
    
    if (sp.dyn_strong_edge_count == 0 && sp.dyn_weak_edge_count == 0) return false;

    // ========================================================================
    // Krok 4: Transformacja do struktury CSR (Compressed Sparse Row) dla BFS
    // ========================================================================
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
    
    if (strong_total > ExactPatternScratchpad::MAX_ADJ || weak_total > ExactPatternScratchpad::MAX_ADJ) {
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