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
        // Fallback w razie zapętlenia
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

        // Medusa 3D polega na złożonym grafie komórek bivalue
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);

        const uint64_t ab = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t ac = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1));
        const uint64_t bc = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));

        plan.add_anchor(r1 * n + c1, ab);
        plan.add_anchor(r1 * n + c2, ac);
        plan.add_anchor(r2 * n + c2, bc);
        plan.add_anchor(r2 * n + c3, ab);
        plan.add_anchor(r1 * n + c3, bc);
        plan.add_anchor(r2 * n + c1, ac);
        
        // TARGET INJECTION: Komórka T widzi r1*c1 i r2*c1, które będą miały przeciwne kolory dla d1
        int tr = random_third_index(n, r1, r2, rng);
        int dt = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        plan.add_anchor(tr * n + c1, (1ULL << (d1 - 1)) | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_death_blossom(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;

        // Wrzucamy Pivot, Płatki i Target do TEGO SAMEGO BLOKU, by zagwarantować widoczność i uniknąć anty-clusteringu
        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int p0 = topo.house_offsets[2 * n + box];
        const int p1 = topo.house_offsets[2 * n + box + 1];
        
        if (p1 - p0 < 5) return false;

        int cells[64];
        int count = p1 - p0;
        for (int i = 0; i < count; ++i) {
            cells[i] = topo.houses_flat[p0 + i];
        }
        
        // Przetasujmy komórki w bloku, by nie były przewidywalne
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

        plan.add_anchor(petal_cells[0], (1ULL << (d1 - 1)) | (1ULL << (dz - 1)));
        plan.add_anchor(petal_cells[1], (1ULL << (d2 - 1)) | (1ULL << (dz - 1)));
        plan.add_anchor(petal_cells[2], (1ULL << (d3 - 1)) | (1ULL << (dz - 1)));
        
        // TARGET INJECTION: Komórka w tym samym bloku, w której wymuszamy eliminację Z
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

        int row_only = row * n + ((c0 + topo.box_cols) % n);
        int box_only = -1;
        for (int dr = 0; dr < topo.box_rows; ++dr) {
            if (r0 + dr == row) continue;
            box_only = (r0 + dr) * n + c0;
            break;
        }
        if (box_only == -1) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);

        const uint64_t m_inter = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        const uint64_t m_row   = (1ULL << (d1 - 1)) | (1ULL << (d4 - 1));
        const uint64_t m_box   = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)) | (1ULL << (d4 - 1));

        plan.add_anchor(i1, m_inter);
        plan.add_anchor(i2, m_inter);
        plan.add_anchor(row_only, m_row);
        plan.add_anchor(box_only, m_box);

        // TARGET INJECTION: Komórka w rzędzie, by zlikwidować nadwyżkę D1
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

        // NOWE: Blokowanie pozostałych komórek intersekcji
        for (int dc = 0; dc < topo.box_cols; ++dc) {
            const int cell = row * n + c0 + dc;
            if (cell != i1 && cell != i2) {
                // Blokujemy w niej kandydatów m_inter, aby komórka nie urosła do rozmiarów ALS SDC
                plan.add_skeleton(cell, ~(m_inter) & pf_full_mask_for_n(n));
            }
        }

        plan.explicit_skeleton = true;
        plan.valid = true;
        return true;
    }

    static bool build_kraken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;

        // Baza ryby 2x2 (X-Wing) + macka
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        int c3 = random_third_index(n, c1, c2, rng);
        if (r1 == r2 || c1 == c2 || c1 == c3 || c2 == c3) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t d1_bit = (1ULL << (d1 - 1));

        // PIGEONHOLE FIX: Każda kotwica musi dostać INNĄ cyfrę 'd_other'
        auto apply_fish_anchor = [&](int idx) {
            int d_other = random_digit_distinct(n, d1_bit, rng);
            plan.add_anchor(idx, d1_bit | (1ULL << (d_other - 1)));
        };

        apply_fish_anchor(r1 * n + c1);
        apply_fish_anchor(r1 * n + c2);
        apply_fish_anchor(r2 * n + c1);
        apply_fish_anchor(r2 * n + c2); // Fin
        apply_fish_anchor(r1 * n + c3); // Tentacle

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_franken_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        // Franken Fish: 2 Rzędy i 1 Box (Bazy) przecinające się z 3 Kolumnami (Covers)
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        if (r1 == r2 || c1 == c2) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t d1_bit = (1ULL << (d1 - 1));

        // PIGEONHOLE FIX
        auto apply_fish_anchor = [&](int idx) {
            int d_other = random_digit_distinct(n, d1_bit, rng);
            plan.add_anchor(idx, d1_bit | (1ULL << (d_other - 1)));
        };

        apply_fish_anchor(r1 * n + c1);
        apply_fish_anchor(r1 * n + c2);
        apply_fish_anchor(r2 * n + c1);
        apply_fish_anchor(r2 * n + c2);

        const int b1 = topo.cell_box[r1 * n + c1];
        const int house = 2 * n + b1;
        const int p0 = topo.house_offsets[house];
        const int p1 = topo.house_offsets[house + 1];
        for (int p = p0; p < p1 && plan.anchor_count < 6; ++p) {
            const int idx = topo.houses_flat[p];
            const int rr = topo.cell_row[idx];
            const int cc = topo.cell_col[idx];
            if (rr == r1 || rr == r2 || cc == c1 || cc == c2) continue;
            apply_fish_anchor(idx);
        }
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_mutant_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = random_distinct_index(n, r1, rng);
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = random_distinct_index(n, c1, rng);
        if (r1 == r2 || c1 == c2) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t d1_bit = (1ULL << (d1 - 1));

        // PIGEONHOLE FIX
        auto apply_fish_anchor = [&](int idx) -> bool {
            int d_other = random_digit_distinct(n, d1_bit, rng);
            plan.add_anchor(idx, d1_bit | (1ULL << (d_other - 1)));
            return true;
        };

        apply_fish_anchor(r1 * n + c1);
        apply_fish_anchor(r1 * n + c2);
        apply_fish_anchor(r2 * n + c1);
        apply_fish_anchor(r2 * n + c2);

        int b1 = topo.cell_box[r1 * n + c2];
        const int house = 2 * n + b1;
        const int p0 = topo.house_offsets[house];
        const int p1 = topo.house_offsets[house + 1];
        
        for (int p = p0; p < p1; ++p) {
            const int idx = topo.houses_flat[p];
            if (topo.cell_row[idx] == r1 || topo.cell_col[idx] == c2) continue;
            if (apply_fish_anchor(idx)) break;
        }

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_squirmbag(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        // Squirmbag (Starfish) to ryba 5x5, wymaga n >= 5.
        if (n < 5) return false;

        int rows[5];
        int cols[5];
        
        rows[0] = static_cast<int>(rng() % static_cast<uint64_t>(n));
        cols[0] = static_cast<int>(rng() % static_cast<uint64_t>(n));
        
        for(int i = 1; i < 5; ++i) {
            rows[i] = random_distinct_index(n, rows[i-1], rng);
            bool ok = false;
            for(int j=0; j<100 && !ok; ++j) {
                ok = true;
                for(int k=0; k<i; ++k) if(rows[k] == rows[i]) ok = false;
                if(!ok) rows[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            
            cols[i] = random_distinct_index(n, cols[i-1], rng);
            ok = false;
            for(int j=0; j<100 && !ok; ++j) {
                ok = true;
                for(int k=0; k<i; ++k) if(cols[k] == cols[i]) ok = false;
                if(!ok) cols[i] = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
        }

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const uint64_t d1_bit = (1ULL << (d1 - 1));

        // PIGEONHOLE FIX: Unikalne wylosowanie dla każdego punktu siatki ryby
        for (int ri = 0; ri < 5; ++ri) {
            for (int ci = 0; ci < 5; ++ci) {
                int d_other = random_digit_distinct(n, d1_bit, rng);
                plan.add_anchor(rows[ri] * n + cols[ci], d1_bit | (1ULL << (d_other - 1)));
            }
        }

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 25);
        return plan.valid;
    }

    static bool build_als_xy_wing(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 4) return false;
        
        const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[pivot];
        const int col = topo.cell_col[pivot];
        
        // Musisz wylosować DWIE komórki dla pivota, obie widzące się w tym samym "domku"
        // Najlepiej z tego samego rzędu
        int col2 = (col + 1) % n;
        int p_cell1 = pivot;
        int p_cell2 = row * n + col2;
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        
        const uint64_t xy = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t xz = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1));
        const uint64_t yz = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        
        // Pivot ALS (2 komórki, 3 kandydatów)
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
        
        // Wings (Pojedyncze komórki tworzące degenerację ALS)
        plan.add_anchor(w1, xz);
        plan.add_anchor(w2, yz);
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
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

        // Dwa ALS współdzielące cyfrę X (z ograniczeniem) oraz Z.
        plan.add_anchor(r1 * n + c1, x | z | a); // ALS A
        plan.add_anchor(r1 * n + c2, a | z);
        
        plan.add_anchor(r2 * n + c1, x | z | b); // ALS B
        plan.add_anchor(r2 * n + c2, b | z);

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
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

        // 3 komórki w tym samym rzędzie i bloku dla Pivota
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
        // Pivot = WXYZ (Rozmiar 3, popcount 4)
        plan.add_anchor(p1, pivot_mask);
        plan.add_anchor(p2, pivot_mask);
        plan.add_anchor(p3, pivot_mask);
        
        // Wings outside the box but seeing the pivot
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
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 6);
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
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= (aic_mode ? 7 : 6));
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
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= (triple_mode ? 3 : 2));
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

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int dt = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        
        const uint64_t a = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t b = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        const uint64_t c = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1));
        
        // aic path: A(d1,d2) -r1- B(d2,d3) -c2- C(d3,d1)
        plan.add_anchor(r1 * n + c1, a); // A
        plan.add_anchor(r1 * n + c2, b); // B
        plan.add_anchor(r2 * n + c2, c); // C
        
        force_strong_link_row(topo, plan, r1, c1, c2, d2);
        force_strong_link_col(topo, plan, c2, r1, r2, d3);

        // TARGET INJECTION: Komórka widząca A(r1,c1) i C(r2,c2). 
        int tr = r2;
        int tc = c1;
        plan.add_anchor(tr * n + tc, (1ULL << (d1 - 1)) | (1ULL << (dt - 1)));

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }

    static bool build_grouped_aic(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6 || topo.box_rows <= 1 || topo.box_cols <= 1) return false;
        
        const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        const int row = topo.cell_row[pivot];
        const int box = topo.cell_box[pivot];
        
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n)) + 1;
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        
        const uint64_t a = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t b = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        const uint64_t c = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1));
        
        plan.add_anchor(pivot, a);
        for (int cc = 0; cc < n && plan.anchor_count < 3; ++cc) {
            const int idx = row * n + cc;
            if (idx == pivot || topo.cell_box[idx] != box) continue;
            plan.add_anchor(idx, b);
        }
        
        const int house = 2 * n + box;
        const int p0 = topo.house_offsets[house];
        const int p1 = topo.house_offsets[house + 1];
        for (int p = p0; p < p1 && plan.anchor_count < 6; ++p) {
            const int idx = topo.houses_flat[p];
            if (idx == pivot || topo.cell_row[idx] == row) continue;
            plan.add_anchor(idx, c);
        }
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
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
        const int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        const uint64_t core = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        
        plan.add_anchor(r1 * n + c1, core);
        plan.add_anchor(r1 * n + c2, core);
        plan.add_anchor(r2 * n + c1, core);
        plan.add_anchor(r2 * n + c2, core);
        
        for (int rr = 0; rr < n && plan.anchor_count < 6; ++rr) {
            if (rr == r1 || rr == r2) continue;
            if (topo.cell_box[rr * n + c1] == topo.cell_box[r1 * n + c1]) {
                plan.add_anchor(rr * n + c1, core);
            }
        }
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
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
        int d2 = random_digit_distinct(n, (1ULL << (d1 - 1)), rng);
        int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        
        uint64_t a = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        uint64_t b = (1ULL << (d2 - 1)) | (1ULL << (d3 - 1));
        uint64_t c = (1ULL << (d1 - 1)) | (1ULL << (d3 - 1));
        
        plan.add_anchor(r1 * n + c1, a);
        plan.add_anchor(r1 * n + c2, b);
        plan.add_anchor(r2 * n + c2, c);
        plan.add_anchor(r2 * n + c1, a);
        
        force_strong_link_row(topo, plan, r1, c1, c2, d2);
        force_strong_link_col(topo, plan, c2, r1, r2, d3);
        force_strong_link_row(topo, plan, r2, c1, c2, d1);
        force_strong_link_col(topo, plan, c1, r1, r2, d1);

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
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
        const uint64_t target = (1ULL << (d1 - 1));

        auto get_mask = [&]() {
            return target | (1ULL << (random_digit_distinct(n, target, rng) - 1));
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
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 6);
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
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
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
            if (row_out < 0) row_out = row * n + cc;
        }
        for (int rr = 0; rr < n; ++rr) {
            if (rr >= r0 && rr < r0 + topo.box_rows) continue;
            if (col_out < 0) col_out = rr * n + col;
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
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        const int d4 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)) | (1ULL << (d3 - 1)), rng);
        
        const uint64_t core = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t line_mask = core | (1ULL << (d3 - 1));
        const uint64_t box_mask = core | (1ULL << (d4 - 1));

        plan.add_anchor(pivot, core);
        plan.add_anchor(row_out, core);
        plan.add_anchor(col_out, core);
        plan.add_anchor(box_support, box_mask);
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 4);
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
        const int d3 = random_digit_distinct(n, (1ULL << (d1 - 1)) | (1ULL << (d2 - 1)), rng);
        
        const uint64_t pair = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
        const uint64_t row_victim = pair | (1ULL << (d3 - 1));
        
        plan.add_anchor(r1 * n + c1, pair);
        plan.add_anchor(r1 * n + c2, pair);
        plan.add_anchor(r2 * n + c2, pair);
        plan.add_anchor(r2 * n + c1, pair);
        plan.add_anchor(r1 * n + c3, row_victim);
        
        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= 5);
        return plan.valid;
    }

    static bool build_large_fish(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, int line_count, bool finned_mode) {
        plan = {};
        const int n = topo.n;
        if (n < std::max(6, line_count + 2)) return false;

        int rows[5] = { -1, -1, -1, -1, -1 };
        int cols[5] = { -1, -1, -1, -1, -1 };
        
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

        // PIGEONHOLE FIX: Każda kotwica dużej ryby musi mieć inną `d_other`
        for (int ri = 0; ri < line_count; ++ri) {
            for (int ci = 0; ci < line_count; ++ci) {
                int d_other = random_digit_distinct(n, bit, rng);
                plan.add_anchor(rows[ri] * n + cols[ci], bit | (1ULL << (d_other - 1)));
            }
        }
        
        if (finned_mode) {
            int extra_col = cols[0];
            for (int g = 0; g < 96 && std::find(cols, cols + line_count, extra_col) != cols + line_count; ++g) {
                extra_col = static_cast<int>(rng() % static_cast<uint64_t>(n));
            }
            if (std::find(cols, cols + line_count, extra_col) == cols + line_count) {
                int d_other1 = random_digit_distinct(n, bit, rng);
                plan.add_anchor(rows[0] * n + extra_col, bit | (1ULL << (d_other1 - 1)));
            }
        }
        
        // Zablokuj komórki poza rybą
        for (int ri = 0; ri < line_count; ++ri) {
            const uint64_t mask = ~bit & pf_full_mask_for_n(n);
            for (int cc = 0; cc < n; ++cc) {
                if (std::find(cols, cols + line_count, cc) == cols + line_count) {
                    plan.add_skeleton(rows[ri] * n + cc, mask);
                }
            }
        }

        plan.explicit_skeleton = true;
        plan.valid = (plan.anchor_count >= line_count * line_count);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing