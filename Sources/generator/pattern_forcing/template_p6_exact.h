// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: template_p6_exact.h
// Opis: Biblioteka precyzyjnych szablonów geometrycznych dla strategii z 
//       poziomu 6, 7 i 8 (Nightmare / Theoretical). Wstrzykuje rygorystyczne
//       maski (Anchors) oraz powiązania silne (Skeletons), wymuszając na 
//       solverze DLX zbudowanie planszy zawierającej docelowy wzorzec.
//       Gwarancja Zero-Allocation. Ochrona asymetrii NxN oraz Pigeonhole.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <cstdint>
#include <random>
#include <algorithm>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateP6Exact {
private:
    static bool is_peer(const GenericTopology& topo, int a, int b) {
        return topo.cell_row[a] == topo.cell_row[b] ||
               topo.cell_col[a] == topo.cell_col[b] ||
               topo.cell_box[a] == topo.cell_box[b];
    }

    // Pomocnicze funkcje zapewniające bezkolizyjne, losowe indeksowanie
    static int random_distinct_index(int n, int avoid, std::mt19937_64& rng) {
        int out = avoid;
        for (int g = 0; g < 64 && out == avoid; ++g) {
            out = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return out;
    }

    static int random_third_index(int n, int a, int b, std::mt19937_64& rng) {
        int out = a;
        for (int g = 0; g < 96 && (out == a || out == b); ++g) {
            out = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        return out;
    }

    static int random_digit_distinct(int n, uint64_t avoid_mask, std::mt19937_64& rng) {
        int d = 1;
        for (int g = 0; g < 128; ++g) {
            d = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
            if ((avoid_mask & (1ULL << (d - 1))) == 0) return d;
        }
        for (int i = 1; i <= n; ++i) {
            if ((avoid_mask & (1ULL << (i - 1))) == 0) return i;
        }
        return d;
    }

    // Zabezpiecza Silne Powiązanie (Strong Link) dla danej cyfry w rzędzie
    static void force_strong_link_row(const GenericTopology& topo, ExactPatternTemplatePlan& plan, int row, int c1, int c2, int digit) {
        const uint64_t mask = ~(1ULL << (digit - 1)) & pf_full_mask_for_n(topo.n);
        for (int cc = 0; cc < topo.n; ++cc) {
            if (cc == c1 || cc == c2) continue;
            plan.add_skeleton(row * topo.n + cc, mask);
        }
    }

    // Zabezpiecza Silne Powiązanie (Strong Link) dla danej cyfry w kolumnie
    static void force_strong_link_col(const GenericTopology& topo, ExactPatternTemplatePlan& plan, int col, int r1, int r2, int digit) {
        const uint64_t mask = ~(1ULL << (digit - 1)) & pf_full_mask_for_n(topo.n);
        for (int rr = 0; rr < topo.n; ++rr) {
            if (rr == r1 || rr == r2) continue;
            plan.add_skeleton(rr * topo.n + col, mask);
        }
    }

public:
    static bool build_medusa(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;

        // Medusa 3D to połączony kolorowanką graf. Wymuszamy łańcuch bivalue.
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        
        const uint64_t ab = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));

        // Łańcuch: A(r1,c1) = B(r1,c2) = C(r2,c2) = D(r2,c3)
        plan.add_anchor(r1 * n + c1, ab);
        plan.add_anchor(r1 * n + c2, ab);
        plan.add_anchor(r2 * n + c2, ab);
        plan.add_anchor(r2 * n + c3, ab);
        
        // Zabezpieczenie silnych powiązań przed szumem z DLX
        force_strong_link_row(topo, plan, r1, c1, c2, d1);
        force_strong_link_col(topo, plan, c2, r1, r2, d1);
        force_strong_link_row(topo, plan, r2, c2, c3, d1);
        
        // TARGET INJECTION: Cel musi widzieć komórki w przeciwnych kolorach
        int tr = r2;
        int tc = c1;
        int dt = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        plan.add_anchor(tr * n + tc, (1ULL << (d1 - 1)) | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_death_blossom(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int p0 = topo.house_offsets[2 * n + box];
        const int p1 = topo.house_offsets[2 * n + box + 1];
        
        if (p1 - p0 < 5) return false;

        int cells[64];
        int count = p1 - p0;
        for (int i = 0; i < count; ++i) {
            cells[i] = topo.houses_flat[p0 + i];
        }
        
        for (int i = count - 1; i > 0; --i) {
            int j = static_cast<int>(rng() % static_cast<uint64_t>(i + 1));
            std::swap(cells[i], cells[j]);
        }

        int pivot = cells[0];
        int petal_cells[3] = {cells[1], cells[2], cells[3]};
        int target = cells[4];

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int dz = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);
        const int dt = random_digit_distinct(n, (1ULL << (dz - 1)), rng);

        const uint64_t pivot_mask = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        plan.add_anchor(pivot, pivot_mask);

        // Płatki współdzielą Z
        plan.add_anchor(petal_cells[0], (1ULL << (d1 - 1)) | (1ULL << (dz - 1)));
        plan.add_anchor(petal_cells[1], (1ULL << (d2 - 1)) | (1ULL << (dz - 1)));
        plan.add_anchor(petal_cells[2], (1ULL << (d3 - 1)) | (1ULL << (dz - 1)));
        
        // Zabezpieczenie płatków
        force_strong_link_row(topo, plan, topo.cell_row[pivot], topo.cell_col[pivot], topo.cell_col[petal_cells[0]], d1);
        
        // TARGET INJECTION
        plan.add_anchor(target, (1ULL << (dz - 1)) | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_sue_de_coq(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;
        const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));

        if (topo.box_cols < 2) return false;
        int i1 = row * n + c0;
        int i2 = row * n + c0 + 1; 

        // Bezpieczne znalezienie komórki poza blokiem
        int row_only = -1;
        for (int c = 0; c < n; ++c) {
            if (c < c0 || c >= c0 + topo.box_cols) {
                row_only = row * n + c;
                break;
            }
        }
        if (row_only == -1) return false;

        int box_only = -1;
        for (int dr = 0; dr < topo.box_rows; ++dr) {
            if (r0 + dr == row) continue;
            box_only = (r0 + dr) * n + c0;
            break;
        }
        if (box_only == -1) return false;

        // Maski poprawione tak, aby generowały rzetelny układ ALS.
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);

        const uint64_t m_inter = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        const uint64_t m_row   = (1ULL << (d1 - 1)) | (1ULL << (d4 - 1));
        const uint64_t m_box   = (1ULL << (d2 - 1)) | (1ULL << (d4 - 1)); // Zmieniono na spójną pulę (d2|d4)

        plan.add_anchor(i1, m_inter);
        plan.add_anchor(i2, m_inter);
        plan.add_anchor(row_only, m_row);
        plan.add_anchor(box_only, m_box);

        // TARGET INJECTION: Cel w rzędzie zdejmujący D1
        int target_row = -1;
        for (int cc = 0; cc < n; ++cc) {
            if (cc >= c0 && cc < c0 + topo.box_cols) continue;
            int cand = row * n + cc;
            if (cand != row_only) { target_row = cand; break; }
        }
        if (target_row != -1) {
            int dx = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
            plan.add_anchor(target_row, (1ULL << (d1 - 1)) | (1ULL << (dx - 1)));
        }

        // TARGET INJECTION: Cel w bloku zdejmujący D2
        int target_box = -1;
        for (int dr = 0; dr < topo.box_rows; ++dr) {
            if (r0 + dr == row) continue;
            for (int dc = 0; dc < topo.box_cols; ++dc) {
                int cand = (r0 + dr) * n + (c0 + dc);
                if (cand != box_only) { target_box = cand; break; }
            }
            if (target_box != -1) break;
        }
        if (target_box != -1) {
            int dy = random_digit_distinct(n, (1ULL << (d2 - 1)), rng);
            plan.add_anchor(target_box, (1ULL << (d2 - 1)) | (1ULL << (dy - 1)));
        }

        for (int dc = 0; dc < topo.box_cols; ++dc) {
            const int cell = row * n + c0 + dc;
            if (cell != i1 && cell != i2) {
                plan.add_skeleton(cell, ~(m_inter) & pf_full_mask_for_n(n));
            }
        }

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_kraken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 3, true); 
    }

    static bool build_franken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 3, false); 
    }

    static bool build_mutant_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 4, false); 
    }

    static bool build_squirmbag(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        return build_large_fish(topo, rng, plan, 5, false);
    }

    static bool build_large_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, int line_count, bool finned_mode) {
        plan = {};
        const int n = topo.n;
        if (n < std::max(6, line_count + 2)) return false;

        int rows[6] = { -1, -1, -1, -1, -1, -1 };
        int cols[6] = { -1, -1, -1, -1, -1, -1 };
        
        rows[0] = static_cast<int>(rng() % static_cast<uint64_t>(n));
        cols[0] = static_cast<int>(rng() % static_cast<uint64_t>(n));
        for (int i = 1; i < line_count; ++i) {
            rows[i] = rows[0];
            cols[i] = cols[0];
            for (int g = 0; g < 96 && std::find(rows, rows + i, rows[i]) != rows + i; ++g) {
                rows[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            for (int g = 0; g < 96 && std::find(cols, cols + i, cols[i]) != cols + i; ++g) {
                cols[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
        }

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t bit = 1ULL << (d1 - 1);

        for (int ri = 0; ri < line_count; ++ri) {
            for (int ci = 0; ci < line_count; ++ci) {
                int d_other = random_digit_distinct(n, bit, rng);
                plan.add_anchor(rows[ri] * n + cols[ci], bit | (1ULL << (d_other - 1)));
            }
        }
        
        int fin_cell = -1;
        if (finned_mode) {
            int extra_col = cols[0];
            for (int g = 0; g < 96; ++g) {
                extra_col = static_cast<int>(rng() % static_cast<uint64_t>(n));
                if (std::find(cols, cols + line_count, extra_col) == cols + line_count &&
                    topo.cell_box[rows[0] * n + extra_col] == topo.cell_box[rows[0] * n + cols[0]]) {
                    break;
                }
            }
            if (std::find(cols, cols + line_count, extra_col) == cols + line_count) {
                int d_other1 = random_digit_distinct(n, bit, rng);
                fin_cell = rows[0] * n + extra_col;
                plan.add_anchor(fin_cell, bit | (1ULL << (d_other1 - 1)));
            } else {
                return false; 
            }
        }
        
        // TARGET INJECTION: Cel na linii 'covers' ale nie na 'base'
        int target_idx = -1;
        for (int r = 0; r < n && target_idx < 0; ++r) {
            if (std::find(rows, rows + line_count, r) != rows + line_count) {
                for (int c = 0; c < line_count; ++c) {
                    int cand_idx = r * n + cols[c];
                    if (finned_mode && !is_peer(topo, cand_idx, fin_cell)) continue;
                    target_idx = cand_idx;
                    break;
                }
            }
        }
        
        if (target_idx >= 0) {
            int d_t = random_digit_distinct(n, bit, rng);
            plan.add_anchor(target_idx, bit | (1ULL << (d_t - 1)));
        } else {
            return false;
        }

        // Zablokuj komórki poza rybą w rzędach bazowych - ryba nie może mieć szumu, 
        // ale ignorujemy Target, żeby dało się go usunąć
        const uint64_t mask = ~bit & pf_full_mask_for_n(n);
        for (int ri = 0; ri < line_count; ++ri) {
            for (int cc = 0; cc < n; ++cc) {
                if (std::find(cols, cols + line_count, cc) == cols + line_count) {
                    int cell = rows[ri] * n + cc;
                    if (cell != fin_cell && cell != target_idx) {
                        plan.add_skeleton(cell, mask);
                    }
                }
            }
        }

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_als_xy_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;
        
        const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[pivot];
        const int col = topo.cell_col[pivot];
        
        int col2 = (col + 1) % n;
        int p_cell1 = pivot;
        int p_cell2 = row * n + col2;
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        
        const uint64_t xy = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t xz = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1));
        const uint64_t yz = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        
        const uint64_t pivot_mask = xy | (1ULL << (d3 - 1));
        plan.add_anchor(p_cell1, pivot_mask);
        plan.add_anchor(p_cell2, pivot_mask);
        
        int w1 = -1, w2 = -1;
        for (int cc = 0; cc < n && w1 < 0; ++cc) {
            if (cc != col && cc != col2) w1 = row * n + cc;
        }
        for (int rr = 0; rr < n && w2 < 0; ++rr) {
            if (rr != row) w2 = rr * n + col;
        }
        if (w1 < 0 || w2 < 0) return false;
        
        plan.add_anchor(w1, xz);
        plan.add_anchor(w2, yz);
        
        // TARGET INJECTION
        int target = -1;
        for(int rr = 0; rr < n && target < 0; ++rr) {
            if (rr != row && rr != topo.cell_row[w2]) {
                target = rr * n + topo.cell_col[w1];
            }
        }
        if (target >= 0) {
            int dt = random_digit_distinct(n, (1ULL << (d3 - 1)), rng);
            plan.add_anchor(target, (1ULL << (d3 - 1)) | (1ULL << (dt - 1)));
        }
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_als_xz(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        if (r1 == r2 || c1 == c2) return false;

        const int dx = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int dz = random_digit_distinct(n, (1ULL << (dx - 1)), rng);
        const int da = random_digit_distinct(n, (1ULL << (dx - 1)) | (1ULL << (dz - 1)), rng);
        const int db = random_digit_distinct(n, (1ULL << (dx - 1)) | (1ULL << (dz - 1)) | (1ULL << (da - 1)), rng);

        const uint64_t x = (1ULL << (dx - 1));
        const uint64_t z = (1ULL << (dz - 1));
        const uint64_t a = (1ULL << (da - 1));
        const uint64_t b = (1ULL << (db - 1));

        plan.add_anchor(r1 * n + c1, x | z | a); 
        plan.add_anchor(r1 * n + c2, a | z);
        
        plan.add_anchor(r2 * n + c1, x | z | b); 
        plan.add_anchor(r2 * n + c2, b | z);

        // TARGET INJECTION
        int c3 = random_third_index(n, c1, c2, rng);
        int dt = random_digit_distinct(n, z, rng);
        plan.add_anchor(r1 * n + c3, z | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_wxyz_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 5 || topo.box_cols < 3) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;
        
        const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));

        const int p1 = row * n + c0;
        const int p2 = row * n + c0 + 1;
        const int p3 = row * n + c0 + 2;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);

        const uint64_t w = (1ULL << (d1 - 1));
        const uint64_t x = (1ULL << (d2 - 1));
        const uint64_t y = (1ULL << (d3 - 1));
        const uint64_t z = (1ULL << (d4 - 1));
        
        const uint64_t pivot_mask = w | x | y | z;
        plan.add_anchor(p1, pivot_mask);
        plan.add_anchor(p2, pivot_mask);
        plan.add_anchor(p3, pivot_mask);
        
        int w1_cell = -1, w2_cell = -1, w3_cell = -1;
        for (int cc = 0; cc < n && w1_cell < 0; ++cc) {
            if (cc < c0 || cc >= c0 + topo.box_cols) w1_cell = row * n + cc;
        }
        for (int rr = 0; rr < n && w2_cell < 0; ++rr) {
            if (rr < r0 || rr >= r0 + topo.box_rows) w2_cell = rr * n + c0;
        }
        for (int rr = 0; rr < n && w3_cell < 0; ++rr) {
            if (rr < r0 || rr >= r0 + topo.box_rows) {
                if (rr * n + c0 + 1 != w2_cell) w3_cell = rr * n + c0 + 1;
            }
        }

        if (w1_cell < 0 || w2_cell < 0 || w3_cell < 0) return false;

        plan.add_anchor(w1_cell, w | z);
        plan.add_anchor(w2_cell, x | z);
        plan.add_anchor(w3_cell, y | z);
        
        // TARGET INJECTION
        int target = -1;
        for(int rr = 0; rr < n && target < 0; ++rr) {
            if (rr != topo.cell_row[w2_cell] && rr != topo.cell_row[w3_cell]) {
                target = rr * n + topo.cell_col[w1_cell];
            }
        }
        if (target >= 0) {
            int dt = random_digit_distinct(n, z, rng);
            plan.add_anchor(target, z | (1ULL << (dt - 1)));
        }

        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_als_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool aic_mode) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        int c4 = c3;
        for(int g=0; g<64 && (c4==c1 || c4==c2 || c4==c3); ++g) c4 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        if (c1 == c2 || c1 == c3 || c1 == c4 || c2 == c3 || c2 == c4 || c3 == c4) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);
        
        const uint64_t a = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        const uint64_t b = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)) | (1ULL << (d4 - 1));
        const uint64_t c = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1)) | (1ULL << (d4 - 1));
        
        plan.add_anchor(r1 * n + c1, a);
        plan.add_anchor(r1 * n + c2, a); // ALS A
        
        plan.add_anchor(r2 * n + c2, b);
        plan.add_anchor(r2 * n + c3, b); // ALS B
        
        plan.add_anchor(r1 * n + c3, c);
        plan.add_anchor(r1 * n + c4, c); // ALS C
        
        if (aic_mode) {
            plan.add_anchor(r2 * n + c1, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)));
        }

        // TARGET INJECTION
        int target = r2 * n + c4;
        int dt = random_digit_distinct(n, (1ULL << (d4 - 1)), rng);
        plan.add_anchor(target, (1ULL << (d4 - 1)) | (1ULL << (dt - 1)));
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_aligned_exclusion(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool triple_mode) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        const int a = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        int b = a;
        for (int p = topo.peer_offsets[a]; p < topo.peer_offsets[a + 1] && b == a; ++p) {
            b = topo.peers_flat[p];
        }
        if (b == a) return false;
        
        int c = -1;
        if (triple_mode) {
            for (int p = topo.peer_offsets[b]; p < topo.peer_offsets[b + 1] && c < 0; ++p) {
                const int idx = topo.peers_flat[p];
                if (idx == a) continue;
                if (topo.cell_row[idx] == topo.cell_row[a] ||
                    topo.cell_col[idx] == topo.cell_col[a] ||
                    topo.cell_box[idx] == topo.cell_box[a]) {
                    c = idx;
                }
            }
            if (c < 0) return false;
        }

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);
        
        plan.add_anchor(a, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)));
        plan.add_anchor(b, (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)));
        if (triple_mode) plan.add_anchor(c, (1ULL << (d3 - 1)) | (1ULL << (d4 - 1)));

        // TARGET INJECTION
        int target = -1;
        for (int p = topo.peer_offsets[a]; p < topo.peer_offsets[a + 1]; ++p) {
            const int peer = topo.peers_flat[p];
            if (peer != b && peer != c && is_peer(topo, peer, b)) {
                target = peer; break;
            }
        }
        if (target >= 0) {
            int dt = random_digit_distinct(n, (1ULL << (d2 - 1)), rng);
            plan.add_anchor(target, (1ULL << (d2 - 1)) | (1ULL << (dt - 1)));
        }
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);

        // AIC: Przestawiono na poprawne Single-Digit Chains z wsparciem Certyfikatora
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t bit = 1ULL << (d1 - 1);
        
        int dA = random_digit_distinct(n, bit, rng);
        int dB = random_digit_distinct(n, bit, rng);
        int dC = random_digit_distinct(n, bit, rng);
        int dD = random_digit_distinct(n, bit, rng);
        
        int a = r1 * n + c1;
        int b = r1 * n + c2;
        int c = r2 * n + c2;
        int d = r2 * n + c3;
        
        plan.add_anchor(a, bit | (1ULL << (dA - 1))); 
        plan.add_anchor(b, bit | (1ULL << (dB - 1))); 
        plan.add_anchor(c, bit | (1ULL << (dC - 1))); 
        plan.add_anchor(d, bit | (1ULL << (dD - 1))); 
        
        force_strong_link_row(topo, plan, r1, c1, c2, d1); // A =strong= B
        force_strong_link_row(topo, plan, r2, c2, c3, d1); // C =strong= D

        // TARGET INJECTION: Komórka widząca A(r1,c1) i D(r2,c3). 
        int tr = r2;
        int tc = c1;
        int dt = random_digit_distinct(n, bit, rng);
        plan.add_anchor(tr * n + tc, bit | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_grouped_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        
        // C2 w tym samym bloku co C1
        for (int g = 0; g < 64 && topo.cell_box[r1 * n + c1] != topo.cell_box[r1 * n + c2]; ++g) {
            c2 = random_distinct_index(n, c1, rng);
        }
        if (topo.cell_box[r1 * n + c1] != topo.cell_box[r1 * n + c2]) return false;
        
        int r2 = random_distinct_index(n, r1, rng);
        for (int g = 0; g < 64 && topo.cell_box[r2 * n + c1] == topo.cell_box[r1 * n + c1]; ++g) {
            r2 = random_distinct_index(n, r1, rng);
        }
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t bit = 1ULL << (d1 - 1);
        
        int a1 = r1 * n + c1;
        int a2 = r1 * n + c2; // Grupa w bloku
        int b = r2 * n + c1; 
        int c = r2 * n + c2; 
        
        int dX = random_digit_distinct(n, bit, rng);
        int dY = random_digit_distinct(n, bit, rng);
        
        plan.add_anchor(a1, bit | (1ULL << (dX - 1)));
        plan.add_anchor(a2, bit | (1ULL << (dX - 1)));
        plan.add_anchor(b, bit | (1ULL << (dY - 1)));
        plan.add_anchor(c, bit | (1ULL << (dY - 1)));
        
        force_strong_link_col(topo, plan, c1, r1, r2, d1);
        
        // TARGET INJECTION
        int r3 = -1;
        for (int rr = 0; rr < n; ++rr) {
            if (rr != r1 && topo.cell_box[rr * n + c1] == topo.cell_box[r1 * n + c1]) {
                r3 = rr; break;
            }
        }
        if (r3 >= 0) {
            int t = r3 * n + c2;
            int dT = random_digit_distinct(n, bit, rng);
            plan.add_anchor(t, bit | (1ULL << (dT - 1)));
        }

        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_grouped_x_cycle(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t bit = 1ULL << (d1 - 1);
        
        int dO1 = random_digit_distinct(n, bit, rng);
        int dO2 = random_digit_distinct(n, bit, rng);
        
        // A - B - C - D
        plan.add_anchor(r1 * n + c1, bit | (1ULL << (dO1 - 1)));
        plan.add_anchor(r1 * n + c2, bit | (1ULL << (dO2 - 1)));
        plan.add_anchor(r2 * n + c1, bit | (1ULL << (dO1 - 1)));
        plan.add_anchor(r2 * n + c2, bit | (1ULL << (dO2 - 1)));
        
        force_strong_link_row(topo, plan, r1, c1, c2, d1);
        force_strong_link_row(topo, plan, r2, c1, c2, d1);
        
        // Dodaj grupę
        int target = -1;
        for (int rr = 0; rr < n && plan.anchor_count < 6; ++rr) {
            if (rr == r1 || rr == r2) continue;
            if (topo.cell_box[rr * n + c1] == topo.cell_box[r1 * n + c1]) {
                plan.add_anchor(rr * n + c1, bit | (1ULL << (dO1 - 1)));
            } else if (target < 0) {
                target = rr * n + c1;
            }
        }

        // TARGET INJECTION
        if (target >= 0) {
            int dt = random_digit_distinct(n, bit, rng);
            plan.add_anchor(target, bit | (1ULL << (dt - 1)));
        }
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_continuous_nice_loop(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        
        int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t bit = 1ULL << (d1 - 1);
        
        int do1 = random_digit_distinct(n, bit, rng);
        int do2 = random_digit_distinct(n, bit, rng);
        
        plan.add_anchor(r1 * n + c1, bit | (1ULL << (do1 - 1)));
        plan.add_anchor(r1 * n + c2, bit | (1ULL << (do2 - 1)));
        plan.add_anchor(r2 * n + c2, bit | (1ULL << (do1 - 1)));
        plan.add_anchor(r2 * n + c1, bit | (1ULL << (do2 - 1)));
        
        force_strong_link_row(topo, plan, r1, c1, c2, d1);
        force_strong_link_row(topo, plan, r2, c1, c2, d1);

        // TARGET INJECTION: Komórka widząca wyjścia pętli
        int tr = random_third_index(n, r1, r2, rng);
        int dt = random_digit_distinct(n, bit, rng);
        plan.add_anchor(tr * n + c1, bit | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_x_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int r3 = random_third_index(n, r1, r2, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t bit = (1ULL << (d1 - 1));

        auto get_mask = [&]() {
            return bit | (1ULL << (random_digit_distinct(n, bit, rng) - 1));
        };

        plan.add_anchor(r1 * n + c1, get_mask());
        plan.add_anchor(r1 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c2, get_mask());
        plan.add_anchor(r2 * n + c3, get_mask());
        plan.add_anchor(r3 * n + c3, get_mask());
        plan.add_anchor(r3 * n + c1, get_mask());
        
        force_strong_link_row(topo, plan, r1, c1, c2, d1);
        force_strong_link_col(topo, plan, c2, r1, r2, d1);
        force_strong_link_row(topo, plan, r2, c2, c3, d1);
        force_strong_link_col(topo, plan, c3, r2, r3, d1);

        // TARGET INJECTION
        int target = r2 * n + c1;
        int dt = random_digit_distinct(n, bit, rng);
        plan.add_anchor(target, bit | (1ULL << (dt - 1)));
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_xy_chain(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);
        const int d5 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)) | (1ULL << (d4 - 1)), rng);
        
        plan.add_anchor(r1 * n + c1, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)));
        plan.add_anchor(r1 * n + c2, (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)));
        plan.add_anchor(r2 * n + c2, (1ULL << (d3 - 1)) | (1ULL << (d4 - 1)));
        plan.add_anchor(r2 * n + c3, (1ULL << (d4 - 1)) | (1ULL << (d5 - 1)));
        plan.add_anchor(r1 * n + c3, (1ULL << (d5 - 1)) | (1ULL << (d1 - 1)));

        // TARGET INJECTION
        int target = r2 * n + c1;
        int dt = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        plan.add_anchor(target, (1ULL << (d1 - 1)) | (1ULL << (dt - 1)));
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_empty_rectangle(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;
        const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));
        const int col = c0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_cols));
        const int pivot = row * n + col;

        int row_out = -1, col_out = -1, box_support = -1;
        for (int cc = 0; cc < n; ++cc) {
            if (cc >= c0 && cc < c0 + topo.box_cols) continue;
            if (row_out < 0) { row_out = row * n + cc; break; }
        }
        for (int rr = 0; rr < n; ++rr) {
            if (rr >= r0 && rr < r0 + topo.box_rows) continue;
            if (col_out < 0) { col_out = rr * n + col; break; }
        }
        for (int dr = 0; dr < topo.box_rows && box_support < 0; ++dr) {
            for (int dc = 0; dc < topo.box_cols; ++dc) {
                const int idx = (r0 + dr) * n + (c0 + dc);
                if (idx == pivot || r0 + dr == row || c0 + dc == col) continue;
                if (box_support < 0) box_support = idx;
            }
        }
        if (row_out < 0 || col_out < 0 || box_support < 0) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        
        const uint64_t core = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));

        plan.add_anchor(pivot, core);
        plan.add_anchor(row_out, core);
        plan.add_anchor(col_out, core);
        plan.add_anchor(box_support, core);

        // TARGET INJECTION
        int target = topo.cell_row[col_out] * n + topo.cell_col[row_out];
        int dt = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        plan.add_anchor(target, (1ULL << (d1 - 1)) | (1ULL << (dt - 1)));
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }

    static bool build_remote_pairs(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;
        
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        
        const uint64_t pair = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        
        plan.add_anchor(r1 * n + c1, pair);
        plan.add_anchor(r1 * n + c2, pair);
        plan.add_anchor(r2 * n + c2, pair);
        plan.add_anchor(r2 * n + c1, pair);

        // TARGET INJECTION
        int target = r1 * n + c3;
        int dt = random_digit_distinct(n, pair, rng);
        plan.add_anchor(target, pair | (1ULL << (dt - 1)));
        
        plan.explicit_skeleton = true;
        plan.valid = true;
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing