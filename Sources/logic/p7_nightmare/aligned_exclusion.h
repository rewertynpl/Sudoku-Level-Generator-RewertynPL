// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: aligned_exclusion.h (Level 7 - Nightmare)
// Description: Full symmetric Aligned Pair Exclusion (APE) and
// Aligned Triple Exclusion (ATE), zero-allocation and bitboard-driven.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline int aligned_pair_candidate_cap(const CandidateState& st) {
    const int n = st.topo->n;
    if (n <= 9) return 8;
    if (n <= 16) return 7;
    if (n <= 25) return 6;
    if (n <= 36) return 5;
    return 4;
}

inline int aligned_triple_candidate_cap(const CandidateState& st) {
    const int n = st.topo->n;
    if (n <= 9) return 6;
    if (n <= 16) return 5;
    if (n <= 25) return 4;
    return 3;
}

inline bool aligned_pair_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    if (n > 64) return false;
    if (n <= 25) return st.board->empty_cells <= (st.topo->nn - st.topo->n);
    if (n <= 36) return st.board->empty_cells <= (st.topo->nn - 2 * st.topo->n);
    return st.board->empty_cells <= (st.topo->nn - 3 * st.topo->n);
}

inline bool aligned_triple_allowed(const CandidateState& st) {
    const int n = st.topo->n;
    if (n > 64) return false;
    if (n <= 25) return st.board->empty_cells <= (st.topo->nn - 2 * st.topo->n);
    if (n <= 36) return st.board->empty_cells <= (st.topo->nn - 3 * st.topo->n);
    return st.board->empty_cells <= (st.topo->nn - 4 * st.topo->n);
}

inline bool aligned_common_peer_survives_pair(
    const CandidateState& st,
    int a,
    uint64_t ba,
    int b,
    uint64_t bb) {
    const uint64_t forbid = ba | bb;
    const int p0 = st.topo->peer_offsets[a];
    const int p1 = st.topo->peer_offsets[a + 1];
    for (int p = p0; p < p1; ++p) {
        const int peer = st.topo->peers_flat[p];
        if (peer == b || st.board->values[peer] != 0) continue;
        if (!st.is_peer(b, peer)) continue;
        if ((st.cands[peer] & ~forbid) == 0ULL) return false;
    }
    return true;
}

inline bool aligned_common_peer_survives_triple(
    const CandidateState& st,
    int a,
    uint64_t ba,
    int b,
    uint64_t bb,
    int c,
    uint64_t bc) {
    const uint64_t forbid = ba | bb | bc;
    const int p0 = st.topo->peer_offsets[a];
    const int p1 = st.topo->peer_offsets[a + 1];
    for (int p = p0; p < p1; ++p) {
        const int peer = st.topo->peers_flat[p];
        if (peer == b || peer == c || st.board->values[peer] != 0) continue;
        if (!st.is_peer(b, peer) || !st.is_peer(c, peer)) continue;
        if ((st.cands[peer] & ~forbid) == 0ULL) return false;
    }
    return true;
}

inline bool aligned_pair_assignment_valid(
    CandidateState& st,
    int a,
    int da,
    int b,
    int db) {
    if (da == db) return false;
    if (!st.board->can_place(a, da) || !st.board->can_place(b, db)) return false;
    return aligned_common_peer_survives_pair(st, a, (1ULL << (da - 1)), b, (1ULL << (db - 1)));
}

inline uint64_t aligned_pair_collect_bad_mask(
    CandidateState& st,
    int cell,
    uint64_t cell_mask,
    int other,
    uint64_t other_mask) {
    uint64_t bad = 0ULL;
    uint64_t wc = cell_mask;
    while (wc != 0ULL) {
        const uint64_t bc = config::bit_lsb(wc);
        wc = config::bit_clear_lsb_u64(wc);
        const int dc = config::bit_ctz_u64(bc) + 1;

        bool valid = false;
        uint64_t wo = other_mask;
        while (wo != 0ULL) {
            const uint64_t bo = config::bit_lsb(wo);
            wo = config::bit_clear_lsb_u64(wo);
            const int d_other = config::bit_ctz_u64(bo) + 1;
            if (aligned_pair_assignment_valid(st, cell, dc, other, d_other)) {
                valid = true;
                break;
            }
        }
        if (!valid) bad |= bc;
    }
    return bad;
}

inline bool aligned_triple_assignment_valid(
    CandidateState& st,
    int a,
    int da,
    int b,
    int db,
    int c,
    int dc) {
    if (da == db || da == dc || db == dc) return false;
    if (!st.board->can_place(a, da) ||
        !st.board->can_place(b, db) ||
        !st.board->can_place(c, dc)) {
        return false;
    }
    return aligned_common_peer_survives_triple(
        st,
        a, (1ULL << (da - 1)),
        b, (1ULL << (db - 1)),
        c, (1ULL << (dc - 1)));
}

