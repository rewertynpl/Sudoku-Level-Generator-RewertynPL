// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: death_blossom.h (Poziom 7 - Nightmare)
// Opis: Algorytm rozwiązujący struktury Death Blossom. 
//       Wyszukuje komórkę główną (Pivot) zawierającą N kandydatów, 
//       z którą łączy się N komórek dwuwartościowych (Płatków - Petals), 
//       gdzie każdy płatek dzieli z Pivotem jedną unikalną cyfrę, a 
//       wszystkie płatki dzielą między sobą wspólną "obcą" cyfrę Z.
//       Zoptymalizowany dla Zero-Allocation HPC, N=64 i geometrii asymetrycznej.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <bit>
#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline ApplyResult apply_death_blossom(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    
    // Odrzucenie zbyt pustych siatek, gdzie zaawansowane łańcuchy są przedwczesne
    // i grożą kombinatoryczną eksplozją.
    if (n > 64 || st.board->empty_cells > (nn - std::max((5 * n) / 4, n))) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    bool progress = false;

    // petals[digit-1][] przechowuje komórki bivalue, które widzą pivot i zawierają dany digit
    int petals[64][192]{};
    int petal_cnt[64]{};
    int pivot_digits[64]{};

    for (int pivot = 0; pivot < nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;

        const uint64_t pivot_mask = st.cands[pivot];
        const int pivot_pc = std::popcount(pivot_mask);
        
        // Ograniczamy do 3 lub 4 płatków. Powyżej 4 to matematyczna rzadkość 
        // nawet w asymetrycznym Sudoku i obciąża pipeline bez zysku.
        if (pivot_pc < 3 || pivot_pc > 4) continue;

        std::fill_n(petal_cnt, n, 0);
        int pivot_digit_cnt = 0;
        
        uint64_t w = pivot_mask;
        while (w != 0ULL && pivot_digit_cnt < n) {
            const uint64_t bit = config::bit_lsb(w);
            w = config::bit_clear_lsb_u64(w);
            pivot_digits[pivot_digit_cnt++] = static_cast<int>(std::countr_zero(bit)) + 1;
        }

        // Zbieranie powiązanych płatków dwuwartościowych z pola widzenia pivota
        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        for (int p = p0; p < p1; ++p) {
            const int peer = st.topo->peers_flat[p];
            if (st.board->values[peer] != 0) continue;
            
            const uint64_t pm = st.cands[peer];
            if (std::popcount(pm) != 2) continue; // Ścisły Death Blossom operuje tylko na bivalue

            for (int di = 0; di < pivot_digit_cnt; ++di) {
                const int d = pivot_digits[di];
                if ((pm & (1ULL << (d - 1))) != 0ULL) {
                    if (petal_cnt[d - 1] < 192) {
                        petals[d - 1][petal_cnt[d - 1]++] = peer;
                    }
                }
            }
        }

        // ====================================================================
        // Rozwiązanie dla Death Blossom z 3 płatkami (Gdy Pivot ma 3 kandydatów)
        // ====================================================================
        if (pivot_pc == 3) {
            const int da = pivot_digits[0];
            const int db = pivot_digits[1];
            const int dc = pivot_digits[2];
            
            if (petal_cnt[da-1] == 0 || petal_cnt[db-1] == 0 || petal_cnt[dc-1] == 0) continue;

            for (int pa = 0; pa < petal_cnt[da-1]; ++pa) {
                const int p_a = petals[da-1][pa];
                const uint64_t z_mask_a = st.cands[p_a] & ~(1ULL << (da - 1));
                
                for (int pb = 0; pb < petal_cnt[db-1]; ++pb) {
                    const int p_b = petals[db-1][pb];
                    if (p_a == p_b) continue;
                    
                    const uint64_t z_mask_b = st.cands[p_b] & ~(1ULL << (db - 1));
                    if (z_mask_a != z_mask_b) continue; // Płatki muszą mieć wspólną cyfrę wyjściową Z

                    for (int pc = 0; pc < petal_cnt[dc-1]; ++pc) {
                        const int p_c = petals[dc-1][pc];
                        if (p_c == p_a || p_c == p_b) continue;
                        
                        const uint64_t z_mask_c = st.cands[p_c] & ~(1ULL << (dc - 1));
                        if (z_mask_a != z_mask_c) continue;

                        const uint64_t elim_bit = z_mask_a;
                        for (int t = 0; t < nn; ++t) {
                            if (st.board->values[t] != 0 || t == pivot) continue;
                            if (t == p_a || t == p_b || t == p_c) continue;
                            if ((st.cands[t] & elim_bit) == 0ULL) continue;
                            
                            // Cel musi widzieć WSZYSTKIE płatki naraz
                            if (!st.is_peer(t, p_a) || !st.is_peer(t, p_b) || !st.is_peer(t, p_c)) continue;

                            const ApplyResult er = st.eliminate(t, elim_bit);
                            if (er == ApplyResult::Contradiction) { 
                                s.elapsed_ns += st.now_ns() - t0; 
                                return er; 
                            }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }
        // ====================================================================
        // Rozwiązanie dla Death Blossom z 4 płatkami (Gdy Pivot ma 4 kandydatów)
        // ====================================================================
        else if (pivot_pc == 4) {
            const int da = pivot_digits[0], db = pivot_digits[1], dc = pivot_digits[2], dd = pivot_digits[3];
            
            if (petal_cnt[da-1] == 0 || petal_cnt[db-1] == 0 || petal_cnt[dc-1] == 0 || petal_cnt[dd-1] == 0) continue;

            for (int pa = 0; pa < petal_cnt[da-1]; ++pa) {
                const int p_a = petals[da-1][pa];
                const uint64_t z_mask_a = st.cands[p_a] & ~(1ULL << (da - 1));
                
                for (int pb = 0; pb < petal_cnt[db-1]; ++pb) {
                    const int p_b = petals[db-1][pb];
                    if (p_a == p_b || (st.cands[p_b] & ~(1ULL << (db - 1))) != z_mask_a) continue;
                    
                    for (int pc = 0; pc < petal_cnt[dc-1]; ++pc) {
                        const int p_c = petals[dc-1][pc];
                        if (p_c == p_a || p_c == p_b || (st.cands[p_c] & ~(1ULL << (dc - 1))) != z_mask_a) continue;
                        
                        for (int pd = 0; pd < petal_cnt[dd-1]; ++pd) {
                            const int p_d = petals[dd-1][pd];
                            if (p_d == p_a || p_d == p_b || p_d == p_c || (st.cands[p_d] & ~(1ULL << (dd - 1))) != z_mask_a) continue;

                            const uint64_t elim_bit = z_mask_a;
                            for (int t = 0; t < nn; ++t) {
                                if (st.board->values[t] != 0 || t == pivot) continue;
                                if (t == p_a || t == p_b || t == p_c || t == p_d) continue;
                                if ((st.cands[t] & elim_bit) == 0ULL) continue;
                                
                                // Cel musi widzieć WSZYSTKIE płatki naraz
                                if (!st.is_peer(t, p_a) || !st.is_peer(t, p_b) || 
                                    !st.is_peer(t, p_c) || !st.is_peer(t, p_d)) continue;

                                const ApplyResult er = st.eliminate(t, elim_bit);
                                if (er == ApplyResult::Contradiction) { 
                                    s.elapsed_ns += st.now_ns() - t0; 
                                    return er; 
                                }
                                progress = progress || (er == ApplyResult::Progress);
                            }
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_death_blossom = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
