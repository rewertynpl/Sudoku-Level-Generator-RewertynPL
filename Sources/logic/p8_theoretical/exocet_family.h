// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: exocet_family.h (Level 8 - Theoretical)
// Description: Exocet and Senior Exocet with direct structural probing
// on detected base/target layouts, zero-allocation.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/required_strategy_gate.h"
#include "../shared/state_probe.h"
#include "../p7_nightmare/aic_grouped_aic.h"
#include "p8_density.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool exocet_propagate_singles(CandidateState& st, int max_steps) {
    return shared::propagate_singles(st, max_steps);
}

inline bool exocet_probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int digit,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    return shared::probe_candidate_contradiction(st, idx, digit, max_steps, sp);
}

inline ApplyResult exocet_apply_house_confinement(
    CandidateState& st,
    int house,
    uint64_t digit_mask,
    const int* allowed,
    int allowed_count) {
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];

    uint64_t mask = digit_mask;
    while (mask != 0ULL) {
        const uint64_t bit = config::bit_lsb(mask);
        mask = config::bit_clear_lsb_u64(mask);

        int total_places = 0;
        int allowed_places = 0;
        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            if (st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
            ++total_places;
            for (int i = 0; i < allowed_count; ++i) {
                if (allowed[i] == idx) {
                    ++allowed_places;
                    break;
                }
            }
        }
        if (total_places <= 0 || total_places != allowed_places) continue;

        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            bool in_allowed = false;
            for (int i = 0; i < allowed_count; ++i) {
                if (allowed[i] == idx) {
                    in_allowed = true;
                    break;
                }
            }
            if (in_allowed || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
            const ApplyResult er = st.eliminate(idx, bit);
            if (er != ApplyResult::NoProgress) return er;
        }
    }

    return ApplyResult::NoProgress;
}

inline void exocet_push_unique_cell(
    int* cells,
    int& count,
    int idx) {
    for (int i = 0; i < count; ++i) {
        if (cells[i] == idx) return;
    }
    if (count < 64) cells[count++] = idx;
}

inline int exocet_collect_line_targets(
    const CandidateState& st,
    int house,
    int base_box,
    uint64_t digit_mask,
    int* out_cells) {
    const int p0 = st.topo->house_offsets[house];
    const int p1 = st.topo->house_offsets[house + 1];
    int count = 0;
    for (int p = p0; p < p1; ++p) {
        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
        if (st.board->values[idx] != 0) continue;
        if (st.topo->cell_box[idx] == base_box) continue;
        if ((st.cands[idx] & digit_mask) == 0ULL) continue;
        exocet_push_unique_cell(out_cells, count, idx);
    }
    return count;
}

inline int exocet_collect_scope_cells(
    const CandidateState& st,
    int base1,
    int base2,
    int t1,
    int t2,
    uint64_t digit_mask,
    int* out_cells) {
    int count = 0;
    const int n = st.topo->n;
    const int r1 = st.topo->cell_row[base1];
    const int r2 = st.topo->cell_row[base2];
    const int c1 = st.topo->cell_col[base1];
    const int c2 = st.topo->cell_col[base2];
    const int b = st.topo->cell_box[base1];
    const int row_h1 = r1;
    const int row_h2 = r2;
    const int col_h1 = n + c1;
    const int col_h2 = n + c2;

    exocet_push_unique_cell(out_cells, count, base1);
    exocet_push_unique_cell(out_cells, count, base2);
    exocet_push_unique_cell(out_cells, count, t1);
    exocet_push_unique_cell(out_cells, count, t2);

    int family[64]{};
    int family_count = exocet_collect_line_targets(st, row_h1, b, digit_mask, family);
    for (int i = 0; i < family_count; ++i) exocet_push_unique_cell(out_cells, count, family[i]);
    family_count = exocet_collect_line_targets(st, row_h2, b, digit_mask, family);
    for (int i = 0; i < family_count; ++i) exocet_push_unique_cell(out_cells, count, family[i]);
    family_count = exocet_collect_line_targets(st, col_h1, b, digit_mask, family);
    for (int i = 0; i < family_count; ++i) exocet_push_unique_cell(out_cells, count, family[i]);
    family_count = exocet_collect_line_targets(st, col_h2, b, digit_mask, family);
    for (int i = 0; i < family_count; ++i) exocet_push_unique_cell(out_cells, count, family[i]);

    return count;
}

