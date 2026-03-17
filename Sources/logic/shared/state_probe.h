// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: state_probe.h
// Opis: Wykonywanie "zrzutów" stanu planszy, bezpiecznych symulacji (Probing)
//       oraz szybkiej propagacji singli (Naked & Hidden) z gwarancją Zero-Allocation.
//       Rozwiązany problem dławienia łańcuchów wymuszających (P7/P8).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>

#include "exact_pattern_scratchpad.h"
#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"

namespace sudoku_hpc::logic::shared {

inline void snapshot_state(CandidateState& st, ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    std::copy_n(st.cands, nn, sp.dyn_cands_backup);
    std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
    std::copy_n(st.board->row_used.data(), n, sp.dyn_row_used_backup);
    std::copy_n(st.board->col_used.data(), n, sp.dyn_col_used_backup);
    std::copy_n(st.board->box_used.data(), n, sp.dyn_box_used_backup);
    sp.dyn_empty_backup = st.board->empty_cells;
}

inline void restore_state(CandidateState& st, ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    std::copy_n(sp.dyn_cands_backup, nn, st.cands);
    std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
    std::copy_n(sp.dyn_row_used_backup, n, st.board->row_used.data());
    std::copy_n(sp.dyn_col_used_backup, n, st.board->col_used.data());
    std::copy_n(sp.dyn_box_used_backup, n, st.board->box_used.data());
    st.board->empty_cells = sp.dyn_empty_backup;
}

inline void snapshot_state_slot(CandidateState& st, ExactPatternScratchpad& sp, int slot) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    std::copy_n(st.cands, nn, sp.p8_cands_backup[slot]);
    std::copy_n(st.board->values.data(), nn, sp.p8_values_backup[slot]);
    std::copy_n(st.board->row_used.data(), n, sp.p8_row_used_backup[slot]);
    std::copy_n(st.board->col_used.data(), n, sp.p8_col_used_backup[slot]);
    std::copy_n(st.board->box_used.data(), n, sp.p8_box_used_backup[slot]);
    sp.p8_empty_backup[slot] = st.board->empty_cells;
}

inline void restore_state_slot(CandidateState& st, ExactPatternScratchpad& sp, int slot) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    std::copy_n(sp.p8_cands_backup[slot], nn, st.cands);
    std::copy_n(sp.p8_values_backup[slot], nn, st.board->values.data());
    std::copy_n(sp.p8_row_used_backup[slot], n, st.board->row_used.data());
    std::copy_n(sp.p8_col_used_backup[slot], n, st.board->col_used.data());
    std::copy_n(sp.p8_box_used_backup[slot], n, st.board->box_used.data());
    st.board->empty_cells = sp.p8_empty_backup[slot];
}

inline uint64_t* intersection_slot(ExactPatternScratchpad& sp, int slot) {
    return sp.p8_intersection_backup[slot];
}

// Szybka propagacja singli na głębokich symulacjach (Forcing Chains, P8)
inline bool propagate_singles(CandidateState& st, int max_steps) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    
    for (int step = 0; step < max_steps; ++step) {
        bool changed = false;
        
        // 1. Naked Singles - Pusty węzeł ma już tylko 1 kandydata
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            const uint64_t m = st.cands[idx];
            if (m == 0ULL) return false; // Sprzeczność! Kandydaci wyczerpani.
            
            // Szybki bit-trick: czy maska ma tylko jeden zapalony bit (potęga dwójki)?
            if ((m & (m - 1ULL)) == 0ULL) {
                if (!st.place(idx, config::bit_ctz_u64(m) + 1)) return false;
                changed = true;
            }
        }
        
        // 2. Hidden Singles - Cyfra występuje tylko w jednej dostępnej komórce dla domku
        // Zero-Allocation z użyciem wstępnie przygotowanej topologii
        const size_t house_count = st.topo->house_offsets.size() - 1;
        for (size_t h = 0; h < house_count; ++h) {
            const int p0 = st.topo->house_offsets[h];
            const int p1 = st.topo->house_offsets[h + 1];
            
            for (int d = 1; d <= n; ++d) {
                const uint64_t bit = 1ULL << (d - 1);
                int pos = -1, cnt = 0;
                
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[p];
                    
                    // Jeśli cyfra jest już postawiona w domku, przechodzimy do kolejnej
                    if (st.board->values[idx] == d) { 
                        cnt = 2; 
                        break; 
                    }
                    
                    if (st.board->values[idx] == 0 && (st.cands[idx] & bit)) {
                        pos = idx;
                        ++cnt;
                    }
                }
                
                if (cnt == 1 && pos != -1) {
                    if (!st.place(pos, d)) return false; // Sprzeczność!
                    changed = true;
                }
            }
        }
        
        if (!changed) break;
    }
    
    return true;
}

inline bool probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int digit,
    int max_steps,
    ExactPatternScratchpad& sp) {
    
    snapshot_state(st, sp);

    bool contradiction = false;
    
    // Próbujemy wstawić kandydata i odpalić propagację
    if (!st.place(idx, digit)) {
        contradiction = true;
    } else if (!propagate_singles(st, max_steps)) {
        contradiction = true;
    }

    restore_state(st, sp);
    return contradiction;
}

} // namespace sudoku_hpc::logic::shared
