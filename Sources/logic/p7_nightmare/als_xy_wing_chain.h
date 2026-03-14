// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: als_xy_wing_chain.h (Level 7 - Nightmare)
// Description: Direct zero-allocation passes for ALS-XY-Wing, ALS-Chain and
// ALS-AIC style eliminations on a shared ALS/RCC core.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/als_builder.h"
#include "../shared/exact_pattern_scratchpad.h"

#include "../p6_diabolical/als_xz.h"
#include "../p5_expert/xyz_w_wing.h"
#include "../p6_diabolical/chains_basic.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool als_deep_pass_allowed(const CandidateState& st) {
    if (st.topo->n > 49) return false;
    return st.board->empty_cells <= (st.topo->nn - std::max(st.topo->n, (3 * st.topo->n) / 2));
}

inline int als_direct_max_size(const CandidateState& st) {
    if (st.topo->n <= 12 && st.board->empty_cells <= (st.topo->nn - 3 * st.topo->n)) return 5;
    if (st.topo->n <= 16 && st.board->empty_cells <= (st.topo->nn - 4 * st.topo->n)) return 5;
    if (st.topo->n <= 25 && st.board->empty_cells <= (st.topo->nn - 3 * st.topo->n)) return 5;
    if (st.topo->n <= 36 && st.board->empty_cells <= (st.topo->nn - 2 * st.topo->n)) return 5;
    return 4;
}

inline int build_als_xy_list(const CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, als_direct_max_size(st));
    if (sp.als_count >= shared::ExactPatternScratchpad::MAX_NN) return als_cnt;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        const uint64_t m = st.cands[idx];
        if (std::popcount(m) != 2) continue;

        const int cell[1] = {idx};
        shared::als_push_record(sp, words, -1, cell, 1, m);
        if (sp.als_count >= shared::ExactPatternScratchpad::MAX_NN) break;
    }
    return sp.als_count;
}

