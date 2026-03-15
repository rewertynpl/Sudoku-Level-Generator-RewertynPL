// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: pattern_overlay.h (Poziom 8 - Theoretical)
// Opis: Algorytm Pattern Overlay Method (POM). Przeszukuje czÄ™Ĺ›ciowe, 
//       wolne od sprzecznoĹ›ci nakĹ‚adki dla pojedynczej cyfry i eliminuje 
//       kandydatĂłw, ktĂłrzy nigdy nie wejdÄ… w skĹ‚ad prawidĹ‚owego rozwiÄ…zania.
//       RozwiÄ…zanie Zero-Allocation operujÄ…ce na pĹ‚ytkim wirtualnym stosie.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/required_strategy_gate.h"
#include "../shared/state_probe.h"
#include "p8_density.h"

// DoĹ‚Ä…czamy wymagane moduły dla POM 
#include "msls.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool pom_propagate_singles(CandidateState& st, int max_steps) {
    return shared::propagate_singles(st, max_steps);
}

inline bool pom_probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int d,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    return shared::probe_candidate_contradiction(st, idx, d, max_steps, sp);
}

inline ApplyResult pom_apply_house_overlay_elims(
    CandidateState& st,
    const uint64_t* inter_cands);

inline void pom_capture_intersection(
    CandidateState& st,
    uint64_t* inter_cands);

struct PomUndoMarker {
    int cand_marker = 0;
    int place_marker = 0;
};

inline PomUndoMarker pom_begin_frame(const shared::ExactPatternScratchpad& sp) {
    return PomUndoMarker{sp.pom_undo_count, sp.pom_place_count};
}

inline bool pom_push_cand_undo(
    shared::ExactPatternScratchpad& sp,
    int idx,
    uint64_t old_mask) {
    if (sp.pom_undo_count >= shared::ExactPatternScratchpad::MAX_POM_UNDO) {
        return false;
    }
    sp.pom_undo_idx[static_cast<size_t>(sp.pom_undo_count)] = idx;
    sp.pom_undo_old_mask[static_cast<size_t>(sp.pom_undo_count)] = old_mask;
    ++sp.pom_undo_count;
    return true;
}

inline bool pom_place_with_undo(
    CandidateState& st,
    int idx,
    int d,
    shared::ExactPatternScratchpad& sp) {
    if (st.board->values[idx] != 0) {
        return st.board->values[idx] == static_cast<uint16_t>(d);
    }

    const uint64_t bit = (1ULL << (d - 1));
    if ((st.cands[idx] & bit) == 0ULL) return false;
    if (!st.board->can_place(idx, d)) return false;

    if (!pom_push_cand_undo(sp, idx, st.cands[idx])) return false;
    if (sp.pom_place_count >= shared::ExactPatternScratchpad::MAX_NN) return false;
    st.board->place(idx, d);
    sp.pom_place_idx[static_cast<size_t>(sp.pom_place_count)] = idx;
    sp.pom_place_digit[static_cast<size_t>(sp.pom_place_count)] = static_cast<uint16_t>(d);
    ++sp.pom_place_count;
    st.cands[idx] = 0ULL;

    const int p0 = st.topo->peer_offsets[idx];
    const int p1 = st.topo->peer_offsets[idx + 1];
    for (int p = p0; p < p1; ++p) {
        const int peer = st.topo->peers_flat[p];
        if (st.board->values[peer] != 0) continue;
        uint64_t& pm = st.cands[peer];
        if ((pm & bit) == 0ULL) continue;
        if (!pom_push_cand_undo(sp, peer, pm)) return false;
        pm &= ~bit;
        if (pm == 0ULL) return false;
    }
    return true;
}

inline bool pom_propagate_singles_with_undo(
    CandidateState& st,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    for (int step = 0; step < max_steps; ++step) {
        bool progress = false;
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            const uint64_t mask = st.cands[idx];
            if (mask == 0ULL) return false;
            if ((mask & (mask - 1ULL)) != 0ULL) continue;
            const int d = config::bit_ctz_u64(mask) + 1;
            if (!pom_place_with_undo(st, idx, d, sp)) return false;
            progress = true;
            break;
        }
        if (!progress) break;
    }
    return true;
}

inline void pom_rollback_to(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    const PomUndoMarker& marker) {
    while (sp.pom_place_count > marker.place_marker) {
        --sp.pom_place_count;
        const int idx = sp.pom_place_idx[static_cast<size_t>(sp.pom_place_count)];
        const int d = static_cast<int>(sp.pom_place_digit[static_cast<size_t>(sp.pom_place_count)]);
        st.board->unplace(idx, d);
    }
    while (sp.pom_undo_count > marker.cand_marker) {
        --sp.pom_undo_count;
        const int idx = sp.pom_undo_idx[static_cast<size_t>(sp.pom_undo_count)];
        st.cands[idx] = sp.pom_undo_old_mask[static_cast<size_t>(sp.pom_undo_count)];
    }
}

inline int pom_collect_house_digit_cells(
    CandidateState& st,
    int house,
    uint64_t bit,
    int* out_cells,
    int cap = 8) {
    const int p0 = st.topo->house_offsets[static_cast<size_t>(house)];
    const int p1 = st.topo->house_offsets[static_cast<size_t>(house + 1)];
    int count = 0;
    for (int p = p0; p < p1 && count < cap; ++p) {
        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
        if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
            out_cells[count++] = idx;
        }
    }
    return count;
}

inline void pom_reset_intersection(uint64_t* inter_cands, int nn) {
    for (int i = 0; i < nn; ++i) {
        inter_cands[i] = ~0ULL;
    }
}

inline void pom_capture_digit_paths_recursive(
    CandidateState& st,
    int d,
    uint64_t bit,
    const int* houses,
    int house_count,
    int depth,
    int branch_steps,
    uint64_t* inter_cands,
    int& valid_paths,
    shared::ExactPatternScratchpad& sp) {
    int branch_cells[8]{};
    const int branch_count = pom_collect_house_digit_cells(st, houses[depth], bit, branch_cells, 8);
    if (branch_count < 1 || branch_count > 3) return;

    const PomUndoMarker frame = pom_begin_frame(sp);
    for (int i = 0; i < branch_count; ++i) {
        pom_rollback_to(st, sp, frame);
        if (!pom_place_with_undo(st, branch_cells[i], d, sp)) continue;
        if (!pom_propagate_singles_with_undo(st, branch_steps, sp)) continue;
        if (depth + 1 >= house_count) {
            ++valid_paths;
            pom_capture_intersection(st, inter_cands);
        } else {
            pom_capture_digit_paths_recursive(
                st, d, bit, houses, house_count, depth + 1, branch_steps, inter_cands, valid_paths, sp);
        }
    }
    pom_rollback_to(st, sp, frame);
}