inline ApplyResult exocet_eliminate_outside_allowed(
    CandidateState& st,
    uint64_t bit,
    const int* scope_cells,
    int scope_count,
    const int* allowed,
    int allowed_count) {
    for (int i = 0; i < scope_count; ++i) {
        const int idx = scope_cells[i];
        bool in_allowed = false;
        for (int j = 0; j < allowed_count; ++j) {
            if (allowed[j] == idx) {
                in_allowed = true;
                break;
            }
        }
        if (in_allowed || st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
        const ApplyResult er = st.eliminate(idx, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult exocet_apply_target_typing(
    CandidateState& st,
    int base1,
    int base2,
    int t1,
    int t2,
    uint64_t target_union,
    const int* row1_targets,
    int row1_target_count,
    const int* row2_targets,
    int row2_target_count,
    const int* col1_targets,
    int col1_target_count,
    const int* col2_targets,
    int col2_target_count) {
    if (target_union == 0ULL) return ApplyResult::NoProgress;

    int scope[64]{};
    const int scope_count = exocet_collect_scope_cells(st, base1, base2, t1, t2, target_union, scope);

    uint64_t mask = target_union;
    while (mask != 0ULL) {
        const uint64_t bit = config::bit_lsb(mask);
        mask = config::bit_clear_lsb_u64(mask);

        int row1_live = 0;
        int row2_live = 0;
        int col1_live = 0;
        int col2_live = 0;
        int allowed[64]{};
        int allowed_count = 0;

        for (int i = 0; i < row1_target_count; ++i) {
            const int idx = row1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++row1_live;
                exocet_push_unique_cell(allowed, allowed_count, idx);
            }
        }
        for (int i = 0; i < row2_target_count; ++i) {
            const int idx = row2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++row2_live;
            }
        }
        for (int i = 0; i < col1_target_count; ++i) {
            const int idx = col1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++col1_live;
            }
        }
        for (int i = 0; i < col2_target_count; ++i) {
            const int idx = col2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++col2_live;
            }
        }

        if (row1_live > 0 && col2_live > 0 && row2_live == 0 && col1_live == 0) {
            for (int i = 0; i < col2_target_count; ++i) {
                const int idx = col2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, scope, scope_count, allowed, allowed_count);
            if (er != ApplyResult::NoProgress) return er;
            continue;
        }

        allowed_count = 0;
        for (int i = 0; i < row2_target_count; ++i) {
            const int idx = row2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                exocet_push_unique_cell(allowed, allowed_count, idx);
            }
        }
        if (row2_live > 0 && col1_live > 0 && row1_live == 0 && col2_live == 0) {
            for (int i = 0; i < col1_target_count; ++i) {
                const int idx = col1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t2);
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, scope, scope_count, allowed, allowed_count);
            if (er != ApplyResult::NoProgress) return er;
            continue;
        }

        if (row1_live > 0 && row2_live == 0) {
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, row2_targets, row2_target_count, row1_targets, 0);
            if (er != ApplyResult::NoProgress) return er;
        }
        if (row2_live > 0 && row1_live == 0) {
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, row1_targets, row1_target_count, row2_targets, 0);
            if (er != ApplyResult::NoProgress) return er;
        }
        if (col1_live > 0 && col2_live == 0) {
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, col2_targets, col2_target_count, col1_targets, 0);
            if (er != ApplyResult::NoProgress) return er;
        }
        if (col2_live > 0 && col1_live == 0) {
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, col1_targets, col1_target_count, col2_targets, 0);
            if (er != ApplyResult::NoProgress) return er;
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult exocet_apply_senior_companion_typing(
    CandidateState& st,
    int base1,
    int base2,
    int t1,
    int t2,
    uint64_t target_union,
    const int* row1_targets,
    int row1_target_count,
    const int* row2_targets,
    int row2_target_count,
    const int* col1_targets,
    int col1_target_count,
    const int* col2_targets,
    int col2_target_count) {
    if (target_union == 0ULL) return ApplyResult::NoProgress;

    int scope[64]{};
    const int scope_count = exocet_collect_scope_cells(st, base1, base2, t1, t2, target_union, scope);

    uint64_t mask = target_union;
    while (mask != 0ULL) {
        const uint64_t bit = config::bit_lsb(mask);
        mask = config::bit_clear_lsb_u64(mask);

        int row1_live = 0;
        int row2_live = 0;
        int col1_live = 0;
        int col2_live = 0;

        for (int i = 0; i < row1_target_count; ++i) {
            const int idx = row1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++row1_live;
        }
        for (int i = 0; i < row2_target_count; ++i) {
            const int idx = row2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++row2_live;
        }
        for (int i = 0; i < col1_target_count; ++i) {
            const int idx = col1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++col1_live;
        }
        for (int i = 0; i < col2_target_count; ++i) {
            const int idx = col2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++col2_live;
        }

        int allowed[64]{};
        int allowed_count = 0;
        bool matched = false;

        if (row1_live > 0 && row2_live > 0 && col1_live == 0 && col2_live == 0) {
            for (int i = 0; i < row1_target_count; ++i) {
                const int idx = row1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < row2_target_count; ++i) {
                const int idx = row2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col1_live > 0 && col2_live > 0 && row1_live == 0 && row2_live == 0) {
            for (int i = 0; i < col1_target_count; ++i) {
                const int idx = col1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < col2_target_count; ++i) {
                const int idx = col2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && col1_live > 0 && row2_live > 0 && col2_live == 0) {
            for (int i = 0; i < row1_target_count; ++i) {
                const int idx = row1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < col1_target_count; ++i) {
                const int idx = col1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < row2_target_count; ++i) {
                const int idx = row2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && col2_live > 0 && row2_live > 0 && col1_live == 0) {
            for (int i = 0; i < row1_target_count; ++i) {
                const int idx = row1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < col2_target_count; ++i) {
                const int idx = col2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < row2_target_count; ++i) {
                const int idx = row2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        }

        if (!matched || allowed_count == 0) continue;
        const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, scope, scope_count, allowed, allowed_count);
        if (er != ApplyResult::NoProgress) return er;
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult exocet_apply_senior_cross_line_restriction(
    CandidateState& st,
    int base1,
    int base2,
    int t1,
    int t2,
    uint64_t target_union,
    const int* row1_targets,
    int row1_target_count,
    const int* row2_targets,
    int row2_target_count,
    const int* col1_targets,
    int col1_target_count,
    const int* col2_targets,
    int col2_target_count) {
    if (target_union == 0ULL) return ApplyResult::NoProgress;

    int scope[64]{};
    const int scope_count = exocet_collect_scope_cells(st, base1, base2, t1, t2, target_union, scope);

    uint64_t mask = target_union;
    while (mask != 0ULL) {
        const uint64_t bit = config::bit_lsb(mask);
        mask = config::bit_clear_lsb_u64(mask);

        int row1_live = 0;
        int row2_live = 0;
        int col1_live = 0;
        int col2_live = 0;

        for (int i = 0; i < row1_target_count; ++i) {
            const int idx = row1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++row1_live;
        }
        for (int i = 0; i < row2_target_count; ++i) {
            const int idx = row2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++row2_live;
        }
        for (int i = 0; i < col1_target_count; ++i) {
            const int idx = col1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++col1_live;
        }
        for (int i = 0; i < col2_target_count; ++i) {
            const int idx = col2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) ++col2_live;
        }

        if (row1_live > 0 && row2_live > 0 && col1_live == 0 && col2_live == 0) {
            int allowed[64]{};
            int allowed_count = 0;
            for (int i = 0; i < row1_target_count; ++i) {
                const int idx = row1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < row2_target_count; ++i) {
                const int idx = row2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, scope, scope_count, allowed, allowed_count);
            if (er != ApplyResult::NoProgress) return er;
            continue;
        }

        if (col1_live > 0 && col2_live > 0 && row1_live == 0 && row2_live == 0) {
            int allowed[64]{};
            int allowed_count = 0;
            for (int i = 0; i < col1_target_count; ++i) {
                const int idx = col1_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            for (int i = 0; i < col2_target_count; ++i) {
                const int idx = col2_targets[i];
                if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                    exocet_push_unique_cell(allowed, allowed_count, idx);
                }
            }
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, scope, scope_count, allowed, allowed_count);
            if (er != ApplyResult::NoProgress) return er;
        }
    }

    return ApplyResult::NoProgress;
}

inline void exocet_push_live_targets_for_bit(
    const CandidateState& st,
    uint64_t bit,
    const int* targets,
    int target_count,
    int* allowed,
    int& allowed_count) {
    for (int i = 0; i < target_count; ++i) {
        const int idx = targets[i];
        if (st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
        exocet_push_unique_cell(allowed, allowed_count, idx);
    }
}

inline ApplyResult exocet_apply_senior_witness_target_placement(
    CandidateState& st,
    int base1,
    int base2,
    int t1,
    int t2,
    uint64_t target_union,
    const int* row1_targets,
    int row1_target_count,
    const int* row2_targets,
    int row2_target_count,
    const int* col1_targets,
    int col1_target_count,
    const int* col2_targets,
    int col2_target_count) {
    if (target_union == 0ULL) return ApplyResult::NoProgress;

    int scope[64]{};
    const int scope_count = exocet_collect_scope_cells(st, base1, base2, t1, t2, target_union, scope);

    uint64_t mask = target_union;
    while (mask != 0ULL) {
        const uint64_t bit = config::bit_lsb(mask);
        mask = config::bit_clear_lsb_u64(mask);

        int row1_live = 0, row2_live = 0, col1_live = 0, col2_live = 0;
        int row1_idx = -1, row2_idx = -1, col1_idx = -1, col2_idx = -1;

        for (int i = 0; i < row1_target_count; ++i) {
            const int idx = row1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++row1_live;
                row1_idx = idx;
            }
        }
        for (int i = 0; i < row2_target_count; ++i) {
            const int idx = row2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++row2_live;
                row2_idx = idx;
            }
        }
        for (int i = 0; i < col1_target_count; ++i) {
            const int idx = col1_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++col1_live;
                col1_idx = idx;
            }
        }
        for (int i = 0; i < col2_target_count; ++i) {
            const int idx = col2_targets[i];
            if (st.board->values[idx] == 0 && (st.cands[idx] & bit) != 0ULL) {
                ++col2_live;
                col2_idx = idx;
            }
        }

        int allowed[64]{};
        int allowed_count = 0;
        bool matched = false;

        if (row1_live == 1 && col2_live == 1 && row2_live == 0 && col1_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            matched = true;
        } else if (row1_live == 1 && col1_live == 1 && row2_live == 0 && col2_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live == 1 && col1_live == 1 && row1_live == 0 && col2_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live == 1 && col2_live == 1 && row1_live == 0 && col1_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live == 1 && row2_live == 1 && col1_live == 0 && col2_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col1_live == 1 && col2_live == 1 && row1_live == 0 && row2_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live == 1 && row2_live == 1 && col1_live == 1 && col2_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live == 1 && row2_live == 1 && col2_live == 1 && col1_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live == 1 && col1_live == 1 && col2_live == 1 && row2_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live == 1 && col1_live == 1 && col2_live == 1 && row1_live == 0) {
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && row2_live == 1 && col2_live == 1 && col1_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row1_targets, row1_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && row2_live == 1 && col1_live == 1 && col2_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row1_targets, row1_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live > 0 && row1_live == 1 && col1_live == 1 && col2_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row2_targets, row2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live > 0 && row1_live == 1 && col2_live == 1 && col1_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row2_targets, row2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col1_live > 0 && row1_live == 1 && row2_live == 1 && col2_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, col1_targets, col1_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col1_live > 0 && row1_live == 1 && col2_live == 1 && row2_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, col1_targets, col1_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col2_live > 0 && row1_live == 1 && row2_live == 1 && col1_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, col2_targets, col2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col2_live > 0 && row2_live == 1 && col1_live == 1 && row1_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, col2_targets, col2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && col1_live > 0 && row2_live == 1 && col2_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row1_targets, row1_target_count, allowed, allowed_count);
            exocet_push_live_targets_for_bit(st, bit, col1_targets, col1_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && col2_live > 0 && row2_live == 1 && col1_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row1_targets, row1_target_count, allowed, allowed_count);
            exocet_push_live_targets_for_bit(st, bit, col2_targets, col2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live > 0 && col1_live > 0 && row1_live == 1 && col2_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row2_targets, row2_target_count, allowed, allowed_count);
            exocet_push_live_targets_for_bit(st, bit, col1_targets, col1_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row2_live > 0 && col2_live > 0 && row1_live == 1 && col1_live == 0) {
            exocet_push_live_targets_for_bit(st, bit, row2_targets, row2_target_count, allowed, allowed_count);
            exocet_push_live_targets_for_bit(st, bit, col2_targets, col2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live > 0 && row2_live > 0 && col1_live == 1 && col2_live == 1) {
            exocet_push_live_targets_for_bit(st, bit, row1_targets, row1_target_count, allowed, allowed_count);
            exocet_push_live_targets_for_bit(st, bit, row2_targets, row2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (col1_live > 0 && col2_live > 0 && row1_live == 1 && row2_live == 1) {
            exocet_push_live_targets_for_bit(st, bit, col1_targets, col1_target_count, allowed, allowed_count);
            exocet_push_live_targets_for_bit(st, bit, col2_targets, col2_target_count, allowed, allowed_count);
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        } else if (row1_live == 1 && row2_live == 1 && col1_live == 1 && col2_live == 1) {
            exocet_push_unique_cell(allowed, allowed_count, row1_idx);
            exocet_push_unique_cell(allowed, allowed_count, row2_idx);
            exocet_push_unique_cell(allowed, allowed_count, col1_idx);
            exocet_push_unique_cell(allowed, allowed_count, col2_idx);
            exocet_push_unique_cell(allowed, allowed_count, t1);
            exocet_push_unique_cell(allowed, allowed_count, t2);
            matched = true;
        }

        if (!matched || allowed_count == 0) continue;
        const ApplyResult er = exocet_eliminate_outside_allowed(st, bit, scope, scope_count, allowed, allowed_count);
        if (er != ApplyResult::NoProgress) return er;
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult exocet_apply_scope_confinements(
    CandidateState& st,
    int base1,
    int base2,
    int t1,
    int t2,
    uint64_t core,
    bool senior_mode) {
    const int n = st.topo->n;
    const int r1 = st.topo->cell_row[base1];
    const int r2 = st.topo->cell_row[base2];
    const int c1 = st.topo->cell_col[base1];
    const int c2 = st.topo->cell_col[base2];
    const int b = st.topo->cell_box[base1];

    const int row_h1 = r1;
    const int row_h2 = r2;
    const int col_h1 = n + c1;
    const int col_h2 = n + c2;
    const int box_h = 2 * n + b;
    const uint64_t target_union = (st.cands[t1] | st.cands[t2]) & ~core;

    int row1_cells[64]{};
    int row2_cells[64]{};
    int col1_cells[64]{};
    int col2_cells[64]{};
    const int box_cells[2] = {base1, base2};

    int row1_count = 0;
    int row2_count = 0;
    int col1_count = 0;
    int col2_count = 0;
    exocet_push_unique_cell(row1_cells, row1_count, base1);
    exocet_push_unique_cell(row2_cells, row2_count, base2);
    exocet_push_unique_cell(col1_cells, col1_count, base1);
    exocet_push_unique_cell(col2_cells, col2_count, base2);

    const uint64_t core_scope = core | target_union;
    row1_count += exocet_collect_line_targets(st, row_h1, b, core_scope, row1_cells + row1_count);
    row2_count += exocet_collect_line_targets(st, row_h2, b, core_scope, row2_cells + row2_count);
    col1_count += exocet_collect_line_targets(st, col_h1, b, core_scope, col1_cells + col1_count);
    col2_count += exocet_collect_line_targets(st, col_h2, b, core_scope, col2_cells + col2_count);
    exocet_push_unique_cell(row1_cells, row1_count, t1);
    exocet_push_unique_cell(row2_cells, row2_count, t2);
    exocet_push_unique_cell(col1_cells, col1_count, t2);
    exocet_push_unique_cell(col2_cells, col2_count, t1);

    const ApplyResult r1er = exocet_apply_house_confinement(st, row_h1, core, row1_cells, row1_count);
    if (r1er != ApplyResult::NoProgress) return r1er;
    const ApplyResult r2er = exocet_apply_house_confinement(st, row_h2, core, row2_cells, row2_count);
    if (r2er != ApplyResult::NoProgress) return r2er;
    const ApplyResult c1er = exocet_apply_house_confinement(st, col_h1, core, col1_cells, col1_count);
    if (c1er != ApplyResult::NoProgress) return c1er;
    const ApplyResult c2er = exocet_apply_house_confinement(st, col_h2, core, col2_cells, col2_count);
    if (c2er != ApplyResult::NoProgress) return c2er;
    const ApplyResult ber = exocet_apply_house_confinement(st, box_h, core, box_cells, 2);
    if (ber != ApplyResult::NoProgress) return ber;

    if (target_union != 0ULL) {
        int row1_targets[64]{};
        int row2_targets[64]{};
        int col1_targets[64]{};
        int col2_targets[64]{};
        int row1_target_count = exocet_collect_line_targets(st, row_h1, b, target_union, row1_targets);
        int row2_target_count = exocet_collect_line_targets(st, row_h2, b, target_union, row2_targets);
        int col1_target_count = exocet_collect_line_targets(st, col_h1, b, target_union, col1_targets);
        int col2_target_count = exocet_collect_line_targets(st, col_h2, b, target_union, col2_targets);
        exocet_push_unique_cell(row1_targets, row1_target_count, t1);
        exocet_push_unique_cell(row2_targets, row2_target_count, t2);
        exocet_push_unique_cell(col1_targets, col1_target_count, t2);
        exocet_push_unique_cell(col2_targets, col2_target_count, t1);

        const ApplyResult typed_er = exocet_apply_target_typing(
            st, base1, base2, t1, t2, target_union,
            row1_targets, row1_target_count,
            row2_targets, row2_target_count,
            col1_targets, col1_target_count,
            col2_targets, col2_target_count);
        if (typed_er != ApplyResult::NoProgress) return typed_er;

        if (senior_mode) {
            const ApplyResult senior_typed_er = exocet_apply_senior_companion_typing(
                st, base1, base2, t1, t2, target_union,
                row1_targets, row1_target_count,
                row2_targets, row2_target_count,
                col1_targets, col1_target_count,
                col2_targets, col2_target_count);
            if (senior_typed_er != ApplyResult::NoProgress) return senior_typed_er;

            const ApplyResult cross_line_er = exocet_apply_senior_cross_line_restriction(
                st, base1, base2, t1, t2, target_union,
                row1_targets, row1_target_count,
                row2_targets, row2_target_count,
                col1_targets, col1_target_count,
                col2_targets, col2_target_count);
            if (cross_line_er != ApplyResult::NoProgress) return cross_line_er;

            const ApplyResult placement_er = exocet_apply_senior_witness_target_placement(
                st, base1, base2, t1, t2, target_union,
                row1_targets, row1_target_count,
                row2_targets, row2_target_count,
                col1_targets, col1_target_count,
                col2_targets, col2_target_count);
            if (placement_er != ApplyResult::NoProgress) return placement_er;

            const ApplyResult tr1 = exocet_apply_house_confinement(st, row_h1, target_union, row1_targets, row1_target_count);
            if (tr1 != ApplyResult::NoProgress) return tr1;
            const ApplyResult tr2 = exocet_apply_house_confinement(st, row_h2, target_union, row2_targets, row2_target_count);
            if (tr2 != ApplyResult::NoProgress) return tr2;
            const ApplyResult tc1 = exocet_apply_house_confinement(st, col_h1, target_union, col1_targets, col1_target_count);
            if (tc1 != ApplyResult::NoProgress) return tc1;
            const ApplyResult tc2 = exocet_apply_house_confinement(st, col_h2, target_union, col2_targets, col2_target_count);
            if (tc2 != ApplyResult::NoProgress) return tc2;
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult exocet_structural_probe(
    CandidateState& st,
    int pattern_cap,
    int max_steps,
    bool senior_mode) {
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        return ApplyResult::NoProgress;
    }

    const int n = st.topo->n;
    auto& sp = shared::exact_pattern_scratchpad();
    int patterns = 0;
    int box_cells[64]{};

    for (int b = 0; b < n; ++b) {
        const int h = 2 * n + b;
        const int p0 = st.topo->house_offsets[h];
        const int p1 = st.topo->house_offsets[h + 1];
        int bc = 0;

        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            if (st.board->values[idx] != 0) continue;
            const int pc = std::popcount(st.cands[idx]);
            if (pc < 2 || pc > 6) continue;
            if (bc < 64) box_cells[bc++] = idx;
        }
        if (bc < 2) continue;

        for (int i = 0; i < bc; ++i) {
            const int base1 = box_cells[i];
            const int r1 = st.topo->cell_row[base1];
            const int c1 = st.topo->cell_col[base1];
            const uint64_t m1 = st.cands[base1];

            for (int j = i + 1; j < bc; ++j) {
                if (patterns >= pattern_cap) return ApplyResult::NoProgress;
                const int base2 = box_cells[j];
                const int r2 = st.topo->cell_row[base2];
                const int c2 = st.topo->cell_col[base2];
                if (r1 == r2 || c1 == c2) continue;

                const uint64_t m2 = st.cands[base2];
                const uint64_t core = m1 & m2;
                if (std::popcount(core) < 2) continue;

                const int t1 = r1 * n + c2;
                const int t2 = r2 * n + c1;
                if (st.board->values[t1] != 0 || st.board->values[t2] != 0) continue;

                const uint64_t mt1 = st.cands[t1];
                const uint64_t mt2 = st.cands[t2];
                const uint64_t target_union = (mt1 | mt2) & ~core;
                if ((mt1 & core) == 0ULL || (mt2 & core) == 0ULL) continue;

                const int extra_pc = std::popcount(target_union);
                if (extra_pc > (senior_mode ? 6 : 4)) continue;

                ++patterns;
                const ApplyResult direct_er = exocet_apply_scope_confinements(
                    st, base1, base2, t1, t2, core, senior_mode);
                if (direct_er != ApplyResult::NoProgress) return direct_er;

                int scope[64]{};
                const int scope_count = exocet_collect_scope_cells(st, base1, base2, t1, t2, core | target_union, scope);
                for (int s = 0; s < scope_count; ++s) {
                    const int idx = scope[s];
                    uint64_t probe = st.cands[idx];
                    const int per_cell_budget = senior_mode ? 3 : 2;
                    int tested = 0;

                    while (probe != 0ULL && tested < per_cell_budget) {
                        const uint64_t bit = config::bit_lsb(probe);
                        probe = config::bit_clear_lsb_u64(probe);
                        ++tested;
                        const int d = config::bit_ctz_u64(bit) + 1;
                        if (!exocet_probe_candidate_contradiction(st, idx, d, max_steps, sp)) continue;

                        const ApplyResult er = st.eliminate(idx, bit);
                        if (er == ApplyResult::Contradiction) return er;
                        if (er == ApplyResult::Progress) return er;
                    }
                }
            }
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_exocet_exact(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (p8_board_too_dense(RequiredStrategy::Exocet, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int pattern_cap = std::clamp(4 + n / 3, 6, 16);
    const int max_steps = std::clamp(6 + n / 4, 8, 14);

    const ApplyResult ar = exocet_structural_probe(st, pattern_cap, max_steps, false);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_exocet(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (p8_board_too_dense(RequiredStrategy::Exocet, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    ApplyResult ar = apply_exocet_exact(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    // Usunięta bramka `required_exact_strategy_active` - 
    // Fallbacki działają dla Generatora Szablonów i oczyszczają planszę z szumu.
    
    bool used = false;
    const int depth_cap = std::clamp(8 + st.board->empty_cells / std::max(1, n), 10, 16);
    ar = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = p7_nightmare::apply_grouped_aic(st, tmp, r);
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

inline ApplyResult apply_senior_exocet(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (p8_board_too_dense(RequiredStrategy::SeniorExocet, n, nn, st.board->empty_cells)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    const int pattern_cap = std::clamp(6 + n / 2, 8, 22);
    const int max_steps = std::clamp(10 + n / 3, 12, 20);

    ApplyResult ar = exocet_structural_probe(st, pattern_cap, max_steps, true);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_senior_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    // Usunięta bramka `required_exact_strategy_active` - Fallbacki P8 przywrócone
    
    bool used = false;
    const int depth_cap = std::clamp(12 + st.board->empty_cells / std::max(1, n), 14, 24);
    ar = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = apply_exocet(st, tmp, r);
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