inline ApplyResult direct_als_xz_bridge_pass(CandidateState& st, int als_limit) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, als_direct_max_size(st));
    if (als_cnt < 2) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, als_limit);
    int a_x[8]{}, b_x[8]{}, a_z[8]{}, b_z[8]{};
    int a_xc = 0, b_xc = 0, a_zc = 0, b_zc = 0;

    for (int ia = 0; ia < limit; ++ia) {
        const shared::ALS& a = sp.als_list[ia];
        for (int ib = ia + 1; ib < limit; ++ib) {
            const shared::ALS& b = sp.als_list[ib];
            if (shared::als_overlap(a, b, words)) continue;

            uint64_t xmask = a.digit_mask & b.digit_mask;
            while (xmask != 0ULL) {
                const uint64_t x = config::bit_lsb(xmask);
                xmask = config::bit_clear_lsb_u64(xmask);
                if (!shared::als_restricted_common(st, a, b, x, a_x, a_xc, b_x, b_xc)) continue;

                uint64_t zmask = (a.digit_mask & b.digit_mask) & ~x;
                while (zmask != 0ULL) {
                    const uint64_t z = config::bit_lsb(zmask);
                    zmask = config::bit_clear_lsb_u64(zmask);
                    a_zc = shared::als_collect_holders_for_digit(st, a, z, a_z);
                    b_zc = shared::als_collect_holders_for_digit(st, b, z, b_z);
                    if (a_zc <= 0 || b_zc <= 0) continue;
                    const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                        st, z, a_z, a_zc, b_z, b_zc, &a, &b);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_graph_chain_pass(
    CandidateState& st,
    int min_depth,
    int max_depth,
    int als_limit) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, als_direct_max_size(st));
    if (als_cnt < 3) return ApplyResult::NoProgress;

    const int limit = std::min(als_cnt, als_limit);
    int edge_u[1024]{}, edge_v[1024]{};
    uint64_t edge_digit[1024]{};
    const int edge_count = shared::als_collect_rcc_edges(st, sp.als_list, limit, edge_u, edge_v, edge_digit, 1024);
    if (edge_count == 0) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    int start_z[8]{}, end_z[8]{};

    int state_als[1024]{};
    int state_parent[1024]{};
    int state_in_digit[1024]{};
    int state_first_digit[1024]{};
    int state_depth[1024]{};

    const int visited_cap = std::min(limit * 64, shared::ExactPatternScratchpad::MAX_NN);
    for (int start = 0; start < limit; ++start) {
        std::fill_n(sp.visited, visited_cap, 0);
        int qh = 0;
        int qt = 0;

        for (int e = 0; e < edge_count; ++e) {
            if (edge_u[e] != start) continue;
            const int nxt = edge_v[e];
            const int in_digit = config::bit_ctz_u64(edge_digit[e]) + 1;
            const int visit_idx = nxt * 64 + (in_digit - 1);
            if (visit_idx < visited_cap && sp.visited[visit_idx] != 0) continue;
            if (visit_idx < visited_cap) sp.visited[visit_idx] = 1;

            state_als[qt] = nxt;
            state_parent[qt] = -1;
            state_in_digit[qt] = in_digit;
            state_first_digit[qt] = in_digit;
            state_depth[qt] = 1;
            ++qt;
            if (qt >= 1024) break;
        }

        while (qh < qt) {
            const int sid = qh++;
            const int cur_als = state_als[sid];
            const int in_digit = state_in_digit[sid];
            const int first_digit = state_first_digit[sid];
            const int depth = state_depth[sid];

            const uint64_t forbid =
                (1ULL << (first_digit - 1)) | (1ULL << (in_digit - 1));
            uint64_t zmask = sp.als_list[start].digit_mask & sp.als_list[cur_als].digit_mask & ~forbid;
            if (depth >= min_depth) {
                while (zmask != 0ULL) {
                    const uint64_t z = config::bit_lsb(zmask);
                    zmask = config::bit_clear_lsb_u64(zmask);
                    const int start_cnt = shared::als_collect_holders_for_digit(st, sp.als_list[start], z, start_z);
                    const int end_cnt = shared::als_collect_holders_for_digit(st, sp.als_list[cur_als], z, end_z);
                    if (start_cnt <= 0 || end_cnt <= 0) continue;
                    const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                        st, z, start_z, start_cnt, end_z, end_cnt, &sp.als_list[start], &sp.als_list[cur_als]);
                    if (er != ApplyResult::NoProgress) return er;
                }
            }

            if (depth >= max_depth) continue;

            for (int e = 0; e < edge_count; ++e) {
                if (edge_u[e] != cur_als) continue;
                const int nxt = edge_v[e];
                const int out_digit = config::bit_ctz_u64(edge_digit[e]) + 1;
                if (out_digit == in_digit || nxt == start) continue;
                if (shared::als_path_has_overlap(sp.als_list[nxt], sp.als_list, state_als, state_parent, sid, words)) continue;

                const int visit_idx = nxt * 64 + (out_digit - 1);
                if (visit_idx < visited_cap && sp.visited[visit_idx] != 0) continue;
                if (visit_idx < visited_cap) sp.visited[visit_idx] = 1;

                if (qt >= 1024) break;
                state_als[qt] = nxt;
                state_parent[qt] = sid;
                state_in_digit[qt] = out_digit;
                state_first_digit[qt] = first_digit;
                state_depth[qt] = depth + 1;
                ++qt;
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_xy_wing_pass(CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = build_als_xy_list(st);
    if (als_cnt < 3) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, 128);

    int p_x[8]{}, w1_x[8]{}, p_y[8]{}, w2_y[8]{}, w1_z[8]{}, w2_z[8]{};
    int p_xc = 0, w1_xc = 0, p_yc = 0, w2_yc = 0, w1_zc = 0, w2_zc = 0;

    for (int ip = 0; ip < limit; ++ip) {
        const shared::ALS& pivot = sp.als_list[ip];
        for (int i1 = 0; i1 < limit; ++i1) {
            if (i1 == ip) continue;
            const shared::ALS& wing1 = sp.als_list[i1];
            if (shared::als_overlap(pivot, wing1, words)) continue;

            const uint64_t common_p1 = pivot.digit_mask & wing1.digit_mask;
            if (common_p1 == 0ULL) continue;

            for (int i2 = i1 + 1; i2 < limit; ++i2) {
                if (i2 == ip) continue;
                const shared::ALS& wing2 = sp.als_list[i2];
                if (shared::als_overlap(pivot, wing2, words) || shared::als_overlap(wing1, wing2, words)) continue;

                const uint64_t common_p2 = pivot.digit_mask & wing2.digit_mask;
                if (common_p2 == 0ULL) continue;

                uint64_t wx = common_p1;
                while (wx != 0ULL) {
                    const uint64_t x = config::bit_lsb(wx);
                    wx = config::bit_clear_lsb_u64(wx);
                    if (!shared::als_restricted_common(st, pivot, wing1, x, p_x, p_xc, w1_x, w1_xc)) continue;

                    uint64_t wy = common_p2 & ~x;
                    while (wy != 0ULL) {
                        const uint64_t y = config::bit_lsb(wy);
                        wy = config::bit_clear_lsb_u64(wy);
                        if (!shared::als_restricted_common(st, pivot, wing2, y, p_y, p_yc, w2_y, w2_yc)) continue;

                        uint64_t zmask = (wing1.digit_mask & wing2.digit_mask) & ~(x | y);
                        while (zmask != 0ULL) {
                            const uint64_t z = config::bit_lsb(zmask);
                            zmask = config::bit_clear_lsb_u64(zmask);
                            w1_zc = shared::als_collect_holders_for_digit(st, wing1, z, w1_z);
                            w2_zc = shared::als_collect_holders_for_digit(st, wing2, z, w2_z);
                            if (w1_zc <= 0 || w2_zc <= 0) continue;
                            const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                                st, z, w1_z, w1_zc, w2_z, w2_zc, &pivot, &wing1, &wing2);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_chain_pass(CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, als_direct_max_size(st));
    if (als_cnt < 3) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, 96);

    int a_x[8]{}, b_x[8]{}, b_y[8]{}, c_y[8]{}, a_z[8]{}, c_z[8]{};
    int a_xc = 0, b_xc = 0, b_yc = 0, c_yc = 0, a_zc = 0, c_zc = 0;

    for (int ia = 0; ia < limit; ++ia) {
        const shared::ALS& a = sp.als_list[ia];
        for (int ib = 0; ib < limit; ++ib) {
            if (ib == ia) continue;
            const shared::ALS& b = sp.als_list[ib];
            if (shared::als_overlap(a, b, words)) continue;

            uint64_t xmask = a.digit_mask & b.digit_mask;
            while (xmask != 0ULL) {
                const uint64_t x = config::bit_lsb(xmask);
                xmask = config::bit_clear_lsb_u64(xmask);
                if (!shared::als_restricted_common(st, a, b, x, a_x, a_xc, b_x, b_xc)) continue;

                for (int ic = 0; ic < limit; ++ic) {
                    if (ic == ia || ic == ib) continue;
                    const shared::ALS& c = sp.als_list[ic];
                    if (shared::als_overlap(a, c, words) || shared::als_overlap(b, c, words)) continue;

                    uint64_t ymask = (b.digit_mask & c.digit_mask) & ~x;
                    while (ymask != 0ULL) {
                        const uint64_t y = config::bit_lsb(ymask);
                        ymask = config::bit_clear_lsb_u64(ymask);
                        if (!shared::als_restricted_common(st, b, c, y, b_y, b_yc, c_y, c_yc)) continue;

                        uint64_t zmask = (a.digit_mask & c.digit_mask) & ~(x | y);
                        while (zmask != 0ULL) {
                            const uint64_t z = config::bit_lsb(zmask);
                            zmask = config::bit_clear_lsb_u64(zmask);
                            a_zc = shared::als_collect_holders_for_digit(st, a, z, a_z);
                            c_zc = shared::als_collect_holders_for_digit(st, c, z, c_z);
                            if (a_zc <= 0 || c_zc <= 0) continue;
                            const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                                st, z, a_z, a_zc, c_z, c_zc, &a, &b, &c);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_aic_pass(CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, als_direct_max_size(st));
    if (als_cnt < 4) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, 48);

    int a_h[8]{}, b_h[8]{}, c_h[8]{}, d_h[8]{}, az[8]{}, dz[8]{};
    int a_hc = 0, b_hc = 0, c_hc = 0, d_hc = 0, azc = 0, dzc = 0;

    for (int ia = 0; ia < limit; ++ia) {
        const shared::ALS& a = sp.als_list[ia];
        for (int ib = 0; ib < limit; ++ib) {
            if (ib == ia) continue;
            const shared::ALS& b = sp.als_list[ib];
            if (shared::als_overlap(a, b, words)) continue;

            uint64_t xmask = a.digit_mask & b.digit_mask;
            while (xmask != 0ULL) {
                const uint64_t x = config::bit_lsb(xmask);
                xmask = config::bit_clear_lsb_u64(xmask);
                if (!shared::als_restricted_common(st, a, b, x, a_h, a_hc, b_h, b_hc)) continue;

                for (int ic = 0; ic < limit; ++ic) {
                    if (ic == ia || ic == ib) continue;
                    const shared::ALS& c = sp.als_list[ic];
                    if (shared::als_overlap(a, c, words) || shared::als_overlap(b, c, words)) continue;

                    uint64_t ymask = (b.digit_mask & c.digit_mask) & ~x;
                    while (ymask != 0ULL) {
                        const uint64_t y = config::bit_lsb(ymask);
                        ymask = config::bit_clear_lsb_u64(ymask);
                        if (!shared::als_restricted_common(st, b, c, y, b_h, b_hc, c_h, c_hc)) continue;

                        for (int id = 0; id < limit; ++id) {
                            if (id == ia || id == ib || id == ic) continue;
                            const shared::ALS& d = sp.als_list[id];
                            if (shared::als_overlap(a, d, words) || shared::als_overlap(b, d, words) || shared::als_overlap(c, d, words)) continue;

                            uint64_t wmask = (c.digit_mask & d.digit_mask) & ~(x | y);
                            while (wmask != 0ULL) {
                                const uint64_t w = config::bit_lsb(wmask);
                                wmask = config::bit_clear_lsb_u64(wmask);
                                if (!shared::als_restricted_common(st, c, d, w, c_h, c_hc, d_h, d_hc)) continue;

                                uint64_t zmask = (a.digit_mask & d.digit_mask) & ~(x | y | w);
                                while (zmask != 0ULL) {
                                    const uint64_t z = config::bit_lsb(zmask);
                                    zmask = config::bit_clear_lsb_u64(zmask);
                                    azc = shared::als_collect_holders_for_digit(st, a, z, az);
                                    dzc = shared::als_collect_holders_for_digit(st, d, z, dz);
                                    if (azc <= 0 || dzc <= 0) continue;
                                    const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                                        st, z, az, azc, dz, dzc, &a, &b, &c, &d);
                                    if (er != ApplyResult::NoProgress) return er;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline bool als_aic_digit_chain_connects(
    shared::ExactPatternScratchpad& sp,
    int start_node,
    int end_node,
    int depth_cap) {
    if (start_node < 0 || end_node < 0 || start_node == end_node) return false;
    int neighbors[256]{};
    int* const vis_even = sp.visited;
    int* const vis_odd = sp.bfs_depth;
    int* const queue_state = sp.bfs_queue;
    int* const queue_depth = sp.bfs_parent;

    std::fill_n(vis_even, sp.dyn_node_count, 0);
    std::fill_n(vis_odd, sp.dyn_node_count, 0);

    int qh = 0;
    int qt = 0;
    const int first_cnt = aic_collect_neighbors(sp, start_node, 1, neighbors);
    for (int i = 0; i < first_cnt; ++i) {
        const int v = neighbors[i];
        if (v == end_node) return true;
        if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
        queue_state[qt] = (v << 1) | 1;
        queue_depth[qt] = 1;
        vis_odd[v] = 1;
        ++qt;
    }

    while (qh < qt) {
        const int state = queue_state[qh];
        const int dep = queue_depth[qh];
        ++qh;

        const int u = (state >> 1);
        const int last_type = (state & 1);
        const int next_type = 1 - last_type;
        if (dep >= depth_cap) continue;

        const int next_cnt = aic_collect_neighbors(sp, u, next_type, neighbors);
        for (int i = 0; i < next_cnt; ++i) {
            const int v = neighbors[i];
            const int nd = dep + 1;
            if (v == end_node && (nd & 1) == 1) return true;

            int* const vis = (next_type == 0) ? vis_even : vis_odd;
            if (vis[v] != 0) continue;
            vis[v] = 1;
            if (qt >= shared::ExactPatternScratchpad::MAX_BFS) continue;
            queue_state[qt] = (v << 1) | next_type;
            queue_depth[qt] = nd;
            ++qt;
        }
    }
    return false;
}

inline ApplyResult direct_als_candidate_chain_bridge_pass(
    CandidateState& st,
    int als_limit,
    int depth_cap) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, als_direct_max_size(st));
    if (als_cnt < 2) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, als_limit);
    int a_z[8]{}, b_z[8]{};

    for (int digit = 1; digit <= st.topo->n; ++digit) {
        if (!shared::build_grouped_link_graph_for_digit(st, digit, sp)) continue;
        if (sp.dyn_node_count < 2 || sp.dyn_strong_edge_count == 0) continue;

        const uint64_t z = (1ULL << (digit - 1));
        for (int ia = 0; ia < limit; ++ia) {
            const shared::ALS& a = sp.als_list[ia];
            if ((a.digit_mask & z) == 0ULL) continue;
            const int a_zc = shared::als_collect_holders_for_digit(st, a, z, a_z);
            if (a_zc <= 0 || a_zc > 8) continue;

            for (int ib = ia + 1; ib < limit; ++ib) {
                const shared::ALS& b = sp.als_list[ib];
                if ((b.digit_mask & z) == 0ULL) continue;
                if (shared::als_overlap(a, b, words)) continue;
                const int b_zc = shared::als_collect_holders_for_digit(st, b, z, b_z);
                if (b_zc <= 0 || b_zc > 8) continue;

                bool connected = false;
                for (int i = 0; i < a_zc && !connected; ++i) {
                    const int na = sp.dyn_cell_to_node[a_z[i]];
                    if (na < 0) continue;
                    for (int j = 0; j < b_zc; ++j) {
                        const int nb = sp.dyn_cell_to_node[b_z[j]];
                        if (nb < 0) continue;
                        if (als_aic_digit_chain_connects(sp, na, nb, depth_cap)) {
                            connected = true;
                            break;
                        }
                    }
                }
                if (!connected) continue;

                const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                    st, z, a_z, a_zc, b_z, b_zc, &a, &b);
                if (er != ApplyResult::NoProgress) return er;
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_als_xy_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    ApplyResult ar;
    const int n = st.topo->n;
    const int bridge_limit = std::clamp(64 + n * 2, 96, 144);
    const int graph_limit = std::clamp(48 + n / 2, 56, 80);
    const int graph_depth = std::clamp(4 + n / 12, 4, 6);

    StrategyStats tmp{};
    if (als_deep_pass_allowed(st)) {
        ar = direct_als_xy_wing_pass(st);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_xy_wing = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_xz_bridge_pass(st, bridge_limit);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_xy_wing = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_graph_chain_pass(st, 2, graph_depth, graph_limit);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_xy_wing = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

    }

    ar = p6_diabolical::apply_als_xz(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_xy_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    ar = p5_expert::apply_xyz_wing(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_xy_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_als_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    ApplyResult ar;
    const int n = st.topo->n;
    const int bridge_limit = std::clamp(72 + n * 2, 104, 160);
    const int candidate_bridge_limit = std::clamp(56 + n / 2, 64, 96);
    const int candidate_bridge_depth = std::clamp(7 + n / 10, 7, 10);
    const int graph_limit = std::clamp(56 + n / 2, 64, 96);
    const int graph_depth = std::clamp(5 + n / 12, 5, 7);

    StrategyStats tmp{};
    if (als_deep_pass_allowed(st)) {
        ar = direct_als_xz_bridge_pass(st, bridge_limit);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_chain = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_candidate_chain_bridge_pass(st, candidate_bridge_limit, candidate_bridge_depth);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_chain = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_graph_chain_pass(st, 2, graph_depth, graph_limit);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_chain = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_chain_pass(st);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_chain = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
    }

    ar = p6_diabolical::apply_als_xz(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_chain = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    ar = p6_diabolical::apply_xy_chain(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_chain = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_als_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    ApplyResult ar;

    StrategyStats tmp{};
    if (als_deep_pass_allowed(st)) {
        ar = direct_als_xz_bridge_pass(st, 112);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_aic = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_graph_chain_pass(st, 3, 6, 64);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_aic = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_aic_pass(st);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_aic = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }

        ar = direct_als_candidate_chain_bridge_pass(st, 72, 10);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_aic = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ar;
        }
    }

    ar = direct_als_chain_pass(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_aic = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    bool used = false;
    const int implication_depth = std::clamp(10 + st.topo->n / 2, 10, 22);
    ar = bounded_implication_core(st, tmp, r, implication_depth, true, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_als_aic = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
