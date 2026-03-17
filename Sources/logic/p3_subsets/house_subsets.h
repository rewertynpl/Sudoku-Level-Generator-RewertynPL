// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: house_subsets.h (Poziomy 2, 3, 4)
// Opis: Wykrywanie i aplikacja podzbiorów (Naked/Hidden Pairs, Triples, Quads).
//       Zero-allocation, bez założeń o geometrii symetrycznej, z mocniejszą
//       kanonicznością dla generatora/certyfikacji.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../config/bit_utils.h"
#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p3_subsets {

namespace detail {

inline constexpr int kMaxN = 64;

inline bool is_subset_cell_candidate(uint64_t mask, int subset) {
    const int bits = std::popcount(mask);
    return bits >= 2 && bits <= subset;
}

inline int bit_index_from_mask(uint64_t bit) {
    return static_cast<int>(std::countr_zero(bit));
}

inline void mark_subset_usage(GenericLogicCertifyResult& r, int subset, bool hidden) {
    if (!hidden && subset == 2) r.used_naked_pair = true;
    if (!hidden && subset == 3) r.used_naked_triple = true;
    if (!hidden && subset == 4) r.used_naked_quad = true;
    if (hidden && subset == 2) r.used_hidden_pair = true;
    if (hidden && subset == 3) r.used_hidden_triple = true;
    if (hidden && subset == 4) r.used_hidden_quad = true;
}

inline bool any_other_cell_in_house_has_only_union_digits(
    CandidateState& st,
    const int* house_cells,
    int house_len,
    uint64_t chosen_mask,
    uint64_t union_mask) {

    for (int i = 0; i < house_len; ++i) {
        const int idx = house_cells[i];
        if (((chosen_mask >> i) & 1ULL) != 0ULL) {
            continue;
        }
        if (st.board->values[idx] != 0) {
            continue;
        }
        const uint64_t cm = st.cands[idx];
        if (cm == 0ULL) {
            continue;
        }
        if ((cm & ~union_mask) == 0ULL && (cm & union_mask) != 0ULL) {
            return true;
        }
    }
    return false;
}

inline bool hidden_subset_is_canonical(
    const uint64_t* digit_pos,
    const int* digits,
    int subset,
    uint64_t cell_union_mask,
    uint64_t digit_union_mask) {

    if (std::popcount(cell_union_mask) != subset) {
        return false;
    }
    if (std::popcount(digit_union_mask) != subset) {
        return false;
    }

    // Każda cyfra musi występować co najmniej 2 razy, inaczej to zdegenerowany
    // hidden single albo artefakt certyfikacji.
    for (int i = 0; i < subset; ++i) {
        const int d = digits[i];
        const int cnt = std::popcount(digit_pos[d]);
        if (cnt < 2 || cnt > subset) {
            return false;
        }
    }

    // Wariant kanoniczny: każda wybrana komórka musi uczestniczyć w co najmniej
    // jednej z wybranych cyfr.
    uint64_t covered_cells = 0ULL;
    for (int i = 0; i < subset; ++i) {
        covered_cells |= digit_pos[digits[i]];
    }
    return covered_cells == cell_union_mask;
}

} // namespace detail

