#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p4_hard {

inline ApplyResult apply_y_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    bool progress = false;

    for (int pivot = 0; pivot < st.topo->nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;
        const uint64_t mp = st.cands[pivot];
        if (std::popcount(mp) != 2) continue;

        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        for (int i = p0; i < p1; ++i) {
            const int a = st.topo->peers_flat[i];
            if (st.board->values[a] != 0) continue;
            const uint64_t ma = st.cands[a];
            if (std::popcount(ma) != 2) continue;

            const uint64_t shared_a = ma & mp;
            if (std::popcount(shared_a) != 1) continue;
            const uint64_t z = ma & ~mp;
            if (std::popcount(z) != 1) continue;

            for (int j = i + 1; j < p1; ++j) {
                const int b = st.topo->peers_flat[j];
                if (st.board->values[b] != 0) continue;
                const uint64_t mb = st.cands[b];
                if (std::popcount(mb) != 2) continue;

                const uint64_t shared_b = mb & mp;
                if (std::popcount(shared_b) != 1 || shared_b == shared_a) continue;
                const uint64_t z2 = mb & ~mp;
                if (z2 != z || std::popcount(z2) != 1) continue;

                const int ap0 = st.topo->peer_offsets[a];
                const int ap1 = st.topo->peer_offsets[a + 1];
                for (int p = ap0; p < ap1; ++p) {
                    const int t = st.topo->peers_flat[p];
                    if (t == pivot || t == a || t == b) continue;
                    if (!st.is_peer(t, b)) continue;

                    const ApplyResult er = st.eliminate(t, z);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return er;
                    }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_y_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_remote_pairs(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int* component = sp.cell_to_node;
    int* parity = sp.node_to_cell;
    int* node_degree = sp.node_degree;
    int* in_component = sp.adj_cursor;
    int* seen_parity0 = sp.visited;
    int* seen_parity1 = sp.bfs_depth;
    uint64_t* pair_masks = reinterpret_cast<uint64_t*>(sp.adj_flat);

    int pair_mask_count = 0;
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        const uint64_t m = st.cands[idx];
        if (std::popcount(m) == 2 && pair_mask_count < nn) {
            pair_masks[pair_mask_count++] = m;
        }
    }
    if (pair_mask_count == 0) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    std::sort(pair_masks, pair_masks + pair_mask_count);
    pair_mask_count = static_cast<int>(std::unique(pair_masks, pair_masks + pair_mask_count) - pair_masks);

    for (int p_idx = 0; p_idx < pair_mask_count; ++p_idx) {
        const uint64_t pair_mask = pair_masks[p_idx];
        std::fill_n(component, nn, -1);
        std::fill_n(parity, nn, -1);
        std::fill_n(node_degree, nn, 0);
        int comp_id = 0;

        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0 || st.cands[idx] != pair_mask) continue;
            const int p0 = st.topo->peer_offsets[idx];
            const int p1 = st.topo->peer_offsets[idx + 1];
            for (int p = p0; p < p1; ++p) {
                const int peer = st.topo->peers_flat[p];
                if (st.board->values[peer] == 0 && st.cands[peer] == pair_mask) {
                    ++node_degree[idx];
                }
            }
        }

        for (int start = 0; start < nn; ++start) {
            if (st.board->values[start] != 0 || st.cands[start] != pair_mask) continue;
            if (component[start] != -1) continue;

            int qh = 0;
            int qt = 0;
            bool bipartite_ok = true;
            int endpoint_count = 0;
            int node_count = 0;

            sp.bfs_queue[qt++] = start;
            component[start] = comp_id;
            parity[start] = 0;

            while (qh < qt) {
                const int cur = sp.bfs_queue[qh++];
                if (node_count < shared::ExactPatternScratchpad::MAX_NN) {
                    sp.als_cells[node_count++] = cur;
                }
                const int degree = node_degree[cur];
                if (degree > 2) {
                    bipartite_ok = false;
                } else if (degree == 1) {
                    ++endpoint_count;
                }

                const int p0 = st.topo->peer_offsets[cur];
                const int p1 = st.topo->peer_offsets[cur + 1];
                for (int p = p0; p < p1; ++p) {
                    const int nxt = st.topo->peers_flat[p];
                    if (st.board->values[nxt] != 0 || st.cands[nxt] != pair_mask) continue;

                    if (component[nxt] == -1) {
                        component[nxt] = comp_id;
                        parity[nxt] = parity[cur] ^ 1;
                        if (qt < shared::ExactPatternScratchpad::MAX_BFS) {
                            sp.bfs_queue[qt++] = nxt;
                        }
                    } else if (parity[nxt] == parity[cur]) {
                        bipartite_ok = false;
                    }
                }
            }

            ++comp_id;
            if (!bipartite_ok || node_count < 4) continue;
            if (!(endpoint_count == 0 || endpoint_count == 2)) continue;

            std::fill_n(in_component, nn, 0);
            std::fill_n(seen_parity0, nn, 0);
            std::fill_n(seen_parity1, nn, 0);
            for (int i = 0; i < node_count; ++i) {
                const int idx = sp.als_cells[i];
                in_component[idx] = 1;
                const int p0 = st.topo->peer_offsets[idx];
                const int p1 = st.topo->peer_offsets[idx + 1];
                int* seen = (parity[idx] == 0) ? seen_parity0 : seen_parity1;
                for (int p = p0; p < p1; ++p) {
                    seen[st.topo->peers_flat[p]] = 1;
                }
            }

            for (int t = 0; t < nn; ++t) {
                if (in_component[t] != 0) continue;
                if (st.board->values[t] != 0) continue;
                if (seen_parity0[t] == 0 || seen_parity1[t] == 0) continue;

                const ApplyResult er = st.eliminate(t, pair_mask);
                if (er == ApplyResult::Contradiction) {
                    s.elapsed_ns += st.now_ns() - t0;
                    return er;
                }
                progress = progress || (er == ApplyResult::Progress);
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_remote_pairs = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p4_hard
