// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Modul: chains_basic.h (Poziom 6 - Diabolical)
// Opis: Algorytmy oparte na lancuchach logicznych: X-Chain i XY-Chain.
//       Wszystkie analizy dzialaja na splaszczonych drzewach (BFS) w
//       buforze exact_pattern_scratchpad by uniknac memory overhead.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <cstdint>

#include "../../config/bit_utils.h"
#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../p7_nightmare/aic_grouped_aic.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"

namespace sudoku_hpc::logic::p6_diabolical {

// ============================================================================
// X-Chain (Single Digit Alternating Inference Chain)
// Dla tighteningu wykorzystujemy ten sam alternujacy silnik co w AIC, ale z
// konserwatywnym strong-start only i mniejszym limitem glebokosci.
// ============================================================================
inline ApplyResult apply_x_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    bool used = false;
    const int depth_cap = std::clamp(8 + st.topo->n / 2, 8, 18);
    const ApplyResult ar = logic::p7_nightmare::alternating_chain_core(st, depth_cap, false, used);
    if (ar == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_x_chain = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ar;
}

// ============================================================================
// XY-Chain
// Lancuch oparty o powiazania wezlow typu bivalue (2 kandydatow).
// Szukamy sciezki: xy -> yz -> zw -> wx i eliminujemy wspolna cyfre z
// przeciecia peerow obu koncow.
// ============================================================================
inline ApplyResult apply_xy_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int max_depth = std::clamp(6 + st.topo->n / 2, 8, 16);

    auto& sp = shared::exact_pattern_scratchpad();
    int bivalue_count = 0;

    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 2) {
            sp.als_cells[bivalue_count++] = idx;
        }
    }

    if (bivalue_count < 3) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    auto path_contains_cell = [&](int node_idx, int cell) -> bool {
        int cur = node_idx;
        while (cur >= 0) {
            if (sp.chain_cell[cur] == cell) return true;
            cur = sp.chain_parent[cur];
        }
        return false;
    };

    bool any_progress = false;

    for (int bi = 0; bi < bivalue_count; ++bi) {
        const int start = sp.als_cells[bi];
        const uint64_t start_mask = st.cands[start];

        uint64_t wz = start_mask;
        while (wz != 0ULL) {
            const uint64_t zbit = config::bit_lsb(wz);
            wz = config::bit_clear_lsb_u64(wz);

            if ((start_mask ^ zbit) == 0ULL) continue;

            sp.chain_count = 1;
            sp.chain_cell[0] = start;
            sp.chain_enter_bit[0] = zbit;
            sp.chain_parent[0] = -1;
            sp.chain_depth[0] = 0;

            for (int ni = 0; ni < sp.chain_count; ++ni) {
                const int cur_cell = sp.chain_cell[ni];
                const uint64_t cur_enter = sp.chain_enter_bit[ni];
                const uint64_t cur_mask = st.cands[cur_cell];

                if (std::popcount(cur_mask) != 2 || (cur_mask & cur_enter) == 0ULL) continue;

                const uint64_t exit_bit = cur_mask ^ cur_enter;
                if (exit_bit == 0ULL) continue;
                if (sp.chain_depth[ni] >= max_depth) continue;

                const int p0 = st.topo->peer_offsets[cur_cell];
                const int p1 = st.topo->peer_offsets[cur_cell + 1];
                for (int p = p0; p < p1; ++p) {
                    const int nxt = st.topo->peers_flat[p];
                    if (st.board->values[nxt] != 0) continue;

                    const uint64_t nxt_mask = st.cands[nxt];
                    if (std::popcount(nxt_mask) != 2) continue;
                    if ((nxt_mask & exit_bit) == 0ULL) continue;
                    if (path_contains_cell(ni, nxt)) continue;

                    const uint64_t nxt_other = nxt_mask ^ exit_bit;
                    if (nxt_other == 0ULL) continue;

                    const int next_depth = static_cast<int>(sp.chain_depth[ni]) + 1;
                    if (next_depth >= 2 && nxt_other == zbit) {
                        const int tp0 = st.topo->peer_offsets[start];
                        const int tp1 = st.topo->peer_offsets[start + 1];
                        for (int tp = tp0; tp < tp1; ++tp) {
                            const int t = st.topo->peers_flat[tp];
                            if (t == start || t == nxt) continue;
                            if (st.board->values[t] != 0) continue;
                            if ((st.cands[t] & zbit) == 0ULL) continue;
                            if (!st.is_peer(t, nxt)) continue;

                            const ApplyResult er = st.eliminate(t, zbit);
                            if (er == ApplyResult::Contradiction) {
                                s.elapsed_ns += st.now_ns() - t0;
                                return er;
                            }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_xy_chain = true;
                                any_progress = true;
                            }
                        }
                    }

                    if (sp.chain_count >= ExactPatternScratchpad::MAX_CHAIN) continue;
                    sp.chain_cell[sp.chain_count] = nxt;
                    sp.chain_enter_bit[sp.chain_count] = exit_bit;
                    sp.chain_parent[sp.chain_count] = ni;
                    sp.chain_depth[sp.chain_count] = static_cast<uint8_t>(next_depth);
                    ++sp.chain_count;
                }
            }
        }
    }

    if (any_progress) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