inline ApplyResult pom_apply_overlay_probes(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    uint64_t t0,
    uint64_t* inter_cands,
    int valid_paths,
    int probe_steps,
    int probe_budget_max,
    shared::ExactPatternScratchpad& sp) {
    if (valid_paths < 2) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const ApplyResult house_er = pom_apply_house_overlay_elims(st, inter_cands);
    if (house_er == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return house_er;
    }
    if (house_er == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_pattern_overlay_method = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    sp.reset_pom();
    int probe_budget = probe_budget_max;
    for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
        if (st.board->values[idx] != 0) continue;
        const uint64_t base = st.cands[idx];
        const uint64_t keep = inter_cands[idx] & base;
        if (keep == 0ULL) continue;
        uint64_t rm = base & ~keep;
        while (rm != 0ULL && probe_budget > 0) {
            const uint64_t rm_bit = config::bit_lsb(rm);
            rm = config::bit_clear_lsb_u64(rm);
            --probe_budget;
            const int dd = config::bit_ctz_u64(rm_bit) + 1;
            if (!pom_probe_candidate_contradiction(st, idx, dd, probe_steps, sp)) continue;
            const ApplyResult er = st.eliminate(idx, rm_bit);
            if (er == ApplyResult::Contradiction) {
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return er;
            }
            if (er == ApplyResult::Progress) {
                ++s.hit_count;
                r.used_pattern_overlay_method = true;
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return ApplyResult::Progress;
            }
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult pom_apply_house_overlay_elims(
    CandidateState& st,
    const uint64_t* inter_cands) {
    const int n = st.topo->n;
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    for (int h = 0; h < house_count; ++h) {
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int overlay_places = 0;
            int live_places = 0;
            int only_idx = -1;

            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) != 0ULL) {
                    ++live_places;
                }
                if ((inter_cands[idx] & bit) != 0ULL) {
                    ++overlay_places;
                    only_idx = idx;
                }
            }

            if (overlay_places == 0 || live_places <= overlay_places) continue;
            if (overlay_places == 1 && only_idx >= 0) {
                uint64_t drop = st.cands[only_idx] & ~bit;
                while (drop != 0ULL) {
                    const uint64_t rm = config::bit_lsb(drop);
                    drop = config::bit_clear_lsb_u64(drop);
                    const ApplyResult er = st.eliminate(only_idx, rm);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }

            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                if ((inter_cands[idx] & bit) != 0ULL) continue;
                const ApplyResult er = st.eliminate(idx, bit);
                if (er != ApplyResult::NoProgress) return er;
            }
        }
    }
    return ApplyResult::NoProgress;
}

inline void pom_capture_intersection(
    CandidateState& st,
    uint64_t* inter_cands) {
    const int nn = st.topo->nn;
    for (int i = 0; i < nn; ++i) {
        uint64_t cur = st.cands[i];
        if (st.board->values[i] != 0) {
            cur = (1ULL << (st.board->values[i] - 1));
        }
        inter_cands[i] &= cur;
    }
}

inline void pom_sort_house_candidates(
    int* houses,
    int* counts,
    int count) {
    for (int i = 1; i < count; ++i) {
        const int h = houses[i];
        const int c = counts[i];
        int j = i - 1;
        while (j >= 0 && counts[j] > c) {
            houses[j + 1] = houses[j];
            counts[j + 1] = counts[j];
            --j;
        }
        houses[j + 1] = h;
        counts[j + 1] = c;
    }
}

inline bool pom_global_family_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n > 20) return false;
    return !p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells);
}

inline bool pom_quad_global_family_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n > 16) return false;
    return !p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells);
}

inline bool pom_penta_global_family_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n > 12) return false;
    return !p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells);
}

inline bool pom_hexa_global_family_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n > 9) return false;
    return !p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells);
}