inline uint64_t aligned_triple_collect_bad_mask(
    CandidateState& st,
    int target_cell,
    uint64_t target_mask,
    int cell2,
    uint64_t mask2,
    int cell3,
    uint64_t mask3) {
    uint64_t bad = 0ULL;
    uint64_t wt = target_mask;
    while (wt != 0ULL) {
        const uint64_t bt = config::bit_lsb(wt);
        wt = config::bit_clear_lsb_u64(wt);
        const int dt = config::bit_ctz_u64(bt) + 1;

        bool valid = false;
        uint64_t w2 = mask2;
        while (w2 != 0ULL && !valid) {
            const uint64_t b2 = config::bit_lsb(w2);
            w2 = config::bit_clear_lsb_u64(w2);
            const int d2 = config::bit_ctz_u64(b2) + 1;

            uint64_t w3 = mask3;
            while (w3 != 0ULL) {
                const uint64_t b3 = config::bit_lsb(w3);
                w3 = config::bit_clear_lsb_u64(w3);
                const int d3 = config::bit_ctz_u64(b3) + 1;
                if (aligned_triple_assignment_valid(st, target_cell, dt, cell2, d2, cell3, d3)) {
                    valid = true;
                    break;
                }
            }
        }

        if (!valid) bad |= bt;
    }
    return bad;
}

inline ApplyResult apply_aligned_pair_exclusion(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (!aligned_pair_allowed(st)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int nn = st.topo->nn;
    const int cand_cap = aligned_pair_candidate_cap(st);
    bool progress = false;

    for (int a = 0; a < nn; ++a) {
        if (st.board->values[a] != 0) continue;
        const uint64_t ma = st.cands[a];
        if (ma == 0ULL || std::popcount(ma) > cand_cap) continue;

        for (int b = a + 1; b < nn; ++b) {
            if (st.board->values[b] != 0) continue;
            if (!st.is_peer(a, b)) continue;

            const uint64_t mb = st.cands[b];
            if (mb == 0ULL || std::popcount(mb) > cand_cap) continue;
            if (std::popcount(ma | mb) > (cand_cap + 2)) continue;

            const uint64_t bad_a = aligned_pair_collect_bad_mask(st, a, ma, b, mb);
            const uint64_t bad_b = aligned_pair_collect_bad_mask(st, b, mb, a, ma);

            if (bad_a != 0ULL) {
                const ApplyResult er = st.eliminate(a, bad_a);
                if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                progress = progress || (er == ApplyResult::Progress);
            }
            if (bad_b != 0ULL) {
                const ApplyResult er = st.eliminate(b, bad_b);
                if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                progress = progress || (er == ApplyResult::Progress);
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_aligned_pair_exclusion = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_aligned_triple_exclusion(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (!aligned_triple_allowed(st)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int nn = st.topo->nn;
    const int cand_cap = aligned_triple_candidate_cap(st);
    bool progress = false;

    for (int a = 0; a < nn; ++a) {
        if (st.board->values[a] != 0) continue;
        const uint64_t ma = st.cands[a];
        if (ma == 0ULL || std::popcount(ma) > cand_cap) continue;

        for (int b = a + 1; b < nn; ++b) {
            if (st.board->values[b] != 0) continue;
            if (!st.is_peer(a, b)) continue;

            const uint64_t mb = st.cands[b];
            if (mb == 0ULL || std::popcount(mb) > cand_cap) continue;
            if (std::popcount(ma | mb) > (cand_cap + 3)) continue;

            for (int c = b + 1; c < nn; ++c) {
                if (st.board->values[c] != 0) continue;
                if (!st.is_peer(a, c) || !st.is_peer(b, c)) continue;

                const uint64_t mc = st.cands[c];
                if (mc == 0ULL || std::popcount(mc) > cand_cap) continue;
                if (std::popcount(ma | mb | mc) > (cand_cap + 3)) continue;

                const uint64_t bad_a = aligned_triple_collect_bad_mask(st, a, ma, b, mb, c, mc);
                const uint64_t bad_b = aligned_triple_collect_bad_mask(st, b, mb, a, ma, c, mc);
                const uint64_t bad_c = aligned_triple_collect_bad_mask(st, c, mc, a, ma, b, mb);

                if (bad_a != 0ULL) {
                    const ApplyResult er = st.eliminate(a, bad_a);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
                if (bad_b != 0ULL) {
                    const ApplyResult er = st.eliminate(b, bad_b);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
                if (bad_c != 0ULL) {
                    const ApplyResult er = st.eliminate(c, bad_c);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_aligned_triple_exclusion = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