// Parametry:
// subset = 2 (Pair), 3 (Triple), 4 (Quad)
// hidden = true  -> Hidden subset
// hidden = false -> Naked subset
inline ApplyResult apply_house_subset(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    int subset,
    bool hidden) {

    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (subset < 2 || subset > 4) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int n = st.topo->n;
    bool progress = false;

    int house_cells[detail::kMaxN]{};
    int active_digits[detail::kMaxN]{};
    uint64_t digit_pos[detail::kMaxN]{};

    const size_t house_count = st.topo->house_offsets.size() - 1;
    for (size_t h = 0; h < house_count; ++h) {
        const int p0 = static_cast<int>(st.topo->house_offsets[h]);
        const int p1 = static_cast<int>(st.topo->house_offsets[h + 1]);

        int house_len = 0;
        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            house_cells[house_len++] = idx;
        }

        if (!hidden) {
            int candidates[detail::kMaxN]{};
            int m = 0;
            for (int i = 0; i < house_len; ++i) {
                const int idx = house_cells[i];
                if (st.board->values[idx] != 0) {
                    continue;
                }
                if (detail::is_subset_cell_candidate(st.cands[idx], subset)) {
                    candidates[m++] = idx;
                }
            }

            if (m < subset) {
                continue;
            }

            auto try_naked_combo = [&](const int* combo, int combo_size) -> ApplyResult {
                uint64_t union_mask = 0ULL;
                uint64_t chosen_house_mask = 0ULL;
                for (int i = 0; i < combo_size; ++i) {
                    const int idx = combo[i];
                    union_mask |= st.cands[idx];
                    for (int j = 0; j < house_len; ++j) {
                        if (house_cells[j] == idx) {
                            chosen_house_mask |= (1ULL << j);
                            break;
                        }
                    }
                }

                if (std::popcount(union_mask) != subset) {
                    return ApplyResult::NoProgress;
                }

                // Kanoniczność: nie łap degeneracji, gdzie identyczna redukcja jest już
                // trywialnym mniejszym subsetem albo nie ma nic do usunięcia.
                if (combo_size > subset) {
                    return ApplyResult::NoProgress;
                }
                if (detail::any_other_cell_in_house_has_only_union_digits(st, house_cells, house_len, chosen_house_mask, union_mask) && subset >= 3) {
                    // Dla triples/quads odrzucamy niekanoniczne rozlane układy, które nie
                    // są zwarte i zwykle rozwalają certyfikację required-hit.
                    return ApplyResult::NoProgress;
                }

                bool local_progress = false;
                for (int i = 0; i < house_len; ++i) {
                    if (((chosen_house_mask >> i) & 1ULL) != 0ULL) {
                        continue;
                    }
                    const int idx = house_cells[i];
                    if (st.board->values[idx] != 0) {
                        continue;
                    }
                    const uint64_t kill = st.cands[idx] & union_mask;
                    if (kill == 0ULL) {
                        continue;
                    }
                    const ApplyResult rr = st.eliminate(idx, kill);
                    if (rr == ApplyResult::Contradiction) {
                        return rr;
                    }
                    if (rr == ApplyResult::Progress) {
                        local_progress = true;
                    }
                }

                if (local_progress) {
                    progress = true;
                    return ApplyResult::Progress;
                }
                return ApplyResult::NoProgress;
            };

            if (subset == 2) {
                for (int a = 0; a < m; ++a) {
                    const int combo[2] = {candidates[a], 0};
                    for (int b = a + 1; b < m; ++b) {
                        const int combo2[2] = {candidates[a], candidates[b]};
                        const ApplyResult rr = try_naked_combo(combo2, 2);
                        if (rr == ApplyResult::Contradiction) {
                            s.elapsed_ns += st.now_ns() - t0;
                            return rr;
                        }
                    }
                }
            } else if (subset == 3) {
                for (int a = 0; a < m; ++a) {
                    for (int b = a + 1; b < m; ++b) {
                        for (int c = b + 1; c < m; ++c) {
                            const int combo[3] = {candidates[a], candidates[b], candidates[c]};
                            const ApplyResult rr = try_naked_combo(combo, 3);
                            if (rr == ApplyResult::Contradiction) {
                                s.elapsed_ns += st.now_ns() - t0;
                                return rr;
                            }
                        }
                    }
                }
            } else {
                for (int a = 0; a < m; ++a) {
                    for (int b = a + 1; b < m; ++b) {
                        for (int c = b + 1; c < m; ++c) {
                            for (int d = c + 1; d < m; ++d) {
                                const int combo[4] = {candidates[a], candidates[b], candidates[c], candidates[d]};
                                const ApplyResult rr = try_naked_combo(combo, 4);
                                if (rr == ApplyResult::Contradiction) {
                                    s.elapsed_ns += st.now_ns() - t0;
                                    return rr;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            std::fill_n(digit_pos, detail::kMaxN, 0ULL);
            int ad_count = 0;

            for (int d = 0; d < n; ++d) {
                uint64_t pos_mask = 0ULL;
                int cnt = 0;
                const uint64_t bit = (1ULL << d);
                bool already_placed = false;

                for (int i = 0; i < house_len; ++i) {
                    const int idx = house_cells[i];
                    if (st.board->values[idx] == d + 1) {
                        already_placed = true;
                        break;
                    }
                    if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                        pos_mask |= (1ULL << i);
                        ++cnt;
                    }
                }

                if (already_placed) {
                    continue;
                }
                if (cnt >= 2 && cnt <= subset) {
                    digit_pos[d] = pos_mask;
                    active_digits[ad_count++] = d;
                }
            }

            if (ad_count < subset) {
                continue;
            }

            auto try_hidden_combo = [&](const int* digits, int combo_size) -> ApplyResult {
                uint64_t cell_union_mask = 0ULL;
                uint64_t digit_union_mask = 0ULL;
                for (int i = 0; i < combo_size; ++i) {
                    cell_union_mask |= digit_pos[digits[i]];
                    digit_union_mask |= (1ULL << digits[i]);
                }

                if (!detail::hidden_subset_is_canonical(digit_pos, digits, combo_size, cell_union_mask, digit_union_mask)) {
                    return ApplyResult::NoProgress;
                }

                bool local_progress = false;
                uint64_t tmp = cell_union_mask;
                while (tmp != 0ULL) {
                    const uint64_t lsb = std::countr_zero(tmp) >= 64 ? 0ULL : (1ULL << std::countr_zero(tmp));
                    const int house_pos = static_cast<int>(std::countr_zero(tmp));
                    tmp &= (tmp - 1ULL);

                    const int idx = house_cells[house_pos];
                    if (st.board->values[idx] != 0) {
                        continue;
                    }

                    const uint64_t keep = st.cands[idx] & digit_union_mask;
                    if (keep == 0ULL) {
                        return ApplyResult::Contradiction;
                    }
                    const uint64_t kill = st.cands[idx] & ~digit_union_mask;
                    if (kill == 0ULL) {
                        continue;
                    }

                    const ApplyResult rr = st.eliminate(idx, kill);
                    if (rr == ApplyResult::Contradiction) {
                        return rr;
                    }
                    if (rr == ApplyResult::Progress) {
                        local_progress = true;
                    }
                    (void)lsb;
                }

                if (local_progress) {
                    progress = true;
                    return ApplyResult::Progress;
                }
                return ApplyResult::NoProgress;
            };

            if (subset == 2) {
                for (int a = 0; a < ad_count; ++a) {
                    for (int b = a + 1; b < ad_count; ++b) {
                        const int combo[2] = {active_digits[a], active_digits[b]};
                        const ApplyResult rr = try_hidden_combo(combo, 2);
                        if (rr == ApplyResult::Contradiction) {
                            s.elapsed_ns += st.now_ns() - t0;
                            return rr;
                        }
                    }
                }
            } else if (subset == 3) {
                for (int a = 0; a < ad_count; ++a) {
                    for (int b = a + 1; b < ad_count; ++b) {
                        for (int c = b + 1; c < ad_count; ++c) {
                            const int combo[3] = {active_digits[a], active_digits[b], active_digits[c]};
                            const ApplyResult rr = try_hidden_combo(combo, 3);
                            if (rr == ApplyResult::Contradiction) {
                                s.elapsed_ns += st.now_ns() - t0;
                                return rr;
                            }
                        }
                    }
                }
            } else {
                for (int a = 0; a < ad_count; ++a) {
                    for (int b = a + 1; b < ad_count; ++b) {
                        for (int c = b + 1; c < ad_count; ++c) {
                            for (int d = c + 1; d < ad_count; ++d) {
                                const int combo[4] = {active_digits[a], active_digits[b], active_digits[c], active_digits[d]};
                                const ApplyResult rr = try_hidden_combo(combo, 4);
                                if (rr == ApplyResult::Contradiction) {
                                    s.elapsed_ns += st.now_ns() - t0;
                                    return rr;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        detail::mark_subset_usage(r, subset, hidden);
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_naked_pair(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    return apply_house_subset(st, s, r, 2, false);
}

inline ApplyResult apply_hidden_pair(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    return apply_house_subset(st, s, r, 2, true);
}

inline ApplyResult apply_naked_triple(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    return apply_house_subset(st, s, r, 3, false);
}

inline ApplyResult apply_hidden_triple(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    return apply_house_subset(st, s, r, 3, true);
}

inline ApplyResult apply_naked_quad(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    return apply_house_subset(st, s, r, 4, false);
}

inline ApplyResult apply_hidden_quad(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    return apply_house_subset(st, s, r, 4, true);
}

} // namespace sudoku_hpc::logic::p3_subsets