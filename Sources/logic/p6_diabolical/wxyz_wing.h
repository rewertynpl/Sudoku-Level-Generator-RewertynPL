//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline int wxyz_wing_peer_cap(const CandidateState& st) {
    const int n = st.topo->n;
    if (n <= 9) return 16;
    if (n <= 16) return 14;
    if (n <= 25) return 12;
    return 10;
}

inline void wxyz_sort_candidates(
    const CandidateState& st,
    int* cells,
    int count) {
    for (int i = 1; i < count; ++i) {
        const int cell = cells[i];
        const int pop = std::popcount(st.cands[cell]);
        int j = i - 1;
        while (j >= 0) {
            const int prev = cells[j];
            const int prev_pop = std::popcount(st.cands[prev]);
            if (prev_pop < pop) break;
            if (prev_pop == pop && prev < cell) break;
            cells[j + 1] = prev;
            --j;
        }
        cells[j + 1] = cell;
    }
}

inline int wxyz_collect_holders(
    const CandidateState& st,
    const int* cells,
    int count,
    uint64_t bit,
    int* holders) {
    int hc = 0;
    for (int i = 0; i < count; ++i) {
        if ((st.cands[cells[i]] & bit) != 0ULL) {
            holders[hc++] = cells[i];
        }
    }
    return hc;
}

inline bool wxyz_holders_pairwise_peer(
    const CandidateState& st,
    const int* holders,
    int holder_count) {
    if (holder_count < 2) return false;
    for (int i = 0; i < holder_count; ++i) {
        for (int j = i + 1; j < holder_count; ++j) {
            if (!st.is_peer(holders[i], holders[j])) return false;
        }
    }
    return true;
}

inline ApplyResult apply_wxyz_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();
    int holders[4]{};
    int chosen[4]{};

    for (int pivot = 0; pivot < nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;
        const uint64_t mp = st.cands[pivot];
        const int pivot_pop = std::popcount(mp);
        if (pivot_pop < 2 || pivot_pop > 4) continue;

        sp.wing_count = 0;
        const int peer_cap = wxyz_wing_peer_cap(st);
        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        for (int p = p0; p < p1; ++p) {
            const int w = st.topo->peers_flat[p];
            if (st.board->values[w] != 0) continue;
            const uint64_t mw = st.cands[w];
            const int wing_pop = std::popcount(mw);
            if (wing_pop < 2 || wing_pop > 4) continue;
            if (sp.wing_count < peer_cap) {
                sp.wing_cells[sp.wing_count++] = w;
            }
        }
        if (sp.wing_count < 3) continue;
        wxyz_sort_candidates(st, sp.wing_cells, sp.wing_count);

        chosen[0] = pivot;
        for (int i = 0; i + 2 < sp.wing_count; ++i) {
            chosen[1] = sp.wing_cells[i];
            const uint64_t m1 = mp | st.cands[chosen[1]];
            if (std::popcount(m1) > 4) continue;

            for (int j = i + 1; j + 1 < sp.wing_count; ++j) {
                chosen[2] = sp.wing_cells[j];
                const uint64_t m2 = m1 | st.cands[chosen[2]];
                if (std::popcount(m2) > 4) continue;

                for (int k = j + 1; k < sp.wing_count; ++k) {
                    chosen[3] = sp.wing_cells[k];
                    const uint64_t union_mask = m2 | st.cands[chosen[3]];
                    if (std::popcount(union_mask) != 4) continue;

                    uint64_t zmask = union_mask;
                    while (zmask != 0ULL) {
                        const uint64_t z = config::bit_lsb(zmask);
                        zmask = config::bit_clear_lsb_u64(zmask);
                        const int holder_count = wxyz_collect_holders(st, chosen, 4, z, holders);
                        if (!wxyz_holders_pairwise_peer(st, holders, holder_count)) continue;

                        for (int t = 0; t < nn; ++t) {
                            if (t == chosen[0] || t == chosen[1] || t == chosen[2] || t == chosen[3]) continue;
                            if (st.board->values[t] != 0) continue;
                            if ((st.cands[t] & z) == 0ULL) continue;

                            bool sees_all = true;
                            for (int h = 0; h < holder_count; ++h) {
                                if (!st.is_peer(t, holders[h])) {
                                    sees_all = false;
                                    break;
                                }
                            }
                            if (!sees_all) continue;

                            const ApplyResult er = st.eliminate(t, z);
                            if (er == ApplyResult::Contradiction) {
                                s.elapsed_ns += st.now_ns() - t0;
                                return er;
                            }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_wxyz_wing = true;
                                progress = true;
                            }
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return progress ? ApplyResult::Progress : ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