inline ApplyResult apply_pom_global_digit_hexa_family_exact(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (!pom_hexa_global_family_allowed(st)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    const int house_budget = 6;
    const int branch_steps = std::clamp(6 + n / 4, 8, 12);
    const int probe_steps = std::clamp(6 + n / 3, 8, 12);
    const int probe_budget_max = std::clamp(4 + n / 3, 6, 10);
    int houses[ExactPatternScratchpad::MAX_HOUSES]{};
    int counts[ExactPatternScratchpad::MAX_HOUSES]{};
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int hc = 0;
        for (int h = 0; h < house_count && hc < ExactPatternScratchpad::MAX_HOUSES; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                ++cnt;
                if (cnt > 4) break;
            }
            if (cnt >= 2 && cnt <= 4) {
                houses[hc] = h;
                counts[hc] = cnt;
                ++hc;
            }
        }
        if (hc < 6) continue;
        pom_sort_house_candidates(houses, counts, hc);
        const int limit = std::min(hc, house_budget);

        for (int h1i = 0; h1i < limit; ++h1i) {
            for (int h2i = h1i + 1; h2i < limit; ++h2i) {
                for (int h3i = h2i + 1; h3i < limit; ++h3i) {
                    for (int h4i = h3i + 1; h4i < limit; ++h4i) {
                        for (int h5i = h4i + 1; h5i < limit; ++h5i) {
                            for (int h6i = h5i + 1; h6i < limit; ++h6i) {
                                const int hs[6] = {houses[h1i], houses[h2i], houses[h3i], houses[h4i], houses[h5i], houses[h6i]};
                                bool ok = true;
                                for (int hi = 0; hi < 6; ++hi) {
                                    int tmp_cells[8]{};
                                    const int cc = pom_collect_house_digit_cells(st, hs[hi], bit, tmp_cells, 8);
                                    if (cc < 2 || cc > 3) {
                                        ok = false;
                                        break;
                                    }
                                }
                                if (!ok) continue;

                                sp.reset_pom();
                                const PomUndoMarker root_marker = pom_begin_frame(sp);
                                uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
                                pom_reset_intersection(inter_cands, nn);
                                int valid_paths = 0;
                                pom_capture_digit_paths_recursive(
                                    st, d, bit, hs, 6, 0, branch_steps, inter_cands, valid_paths, sp);
                                pom_rollback_to(st, sp, root_marker);

                                const ApplyResult overlay_er = pom_apply_overlay_probes(
                                    st, s, r, t0, inter_cands, valid_paths, probe_steps, probe_budget_max, sp);
                                if (overlay_er != ApplyResult::NoProgress) return overlay_er;
                }
            }
        }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_pom_global_digit_penta_family_exact(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (!pom_penta_global_family_allowed(st)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    const int house_budget = std::clamp(4 + n / 8, 5, 6);
    const int branch_steps = std::clamp(6 + n / 4, 8, 14);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    const int probe_budget_max = std::clamp(4 + n / 3, 6, 12);
    int houses[ExactPatternScratchpad::MAX_HOUSES]{};
    int counts[ExactPatternScratchpad::MAX_HOUSES]{};
    int cells1[8]{}, cells2[8]{}, cells3[8]{}, cells4[8]{}, cells5[8]{};
    auto& sp = shared::exact_pattern_scratchpad();
    auto* root_cands = sp.p8_cands_backup[0];
    auto* root_values = sp.p8_values_backup[0];
    auto* root_row_used = sp.p8_row_used_backup[0];
    auto* root_col_used = sp.p8_col_used_backup[0];
    auto* root_box_used = sp.p8_box_used_backup[0];
    auto* level1_cands = sp.p8_cands_backup[1];
    auto* level1_values = sp.p8_values_backup[1];
    auto* level1_row_used = sp.p8_row_used_backup[1];
    auto* level1_col_used = sp.p8_col_used_backup[1];
    auto* level1_box_used = sp.p8_box_used_backup[1];
    auto* level2_cands = sp.p8_cands_backup[2];
    auto* level2_values = sp.p8_values_backup[2];
    auto* level2_row_used = sp.p8_row_used_backup[2];
    auto* level2_col_used = sp.p8_col_used_backup[2];
    auto* level2_box_used = sp.p8_box_used_backup[2];
    auto* level3_cands = sp.p8_cands_backup[3];
    auto* level3_values = sp.p8_values_backup[3];
    auto* level3_row_used = sp.p8_row_used_backup[3];
    auto* level3_col_used = sp.p8_col_used_backup[3];
    auto* level3_box_used = sp.p8_box_used_backup[3];
    auto* level4_cands = sp.p8_cands_backup[4];
    auto* level4_values = sp.p8_values_backup[4];
    auto* level4_row_used = sp.p8_row_used_backup[4];
    auto* level4_col_used = sp.p8_col_used_backup[4];
    auto* level4_box_used = sp.p8_box_used_backup[4];

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int hc = 0;
        for (int h = 0; h < house_count && hc < ExactPatternScratchpad::MAX_HOUSES; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                ++cnt;
                if (cnt > 4) break;
            }
            if (cnt >= 2 && cnt <= 4) {
                houses[hc] = h;
                counts[hc] = cnt;
                ++hc;
            }
        }
        if (hc < 5) continue;
        pom_sort_house_candidates(houses, counts, hc);
        const int limit = std::min(hc, house_budget);

        for (int h1i = 0; h1i < limit; ++h1i) {
            for (int h2i = h1i + 1; h2i < limit; ++h2i) {
                for (int h3i = h2i + 1; h3i < limit; ++h3i) {
                    for (int h4i = h3i + 1; h4i < limit; ++h4i) {
                        for (int h5i = h4i + 1; h5i < limit; ++h5i) {
                            const int h1 = houses[h1i];
                            const int h2 = houses[h2i];
                            const int h3 = houses[h3i];
                            const int h4 = houses[h4i];
                            const int h5 = houses[h5i];
                            int c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
                            const int p10 = st.topo->house_offsets[static_cast<size_t>(h1)];
                            const int p11 = st.topo->house_offsets[static_cast<size_t>(h1 + 1)];
                            const int p20 = st.topo->house_offsets[static_cast<size_t>(h2)];
                            const int p21 = st.topo->house_offsets[static_cast<size_t>(h2 + 1)];
                            const int p30 = st.topo->house_offsets[static_cast<size_t>(h3)];
                            const int p31 = st.topo->house_offsets[static_cast<size_t>(h3 + 1)];
                            const int p40 = st.topo->house_offsets[static_cast<size_t>(h4)];
                            const int p41 = st.topo->house_offsets[static_cast<size_t>(h4 + 1)];
                            const int p50 = st.topo->house_offsets[static_cast<size_t>(h5)];
                            const int p51 = st.topo->house_offsets[static_cast<size_t>(h5 + 1)];
                            for (int p = p10; p < p11 && c1 < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells1[c1++] = idx;
                            }
                            for (int p = p20; p < p21 && c2 < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells2[c2++] = idx;
                            }
                            for (int p = p30; p < p31 && c3 < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells3[c3++] = idx;
                            }
                            for (int p = p40; p < p41 && c4 < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells4[c4++] = idx;
                            }
                            for (int p = p50; p < p51 && c5 < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells5[c5++] = idx;
                            }
                            if (c1 < 2 || c2 < 2 || c3 < 2 || c4 < 2 || c5 < 2) continue;
                            if (c1 > 3 || c2 > 3 || c3 > 3 || c4 > 3 || c5 > 3) continue;

                            {
                                const int hs[5] = {h1, h2, h3, h4, h5};
                                sp.reset_pom();
                                const PomUndoMarker root_marker = pom_begin_frame(sp);
                                auto* inter_cands_fast = shared::intersection_slot(sp, 5);
                                pom_reset_intersection(inter_cands_fast, nn);
                                int valid_paths_fast = 0;
                                pom_capture_digit_paths_recursive(
                                    st, d, bit, hs, 5, 0, branch_steps, inter_cands_fast, valid_paths_fast, sp);
                                pom_rollback_to(st, sp, root_marker);

                                const ApplyResult overlay_er = pom_apply_overlay_probes(
                                    st, s, r, t0, inter_cands_fast, valid_paths_fast, probe_steps, probe_budget_max, sp);
                                if (overlay_er != ApplyResult::NoProgress) return overlay_er;
                                continue;
                            }

                            std::copy_n(st.cands, nn, root_cands);
                            std::copy_n(st.board->values.data(), nn, root_values);
                            std::copy_n(st.board->row_used.data(), n, root_row_used);
                            std::copy_n(st.board->col_used.data(), n, root_col_used);
                            std::copy_n(st.board->box_used.data(), n, root_box_used);
                            const int root_empty = st.board->empty_cells;

                            auto* inter_cands = shared::intersection_slot(sp, 6);
                            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
                            int valid_paths = 0;

                            for (int i = 0; i < c1; ++i) {
                                std::copy_n(root_cands, nn, st.cands);
                                std::copy_n(root_values, nn, st.board->values.data());
                                std::copy_n(root_row_used, n, st.board->row_used.data());
                                std::copy_n(root_col_used, n, st.board->col_used.data());
                                std::copy_n(root_box_used, n, st.board->box_used.data());
                                st.board->empty_cells = root_empty;
                                if (!st.place(cells1[i], d) || !pom_propagate_singles(st, branch_steps)) continue;

                                std::copy_n(st.cands, nn, level1_cands);
                                std::copy_n(st.board->values.data(), nn, level1_values);
                                std::copy_n(st.board->row_used.data(), n, level1_row_used);
                                std::copy_n(st.board->col_used.data(), n, level1_col_used);
                                std::copy_n(st.board->box_used.data(), n, level1_box_used);
                                const int level1_empty = st.board->empty_cells;

                                int branch2_count = 0;
                                int branch2_cells[8]{};
                                for (int p = p20; p < p21 && branch2_count < 8; ++p) {
                                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                    if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch2_cells[branch2_count++] = idx;
                                }
                                if (branch2_count < 1 || branch2_count > 3) continue;

                                for (int j = 0; j < branch2_count; ++j) {
                                    std::copy_n(level1_cands, nn, st.cands);
                                    std::copy_n(level1_values, nn, st.board->values.data());
                                    std::copy_n(level1_row_used, n, st.board->row_used.data());
                                    std::copy_n(level1_col_used, n, st.board->col_used.data());
                                    std::copy_n(level1_box_used, n, st.board->box_used.data());
                                    st.board->empty_cells = level1_empty;
                                    if (!st.place(branch2_cells[j], d) || !pom_propagate_singles(st, branch_steps)) continue;

                                    std::copy_n(st.cands, nn, level2_cands);
                                    std::copy_n(st.board->values.data(), nn, level2_values);
                                    std::copy_n(st.board->row_used.data(), n, level2_row_used);
                                    std::copy_n(st.board->col_used.data(), n, level2_col_used);
                                    std::copy_n(st.board->box_used.data(), n, level2_box_used);
                                    const int level2_empty = st.board->empty_cells;

                                    int branch3_count = 0;
                                    int branch3_cells[8]{};
                                    for (int p = p30; p < p31 && branch3_count < 8; ++p) {
                                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                        if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch3_cells[branch3_count++] = idx;
                                    }
                                    if (branch3_count < 1 || branch3_count > 3) continue;

                                    for (int k = 0; k < branch3_count; ++k) {
                                        std::copy_n(level2_cands, nn, st.cands);
                                        std::copy_n(level2_values, nn, st.board->values.data());
                                        std::copy_n(level2_row_used, n, st.board->row_used.data());
                                        std::copy_n(level2_col_used, n, st.board->col_used.data());
                                        std::copy_n(level2_box_used, n, st.board->box_used.data());
                                        st.board->empty_cells = level2_empty;
                                        if (!st.place(branch3_cells[k], d) || !pom_propagate_singles(st, branch_steps)) continue;

                                        std::copy_n(st.cands, nn, level3_cands);
                                        std::copy_n(st.board->values.data(), nn, level3_values);
                                        std::copy_n(st.board->row_used.data(), n, level3_row_used);
                                        std::copy_n(st.board->col_used.data(), n, level3_col_used);
                                        std::copy_n(st.board->box_used.data(), n, level3_box_used);
                                        const int level3_empty = st.board->empty_cells;

                                        int branch4_count = 0;
                                        int branch4_cells[8]{};
                                        for (int p = p40; p < p41 && branch4_count < 8; ++p) {
                                            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch4_cells[branch4_count++] = idx;
                                        }
                                        if (branch4_count < 1 || branch4_count > 3) continue;

                                        for (int m = 0; m < branch4_count; ++m) {
                                            std::copy_n(level3_cands, nn, st.cands);
                                            std::copy_n(level3_values, nn, st.board->values.data());
                                            std::copy_n(level3_row_used, n, st.board->row_used.data());
                                            std::copy_n(level3_col_used, n, st.board->col_used.data());
                                            std::copy_n(level3_box_used, n, st.board->box_used.data());
                                            st.board->empty_cells = level3_empty;
                                            if (!st.place(branch4_cells[m], d) || !pom_propagate_singles(st, branch_steps)) continue;

                                            std::copy_n(st.cands, nn, level4_cands);
                                            std::copy_n(st.board->values.data(), nn, level4_values);
                                            std::copy_n(st.board->row_used.data(), n, level4_row_used);
                                            std::copy_n(st.board->col_used.data(), n, level4_col_used);
                                            std::copy_n(st.board->box_used.data(), n, level4_box_used);
                                            const int level4_empty = st.board->empty_cells;

                                            int branch5_count = 0;
                                            int branch5_cells[8]{};
                                            for (int p = p50; p < p51 && branch5_count < 8; ++p) {
                                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch5_cells[branch5_count++] = idx;
                                            }
                                            if (branch5_count < 1 || branch5_count > 3) continue;

                                            for (int q = 0; q < branch5_count; ++q) {
                                                std::copy_n(level4_cands, nn, st.cands);
                                                std::copy_n(level4_values, nn, st.board->values.data());
                                                std::copy_n(level4_row_used, n, st.board->row_used.data());
                                                std::copy_n(level4_col_used, n, st.board->col_used.data());
                                                std::copy_n(level4_box_used, n, st.board->box_used.data());
                                                st.board->empty_cells = level4_empty;
                                                if (!st.place(branch5_cells[q], d) || !pom_propagate_singles(st, branch_steps)) continue;
                                                ++valid_paths;
                                                pom_capture_intersection(st, inter_cands);
                                            }
                                        }
                                    }
                                }
                            }

                            std::copy_n(root_cands, nn, st.cands);
                            std::copy_n(root_values, nn, st.board->values.data());
                            std::copy_n(root_row_used, n, st.board->row_used.data());
                            std::copy_n(root_col_used, n, st.board->col_used.data());
                            std::copy_n(root_box_used, n, st.board->box_used.data());
                            st.board->empty_cells = root_empty;

                            if (valid_paths < 2) continue;

                            const ApplyResult house_er = pom_apply_house_overlay_elims(st, inter_cands);
                            if (house_er == ApplyResult::Contradiction) {
                                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                return house_er;
                            }
                            if (house_er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_pattern_overlay_method = true;
                                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                return ApplyResult::Progress;
                            }

                            int probe_budget = probe_budget_max;
                            for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
                                if (st.board->values[idx] != 0) continue;
                                const uint64_t base = st.cands[idx];
                                const uint64_t keep = inter_cands[idx] & base;
                                if (keep == 0ULL) continue;
                                uint64_t rm = base & ~keep;
                                while (rm != 0ULL && probe_budget > 0) {
                                    const uint64_t rm_bit = config::bit_lsb(rm);
                                    rm = config::bit_clear_lsb_u64(rm);
                                    --probe_budget;
                                    const int dd = config::bit_ctz_u64(rm_bit) + 1;
                                    if (!pom_probe_candidate_contradiction(st, idx, dd, probe_steps, sp)) continue;
                                    const ApplyResult er = st.eliminate(idx, rm_bit);
                                    if (er == ApplyResult::Contradiction) {
                                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                        return er;
                                    }
                                    if (er == ApplyResult::Progress) {
                                        ++s.hit_count;
                                        r.used_pattern_overlay_method = true;
                                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                        return ApplyResult::Progress;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_pom_global_digit_quad_family_exact(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (!pom_quad_global_family_allowed(st)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    const int house_budget = std::clamp(4 + n / 8, 5, 6);
    const int branch_steps = std::clamp(6 + n / 4, 8, 14);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    const int probe_budget_max = std::clamp(5 + n / 3, 8, 14);
    int houses[ExactPatternScratchpad::MAX_HOUSES]{};
    int counts[ExactPatternScratchpad::MAX_HOUSES]{};
    int cells1[8]{}, cells2[8]{}, cells3[8]{}, cells4[8]{};
    auto& sp = shared::exact_pattern_scratchpad();
    auto* root_cands = sp.p8_cands_backup[0];
    auto* root_values = sp.p8_values_backup[0];
    auto* root_row_used = sp.p8_row_used_backup[0];
    auto* root_col_used = sp.p8_col_used_backup[0];
    auto* root_box_used = sp.p8_box_used_backup[0];
    auto* level1_cands = sp.p8_cands_backup[1];
    auto* level1_values = sp.p8_values_backup[1];
    auto* level1_row_used = sp.p8_row_used_backup[1];
    auto* level1_col_used = sp.p8_col_used_backup[1];
    auto* level1_box_used = sp.p8_box_used_backup[1];
    auto* level2_cands = sp.p8_cands_backup[2];
    auto* level2_values = sp.p8_values_backup[2];
    auto* level2_row_used = sp.p8_row_used_backup[2];
    auto* level2_col_used = sp.p8_col_used_backup[2];
    auto* level2_box_used = sp.p8_box_used_backup[2];
    auto* level3_cands = sp.p8_cands_backup[3];
    auto* level3_values = sp.p8_values_backup[3];
    auto* level3_row_used = sp.p8_row_used_backup[3];
    auto* level3_col_used = sp.p8_col_used_backup[3];
    auto* level3_box_used = sp.p8_box_used_backup[3];

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int hc = 0;
        for (int h = 0; h < house_count && hc < ExactPatternScratchpad::MAX_HOUSES; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                ++cnt;
                if (cnt > 4) break;
            }
            if (cnt >= 2 && cnt <= 4) {
                houses[hc] = h;
                counts[hc] = cnt;
                ++hc;
            }
        }
        if (hc < 4) continue;
        pom_sort_house_candidates(houses, counts, hc);
        const int limit = std::min(hc, house_budget);

        for (int h1i = 0; h1i < limit; ++h1i) {
            for (int h2i = h1i + 1; h2i < limit; ++h2i) {
                for (int h3i = h2i + 1; h3i < limit; ++h3i) {
                    for (int h4i = h3i + 1; h4i < limit; ++h4i) {
                        const int h1 = houses[h1i];
                        const int h2 = houses[h2i];
                        const int h3 = houses[h3i];
                        const int h4 = houses[h4i];
                        int c1 = 0, c2 = 0, c3 = 0, c4 = 0;
                        const int p10 = st.topo->house_offsets[static_cast<size_t>(h1)];
                        const int p11 = st.topo->house_offsets[static_cast<size_t>(h1 + 1)];
                        const int p20 = st.topo->house_offsets[static_cast<size_t>(h2)];
                        const int p21 = st.topo->house_offsets[static_cast<size_t>(h2 + 1)];
                        const int p30 = st.topo->house_offsets[static_cast<size_t>(h3)];
                        const int p31 = st.topo->house_offsets[static_cast<size_t>(h3 + 1)];
                        const int p40 = st.topo->house_offsets[static_cast<size_t>(h4)];
                        const int p41 = st.topo->house_offsets[static_cast<size_t>(h4 + 1)];
                        for (int p = p10; p < p11 && c1 < 8; ++p) {
                            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells1[c1++] = idx;
                        }
                        for (int p = p20; p < p21 && c2 < 8; ++p) {
                            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells2[c2++] = idx;
                        }
                        for (int p = p30; p < p31 && c3 < 8; ++p) {
                            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells3[c3++] = idx;
                        }
                        for (int p = p40; p < p41 && c4 < 8; ++p) {
                            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells4[c4++] = idx;
                        }
                        if (c1 < 2 || c2 < 2 || c3 < 2 || c4 < 2) continue;
                        if (c1 > 4 || c2 > 4 || c3 > 4 || c4 > 4) continue;

                        {
                            const int hs[4] = {h1, h2, h3, h4};
                            sp.reset_pom();
                            const PomUndoMarker root_marker = pom_begin_frame(sp);
                            auto* inter_cands_fast = shared::intersection_slot(sp, 5);
                            pom_reset_intersection(inter_cands_fast, nn);
                            int valid_paths_fast = 0;
                            pom_capture_digit_paths_recursive(
                                st, d, bit, hs, 4, 0, branch_steps, inter_cands_fast, valid_paths_fast, sp);
                            pom_rollback_to(st, sp, root_marker);

                            const ApplyResult overlay_er = pom_apply_overlay_probes(
                                st, s, r, t0, inter_cands_fast, valid_paths_fast, probe_steps, probe_budget_max, sp);
                            if (overlay_er != ApplyResult::NoProgress) return overlay_er;
                            continue;
                        }

                        std::copy_n(st.cands, nn, root_cands);
                        std::copy_n(st.board->values.data(), nn, root_values);
                        std::copy_n(st.board->row_used.data(), n, root_row_used);
                        std::copy_n(st.board->col_used.data(), n, root_col_used);
                        std::copy_n(st.board->box_used.data(), n, root_box_used);
                        const int root_empty = st.board->empty_cells;

                        auto* inter_cands = shared::intersection_slot(sp, 6);
                        for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
                        int valid_paths = 0;

                        for (int i = 0; i < c1; ++i) {
                            std::copy_n(root_cands, nn, st.cands);
                            std::copy_n(root_values, nn, st.board->values.data());
                            std::copy_n(root_row_used, n, st.board->row_used.data());
                            std::copy_n(root_col_used, n, st.board->col_used.data());
                            std::copy_n(root_box_used, n, st.board->box_used.data());
                            st.board->empty_cells = root_empty;
                            if (!st.place(cells1[i], d) || !pom_propagate_singles(st, branch_steps)) continue;

                            std::copy_n(st.cands, nn, level1_cands);
                            std::copy_n(st.board->values.data(), nn, level1_values);
                            std::copy_n(st.board->row_used.data(), n, level1_row_used);
                            std::copy_n(st.board->col_used.data(), n, level1_col_used);
                            std::copy_n(st.board->box_used.data(), n, level1_box_used);
                            const int level1_empty = st.board->empty_cells;

                            int branch2_count = 0;
                            int branch2_cells[8]{};
                            for (int p = p20; p < p21 && branch2_count < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch2_cells[branch2_count++] = idx;
                            }
                            if (branch2_count < 1 || branch2_count > 3) continue;

                            for (int j = 0; j < branch2_count; ++j) {
                                std::copy_n(level1_cands, nn, st.cands);
                                std::copy_n(level1_values, nn, st.board->values.data());
                                std::copy_n(level1_row_used, n, st.board->row_used.data());
                                std::copy_n(level1_col_used, n, st.board->col_used.data());
                                std::copy_n(level1_box_used, n, st.board->box_used.data());
                                st.board->empty_cells = level1_empty;
                                if (!st.place(branch2_cells[j], d) || !pom_propagate_singles(st, branch_steps)) continue;

                                std::copy_n(st.cands, nn, level2_cands);
                                std::copy_n(st.board->values.data(), nn, level2_values);
                                std::copy_n(st.board->row_used.data(), n, level2_row_used);
                                std::copy_n(st.board->col_used.data(), n, level2_col_used);
                                std::copy_n(st.board->box_used.data(), n, level2_box_used);
                                const int level2_empty = st.board->empty_cells;

                                int branch3_count = 0;
                                int branch3_cells[8]{};
                                for (int p = p30; p < p31 && branch3_count < 8; ++p) {
                                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                    if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch3_cells[branch3_count++] = idx;
                                }
                                if (branch3_count < 1 || branch3_count > 3) continue;

                                for (int k = 0; k < branch3_count; ++k) {
                                    std::copy_n(level2_cands, nn, st.cands);
                                    std::copy_n(level2_values, nn, st.board->values.data());
                                    std::copy_n(level2_row_used, n, st.board->row_used.data());
                                    std::copy_n(level2_col_used, n, st.board->col_used.data());
                                    std::copy_n(level2_box_used, n, st.board->box_used.data());
                                    st.board->empty_cells = level2_empty;
                                    if (!st.place(branch3_cells[k], d) || !pom_propagate_singles(st, branch_steps)) continue;

                                    std::copy_n(st.cands, nn, level3_cands);
                                    std::copy_n(st.board->values.data(), nn, level3_values);
                                    std::copy_n(st.board->row_used.data(), n, level3_row_used);
                                    std::copy_n(st.board->col_used.data(), n, level3_col_used);
                                    std::copy_n(st.board->box_used.data(), n, level3_box_used);
                                    const int level3_empty = st.board->empty_cells;

                                    int branch4_count = 0;
                                    int branch4_cells[8]{};
                                    for (int p = p40; p < p41 && branch4_count < 8; ++p) {
                                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                        if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch4_cells[branch4_count++] = idx;
                                    }
                                    if (branch4_count < 1 || branch4_count > 3) continue;

                                    for (int m = 0; m < branch4_count; ++m) {
                                        std::copy_n(level3_cands, nn, st.cands);
                                        std::copy_n(level3_values, nn, st.board->values.data());
                                        std::copy_n(level3_row_used, n, st.board->row_used.data());
                                        std::copy_n(level3_col_used, n, st.board->col_used.data());
                                        std::copy_n(level3_box_used, n, st.board->box_used.data());
                                        st.board->empty_cells = level3_empty;
                                        if (!st.place(branch4_cells[m], d) || !pom_propagate_singles(st, branch_steps)) continue;
                                        ++valid_paths;
                                        pom_capture_intersection(st, inter_cands);
                                    }
                                }
                            }
                        }

                        std::copy_n(root_cands, nn, st.cands);
                        std::copy_n(root_values, nn, st.board->values.data());
                        std::copy_n(root_row_used, n, st.board->row_used.data());
                        std::copy_n(root_col_used, n, st.board->col_used.data());
                        std::copy_n(root_box_used, n, st.board->box_used.data());
                        st.board->empty_cells = root_empty;

                        if (valid_paths < 2) continue;

                        const ApplyResult house_er = pom_apply_house_overlay_elims(st, inter_cands);
                        if (house_er == ApplyResult::Contradiction) {
                            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                            return house_er;
                        }
                        if (house_er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_pattern_overlay_method = true;
                            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                            return ApplyResult::Progress;
                        }

                        int probe_budget = probe_budget_max;
                        for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
                            if (st.board->values[idx] != 0) continue;
                            const uint64_t base = st.cands[idx];
                            const uint64_t keep = inter_cands[idx] & base;
                            if (keep == 0ULL) continue;
                            uint64_t rm = base & ~keep;
                            while (rm != 0ULL && probe_budget > 0) {
                                const uint64_t rm_bit = config::bit_lsb(rm);
                                rm = config::bit_clear_lsb_u64(rm);
                                --probe_budget;
                                const int dd = config::bit_ctz_u64(rm_bit) + 1;
                                if (!pom_probe_candidate_contradiction(st, idx, dd, probe_steps, sp)) continue;
                                const ApplyResult er = st.eliminate(idx, rm_bit);
                                if (er == ApplyResult::Contradiction) {
                                    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                    return er;
                                }
                                if (er == ApplyResult::Progress) {
                                    ++s.hit_count;
                                    r.used_pattern_overlay_method = true;
                                    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                    return ApplyResult::Progress;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_pom_global_digit_family_exact(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (!pom_global_family_allowed(st)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    const int house_budget = std::clamp(4 + n / 8, 5, 7);
    const int branch_steps = std::clamp(6 + n / 4, 8, 14);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    const int probe_budget_max = std::clamp(5 + n / 3, 8, 16);
    int houses[ExactPatternScratchpad::MAX_HOUSES]{};
    int counts[ExactPatternScratchpad::MAX_HOUSES]{};
    int cells1[8]{}, cells2[8]{}, cells3[8]{};
    auto& sp = shared::exact_pattern_scratchpad();
    auto* root_cands = sp.p8_cands_backup[0];
    auto* root_values = sp.p8_values_backup[0];
    auto* root_row_used = sp.p8_row_used_backup[0];
    auto* root_col_used = sp.p8_col_used_backup[0];
    auto* root_box_used = sp.p8_box_used_backup[0];
    auto* level1_cands = sp.p8_cands_backup[1];
    auto* level1_values = sp.p8_values_backup[1];
    auto* level1_row_used = sp.p8_row_used_backup[1];
    auto* level1_col_used = sp.p8_col_used_backup[1];
    auto* level1_box_used = sp.p8_box_used_backup[1];
    auto* level2_cands = sp.p8_cands_backup[2];
    auto* level2_values = sp.p8_values_backup[2];
    auto* level2_row_used = sp.p8_row_used_backup[2];
    auto* level2_col_used = sp.p8_col_used_backup[2];
    auto* level2_box_used = sp.p8_box_used_backup[2];

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int hc = 0;
        for (int h = 0; h < house_count && hc < ExactPatternScratchpad::MAX_HOUSES; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                ++cnt;
                if (cnt > 4) break;
            }
            if (cnt >= 2 && cnt <= 4) {
                houses[hc] = h;
                counts[hc] = cnt;
                ++hc;
            }
        }
        if (hc < 3) continue;
        pom_sort_house_candidates(houses, counts, hc);
        const int limit = std::min(hc, house_budget);

        for (int h1i = 0; h1i < limit; ++h1i) {
            for (int h2i = h1i + 1; h2i < limit; ++h2i) {
                for (int h3i = h2i + 1; h3i < limit; ++h3i) {
                    const int h1 = houses[h1i];
                    const int h2 = houses[h2i];
                    const int h3 = houses[h3i];
                    int c1 = 0, c2 = 0, c3 = 0;
                    const int p10 = st.topo->house_offsets[static_cast<size_t>(h1)];
                    const int p11 = st.topo->house_offsets[static_cast<size_t>(h1 + 1)];
                    const int p20 = st.topo->house_offsets[static_cast<size_t>(h2)];
                    const int p21 = st.topo->house_offsets[static_cast<size_t>(h2 + 1)];
                    const int p30 = st.topo->house_offsets[static_cast<size_t>(h3)];
                    const int p31 = st.topo->house_offsets[static_cast<size_t>(h3 + 1)];
                    for (int p = p10; p < p11 && c1 < 8; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells1[c1++] = idx;
                    }
                    for (int p = p20; p < p21 && c2 < 8; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells2[c2++] = idx;
                    }
                    for (int p = p30; p < p31 && c3 < 8; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) cells3[c3++] = idx;
                    }
                    if (c1 < 2 || c2 < 2 || c3 < 2 || c1 > 4 || c2 > 4 || c3 > 4) continue;

                    {
                        const int hs[3] = {h1, h2, h3};
                        sp.reset_pom();
                        const PomUndoMarker root_marker = pom_begin_frame(sp);
                        auto* inter_cands_fast = shared::intersection_slot(sp, 5);
                        pom_reset_intersection(inter_cands_fast, nn);
                        int valid_paths_fast = 0;
                        pom_capture_digit_paths_recursive(
                            st, d, bit, hs, 3, 0, branch_steps, inter_cands_fast, valid_paths_fast, sp);
                        pom_rollback_to(st, sp, root_marker);

                        const ApplyResult overlay_er = pom_apply_overlay_probes(
                            st, s, r, t0, inter_cands_fast, valid_paths_fast, probe_steps, probe_budget_max, sp);
                        if (overlay_er != ApplyResult::NoProgress) return overlay_er;
                        continue;
                    }

                    std::copy_n(st.cands, nn, root_cands);
                    std::copy_n(st.board->values.data(), nn, root_values);
                    std::copy_n(st.board->row_used.data(), n, root_row_used);
                    std::copy_n(st.board->col_used.data(), n, root_col_used);
                    std::copy_n(st.board->box_used.data(), n, root_box_used);
                    const int root_empty = st.board->empty_cells;

                    auto* inter_cands = shared::intersection_slot(sp, 6);
                    for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
                    int valid_paths = 0;

                    for (int i = 0; i < c1; ++i) {
                        std::copy_n(root_cands, nn, st.cands);
                        std::copy_n(root_values, nn, st.board->values.data());
                        std::copy_n(root_row_used, n, st.board->row_used.data());
                        std::copy_n(root_col_used, n, st.board->col_used.data());
                        std::copy_n(root_box_used, n, st.board->box_used.data());
                        st.board->empty_cells = root_empty;
                        if (!st.place(cells1[i], d) || !pom_propagate_singles(st, branch_steps)) continue;

                        std::copy_n(st.cands, nn, level1_cands);
                        std::copy_n(st.board->values.data(), nn, level1_values);
                        std::copy_n(st.board->row_used.data(), n, level1_row_used);
                        std::copy_n(st.board->col_used.data(), n, level1_col_used);
                        std::copy_n(st.board->box_used.data(), n, level1_box_used);
                        const int level1_empty = st.board->empty_cells;

                        int branch2_count = 0;
                        int branch2_cells[8]{};
                        for (int p = p20; p < p21 && branch2_count < 8; ++p) {
                            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch2_cells[branch2_count++] = idx;
                        }
                        if (branch2_count < 1 || branch2_count > 3) continue;

                        for (int j = 0; j < branch2_count; ++j) {
                            std::copy_n(level1_cands, nn, st.cands);
                            std::copy_n(level1_values, nn, st.board->values.data());
                            std::copy_n(level1_row_used, n, st.board->row_used.data());
                            std::copy_n(level1_col_used, n, st.board->col_used.data());
                            std::copy_n(level1_box_used, n, st.board->box_used.data());
                            st.board->empty_cells = level1_empty;
                            if (!st.place(branch2_cells[j], d) || !pom_propagate_singles(st, branch_steps)) continue;

                            std::copy_n(st.cands, nn, level2_cands);
                            std::copy_n(st.board->values.data(), nn, level2_values);
                            std::copy_n(st.board->row_used.data(), n, level2_row_used);
                            std::copy_n(st.board->col_used.data(), n, level2_col_used);
                            std::copy_n(st.board->box_used.data(), n, level2_box_used);
                            const int level2_empty = st.board->empty_cells;

                            int branch3_count = 0;
                            int branch3_cells[8]{};
                            for (int p = p30; p < p31 && branch3_count < 8; ++p) {
                                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) branch3_cells[branch3_count++] = idx;
                            }
                            if (branch3_count < 1 || branch3_count > 3) continue;

                            for (int k = 0; k < branch3_count; ++k) {
                                std::copy_n(level2_cands, nn, st.cands);
                                std::copy_n(level2_values, nn, st.board->values.data());
                                std::copy_n(level2_row_used, n, st.board->row_used.data());
                                std::copy_n(level2_col_used, n, st.board->col_used.data());
                                std::copy_n(level2_box_used, n, st.board->box_used.data());
                                st.board->empty_cells = level2_empty;
                                if (!st.place(branch3_cells[k], d) || !pom_propagate_singles(st, branch_steps)) continue;
                                ++valid_paths;
                                pom_capture_intersection(st, inter_cands);
                            }
                        }
                    }

                    std::copy_n(root_cands, nn, st.cands);
                    std::copy_n(root_values, nn, st.board->values.data());
                    std::copy_n(root_row_used, n, st.board->row_used.data());
                    std::copy_n(root_col_used, n, st.board->col_used.data());
                    std::copy_n(root_box_used, n, st.board->box_used.data());
                    st.board->empty_cells = root_empty;

                    if (valid_paths < 2) continue;

                    const ApplyResult house_er = pom_apply_house_overlay_elims(st, inter_cands);
                    if (house_er == ApplyResult::Contradiction) {
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return house_er;
                    }
                    if (house_er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_pattern_overlay_method = true;
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return ApplyResult::Progress;
                    }

                    int probe_budget = probe_budget_max;
                    for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
                        if (st.board->values[idx] != 0) continue;
                        const uint64_t base = st.cands[idx];
                        const uint64_t keep = inter_cands[idx] & base;
                        if (keep == 0ULL) continue;
                        uint64_t rm = base & ~keep;
                        while (rm != 0ULL && probe_budget > 0) {
                            const uint64_t rm_bit = config::bit_lsb(rm);
                            rm = config::bit_clear_lsb_u64(rm);
                            --probe_budget;
                            const int dd = config::bit_ctz_u64(rm_bit) + 1;
                            if (!pom_probe_candidate_contradiction(st, idx, dd, probe_steps, sp)) continue;
                            const ApplyResult er = st.eliminate(idx, rm_bit);
                            if (er == ApplyResult::Contradiction) {
                                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                return er;
                            }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_pattern_overlay_method = true;
                                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                return ApplyResult::Progress;
                            }
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_pom_digit_pair_family_exact(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    const int house_budget = std::clamp(3 + n / 8, 4, 8);
    const int branch_steps = std::clamp(6 + n / 4, 8, 14);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    const int probe_budget_max = std::clamp(4 + n / 3, 6, 14);
    int houses[ExactPatternScratchpad::MAX_HOUSES]{};
    int counts[ExactPatternScratchpad::MAX_HOUSES]{};
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int hc = 0;
        for (int h = 0; h < house_count && hc < ExactPatternScratchpad::MAX_HOUSES; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                ++cnt;
                if (cnt > 4) break;
            }
            if (cnt >= 2 && cnt <= 4) {
                houses[hc] = h;
                counts[hc] = cnt;
                ++hc;
            }
        }
        if (hc < 2) continue;
        pom_sort_house_candidates(houses, counts, hc);
        const int limit = std::min(hc, house_budget);

        for (int hi = 0; hi < limit; ++hi) {
            for (int hj = hi + 1; hj < limit; ++hj) {
                const int h1 = houses[hi];
                const int h2 = houses[hj];
                const int hs[2] = {h1, h2};
                bool ok = true;
                for (int hk = 0; hk < 2; ++hk) {
                    int tmp_cells[8]{};
                    const int cc = pom_collect_house_digit_cells(st, hs[hk], bit, tmp_cells, 8);
                    if (cc < 2 || cc > 4) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) continue;

                sp.reset_pom();
                const PomUndoMarker root_marker = pom_begin_frame(sp);
                uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
                pom_reset_intersection(inter_cands, nn);
                int valid_pairs = 0;
                pom_capture_digit_paths_recursive(
                    st, d, bit, hs, 2, 0, branch_steps, inter_cands, valid_pairs, sp);
                pom_rollback_to(st, sp, root_marker);

                const ApplyResult overlay_er = pom_apply_overlay_probes(
                    st, s, r, t0, inter_cands, valid_pairs, probe_steps, probe_budget_max, sp);
                if (overlay_er != ApplyResult::NoProgress) return overlay_er;
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_pom_digit_family_exact(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    auto& sp = shared::exact_pattern_scratchpad();
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    const int house_budget = std::clamp(3 + n / 6, 4, 12);
    const int branch_steps = std::clamp(6 + n / 4, 8, 14);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    const int probe_budget_max = std::clamp(4 + n / 3, 6, 14);
    int houses[ExactPatternScratchpad::MAX_HOUSES]{};
    int counts[ExactPatternScratchpad::MAX_HOUSES]{};
    int house_cells[64]{};

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int hc = 0;
        for (int h = 0; h < house_count && hc < ExactPatternScratchpad::MAX_HOUSES; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                ++cnt;
                if (cnt > 4) break;
            }
            if (cnt >= 2 && cnt <= 4) {
                houses[hc] = h;
                counts[hc] = cnt;
                ++hc;
            }
        }
        if (hc == 0) continue;
        pom_sort_house_candidates(houses, counts, hc);

        const int limit = std::min(hc, house_budget);
        for (int hi = 0; hi < limit; ++hi) {
            const int h = houses[hi];
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cc = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                if (cc < 64) house_cells[cc++] = idx;
            }
            if (cc < 2) continue;

            shared::snapshot_state(st, sp);
            uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
            for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
            uint64_t contradiction_mask = 0ULL;
            int valid_hyp = 0;

            for (int ci = 0; ci < cc; ++ci) {
                const int cell = house_cells[ci];
                shared::restore_state(st, sp);

                bool contradiction = false;
                if (!st.place(cell, d)) {
                    contradiction = true;
                } else if (!pom_propagate_singles(st, branch_steps)) {
                    contradiction = true;
                }

                if (contradiction) {
                    contradiction_mask |= (1ULL << ci);
                    continue;
                }

                ++valid_hyp;
                pom_capture_intersection(st, inter_cands);
            }

            shared::restore_state(st, sp);

            if (contradiction_mask != 0ULL) {
                for (int ci = 0; ci < cc; ++ci) {
                    if ((contradiction_mask & (1ULL << ci)) == 0ULL) continue;
                    const ApplyResult er = st.eliminate(house_cells[ci], bit);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return er;
                    }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_pattern_overlay_method = true;
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return ApplyResult::Progress;
                    }
                }
            }

            if (valid_hyp >= 2) {
                const ApplyResult house_er = pom_apply_house_overlay_elims(st, inter_cands);
                if (house_er == ApplyResult::Contradiction) {
                    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                    return house_er;
                }
                if (house_er == ApplyResult::Progress) {
                    ++s.hit_count;
                    r.used_pattern_overlay_method = true;
                    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                    return ApplyResult::Progress;
                }

                int probe_budget = probe_budget_max;
                for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
                    if (st.board->values[idx] != 0) continue;
                    const uint64_t base = st.cands[idx];
                    const uint64_t keep = inter_cands[idx] & base;
                    if (keep == 0ULL) continue;
                    uint64_t rm = base & ~keep;
                    while (rm != 0ULL && probe_budget > 0) {
                        const uint64_t rm_bit = config::bit_lsb(rm);
                        rm = config::bit_clear_lsb_u64(rm);
                        --probe_budget;
                        const int dd = config::bit_ctz_u64(rm_bit) + 1;
                        if (!pom_probe_candidate_contradiction(st, idx, dd, probe_steps, sp)) continue;
                        const ApplyResult er = st.eliminate(idx, rm_bit);
                        if (er == ApplyResult::Contradiction) {
                            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                            return er;
                        }
                        if (er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_pattern_overlay_method = true;
                            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                            return ApplyResult::Progress;
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// WĹ‚aĹ›ciwy silnik sprawdzania nakĹ‚adek (Bounded Pattern Overlay Method)
// Operuje na pĹ‚ytkim drzewie DFS dla wybranej, najbardziej ograniczonej komĂłrki.
// ============================================================================
inline ApplyResult apply_pom_exact(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    auto& sp = shared::exact_pattern_scratchpad();
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int pivot_budget = std::clamp(4 + n / 3, 6, 12);
    int pivots[64]{};
    int pivot_cnt = 0;
    for (int pc = 2; pc <= 4; ++pc) {
        for (int idx = 0; idx < nn; ++idx) {
            if (pivot_cnt >= pivot_budget) break;
            if (st.board->values[idx] != 0) continue;
            if (std::popcount(st.cands[idx]) != pc) continue;
            pivots[pivot_cnt++] = idx;
        }
        if (pivot_cnt >= pivot_budget) break;
    }
    if (pivot_cnt == 0) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int hyp_steps = std::clamp(6 + n / 4, 8, 12);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    for (int pi = 0; pi < pivot_cnt; ++pi) {
        const int pivot = pivots[pi];
        const uint64_t pm = st.cands[pivot];
        if (pm == 0ULL) continue;

        shared::snapshot_state(st, sp);

        uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
        for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
        uint64_t contradiction_mask = 0ULL;
        int valid_hyp = 0;

        int digit_budget = 0;
        for (uint64_t w = pm; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
            if (digit_budget >= 2) break;
            ++digit_budget;
            const uint64_t bit = config::bit_lsb(w);
            const int d = config::bit_ctz_u64(bit) + 1;

            shared::restore_state(st, sp);

            bool contradiction = false;
            if (!st.place(pivot, d)) {
                contradiction = true;
            } else if (!pom_propagate_singles(st, hyp_steps)) {
                contradiction = true;
            }

            if (contradiction) {
                contradiction_mask |= bit;
                continue;
            }

            ++valid_hyp;
            for (int i = 0; i < nn; ++i) {
                uint64_t cur = st.cands[i];
                if (st.board->values[i] != 0) {
                    cur = (1ULL << (st.board->values[i] - 1));
                }
                inter_cands[i] &= cur;
            }
        }

        shared::restore_state(st, sp);

        if (contradiction_mask != 0ULL) {
            const ApplyResult er = st.eliminate(pivot, contradiction_mask);
            if (er == ApplyResult::Contradiction) {
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return er;
            }
            if (er == ApplyResult::Progress) {
                ++s.hit_count;
                r.used_pattern_overlay_method = true;
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return ApplyResult::Progress;
            }
        }

        // Intersection-driven candidates are additionally validated by
        // contradiction probing before elimination.
        if (valid_hyp >= 2) {
            const ApplyResult house_er = pom_apply_house_overlay_elims(st, inter_cands);
            if (house_er == ApplyResult::Contradiction) {
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return house_er;
            }
            if (house_er == ApplyResult::Progress) {
                ++s.hit_count;
                r.used_pattern_overlay_method = true;
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return ApplyResult::Progress;
            }

            int probe_budget = std::clamp(4 + n / 3, 6, 12);
            for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
                if (st.board->values[idx] != 0) continue;
                const uint64_t base = st.cands[idx];
                const uint64_t keep = inter_cands[idx] & base;
                if (keep == 0ULL) continue;
                uint64_t rm = base & ~keep;
                while (rm != 0ULL && probe_budget > 0) {
                    const uint64_t bit = config::bit_lsb(rm);
                    rm = config::bit_clear_lsb_u64(rm);
                    --probe_budget;
                    const int d = config::bit_ctz_u64(bit) + 1;
                    if (!pom_probe_candidate_contradiction(st, idx, d, probe_steps, sp)) continue;
                    const ApplyResult er = st.eliminate(idx, bit);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return er;
                    }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_pattern_overlay_method = true;
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return ApplyResult::Progress;
                    }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// GĹ‚Ăłwny kontroler Ĺ‚Ä…czÄ…cy nakĹ‚adanie masek z kompozycjÄ… MSLS i Ryb (P8)
// ============================================================================
inline ApplyResult apply_pattern_overlay_method(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (p8_board_too_dense(RequiredStrategy::PatternOverlayMethod, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    const bool exact_only_required =
        ::sudoku_hpc::logic::shared::required_exact_strategy_active(RequiredStrategy::PatternOverlayMethod);
    
    // Krok 1: Global digit overlay family.
    if (!exact_only_required) {
        const ApplyResult hexa_global_family_exact = apply_pom_global_digit_hexa_family_exact(st, tmp, r);
        if (hexa_global_family_exact == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return hexa_global_family_exact;
        }
        if (hexa_global_family_exact == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ApplyResult::Progress;
        }

        const ApplyResult penta_global_family_exact = apply_pom_global_digit_penta_family_exact(st, tmp, r);
        if (penta_global_family_exact == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return penta_global_family_exact;
        }
        if (penta_global_family_exact == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ApplyResult::Progress;
        }

        const ApplyResult quad_global_family_exact = apply_pom_global_digit_quad_family_exact(st, tmp, r);
        if (quad_global_family_exact == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return quad_global_family_exact;
        }
        if (quad_global_family_exact == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ApplyResult::Progress;
        }

        const ApplyResult global_family_exact = apply_pom_global_digit_family_exact(st, tmp, r);
        if (global_family_exact == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return global_family_exact;
        }
        if (global_family_exact == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ApplyResult::Progress;
        }

        const ApplyResult pair_family_exact = apply_pom_digit_pair_family_exact(st, tmp, r);
        if (pair_family_exact == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return pair_family_exact;
        }
        if (pair_family_exact == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ApplyResult::Progress;
        }

        const ApplyResult family_exact = apply_pom_digit_family_exact(st, tmp, r);
        if (family_exact == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return family_exact;
        }
        if (family_exact == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ApplyResult::Progress;
        }
    }

    // Krok 2: Aplikacja czystego POM (szybki overlay)
    const ApplyResult exact = apply_pom_exact(st, tmp, r);
    if (exact == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return exact; 
    }
    if (exact == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_pattern_overlay_method = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }
    if (exact_only_required) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    // Krok 3: Adaptacyjne dynamiczne Ĺ›ledzenie zatorĂłw za pomocÄ… Ĺ‚aĹ„cuchĂłw (GĹ‚Ä™bokoĹ›Ä‡ P8)
    const int depth_cap = std::clamp(8 + (st.board->empty_cells / std::max(1, st.topo->n)), 10, 14);
    bool used_dynamic = false;

    ApplyResult dyn = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used_dynamic);
    if (dyn == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn; 
    }
    if (dyn == ApplyResult::Progress && used_dynamic) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return dyn;
    }

    // Krok 4: Kaskada z P8 - MSLS generuje wielkie nakĹ‚adki matryc
    ApplyResult ar = apply_msls(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar; 
    }
    if (ar == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical

