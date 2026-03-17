// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: required_strategy_gate.h
// Opis: Bramka sterująca korytarzem weryfikacyjnym dla Certyfikatora.
//       Odpowiada za dynamiczne wygłuszanie (Dynamic Suppression) generycznych 
//       łańcuchów proxy, gdy aktywny jest wstrzyknięty exact-template z P6/P7.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <cstdint>

#include "../../config/run_config.h"

namespace sudoku_hpc::logic::shared {

inline thread_local RequiredStrategy tls_required_exact_strategy = RequiredStrategy::None;
inline thread_local bool tls_required_named_priority = false;
inline thread_local bool tls_required_generic_suppression = false;
inline thread_local bool tls_required_pattern_anchor_protection = false;
inline thread_local bool tls_required_pattern_single_protection = false;
inline thread_local bool tls_required_dual_clue_policy = false;
inline thread_local bool tls_required_certifier_sparsify = false;
inline thread_local bool tls_required_generator_dense_seed = false;

inline RequiredStrategy current_required_exact_strategy() {
    return tls_required_exact_strategy;
}

inline RequiredStrategy current_required_named_strategy() {
    return tls_required_exact_strategy;
}

inline bool required_exact_strategy_active(RequiredStrategy rs) {
    return rs != RequiredStrategy::None && tls_required_exact_strategy == rs;
}

inline bool required_strategy_corridor_active() {
    return tls_required_exact_strategy != RequiredStrategy::None;
}

inline bool required_named_priority_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_named_priority;
}

inline bool required_named_priority_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_named_priority;
}

// Główna flaga sprawdzana przez Dyspozytor do wygłuszania Proxy Chains (AIC, Forcing Chains, X/Y-Chains)
inline bool required_generic_family_suppression_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_generic_suppression;
}

inline bool required_generic_family_suppression_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_generic_suppression;
}

inline bool required_pattern_anchor_protection_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_pattern_anchor_protection;
}

inline bool required_pattern_anchor_protection_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_pattern_anchor_protection;
}

inline bool required_pattern_single_protection_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_pattern_single_protection;
}

inline bool required_pattern_single_protection_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_pattern_single_protection;
}

inline bool required_dual_clue_policy_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_dual_clue_policy;
}

inline bool required_dual_clue_policy_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_dual_clue_policy;
}

inline bool required_certifier_sparsify_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_certifier_sparsify;
}

inline bool required_certifier_sparsify_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_certifier_sparsify;
}

inline bool required_generator_dense_seed_active() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_generator_dense_seed;
}

inline bool required_generator_dense_seed_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_generator_dense_seed;
}

inline bool required_strategy_prefers_named_before_generic() {
    const RequiredStrategy rs = tls_required_exact_strategy;
    return rs != RequiredStrategy::None && tls_required_named_priority &&
           strategy_prefers_named_structures_before_generic(rs);
}

inline bool required_strategy_prefers_named_before_generic(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_named_priority &&
           strategy_prefers_named_structures_before_generic(rs);
}

inline bool required_strategy_suppresses_equivalent_generics() {
    const RequiredStrategy rs = tls_required_exact_strategy;
    return rs != RequiredStrategy::None && tls_required_generic_suppression &&
           (strategy_suppress_equivalent_generic_families(rs) || strategy_requires_exact_only(rs));
}

inline bool required_strategy_suppresses_equivalent_generics(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_generic_suppression &&
           (strategy_suppress_equivalent_generic_families(rs) || strategy_requires_exact_only(rs));
}

inline bool required_strategy_is_named_sensitive() {
    const RequiredStrategy rs = tls_required_exact_strategy;
    if (rs == RequiredStrategy::None) {
        return false;
    }
    return strategy_prefers_named_structures_before_generic(rs) ||
           strategy_suppress_equivalent_generic_families(rs) ||
           strategy_requires_exact_only(rs);
}

inline bool required_strategy_is_named_sensitive(RequiredStrategy rs) {
    if (rs == RequiredStrategy::None) {
        return false;
    }
    return required_exact_strategy_active(rs) &&
           (strategy_prefers_named_structures_before_generic(rs) ||
            strategy_suppress_equivalent_generic_families(rs) ||
            strategy_requires_exact_only(rs));
}

inline bool required_strategy_protect_from_singles() {
    return tls_required_exact_strategy != RequiredStrategy::None && tls_required_pattern_single_protection;
}

inline bool required_strategy_protect_from_singles(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && tls_required_pattern_single_protection;
}

inline bool required_strategy_exact_only_active() {
    const RequiredStrategy rs = tls_required_exact_strategy;
    return rs != RequiredStrategy::None && strategy_requires_exact_only(rs);
}

inline bool required_strategy_exact_only_active(RequiredStrategy rs) {
    return required_exact_strategy_active(rs) && strategy_requires_exact_only(rs);
}

inline bool required_strategy_should_open_named_corridor(RequiredStrategy rs, const GenerateRunConfig& cfg) {
    if (rs == RequiredStrategy::None) {
        return false;
    }

    if (cfg.prefer_required_named_first && strategy_prefers_named_structures_before_generic(rs)) {
        return true;
    }
    if (cfg.suppress_equivalent_generics_for_required && 
       (strategy_suppress_equivalent_generic_families(rs) || strategy_requires_exact_only(rs))) {
        return true;
    }
    if (strategy_requires_exact_only(rs)) {
        return true;
    }
    return false;
}

inline bool required_strategy_should_open_named_corridor(RequiredStrategy rs) {
    if (rs == RequiredStrategy::None) {
        return false;
    }
    return strategy_prefers_named_structures_before_generic(rs) ||
           strategy_suppress_equivalent_generic_families(rs) ||
           strategy_requires_exact_only(rs);
}

inline bool required_strategy_should_protect_pattern(RequiredStrategy rs, const GenerateRunConfig& cfg) {
    if (rs == RequiredStrategy::None) {
        return false;
    }
    return (cfg.preserve_required_pattern_anchors && strategy_requires_exact_only(rs)) ||
           cfg.protect_required_pattern_from_singles ||
           cfg.generator_dense_seed_for_required_named ||
           cfg.certifier_sparsify_for_required_named;
}

inline bool required_strategy_should_protect_pattern(RequiredStrategy rs) {
    if (rs == RequiredStrategy::None) {
        return false;
    }
    return strategy_requires_exact_only(rs) ||
           strategy_prefers_named_structures_before_generic(rs) ||
           strategy_suppress_equivalent_generic_families(rs);
}

struct RequiredExactStrategyScope {
    explicit RequiredExactStrategyScope(RequiredStrategy rs)
        : previous_strategy(tls_required_exact_strategy),
          previous_named_priority(tls_required_named_priority),
          previous_generic_suppression(tls_required_generic_suppression),
          previous_anchor_protection(tls_required_pattern_anchor_protection),
          previous_single_protection(tls_required_pattern_single_protection),
          previous_dual_clue_policy(tls_required_dual_clue_policy),
          previous_certifier_sparsify(tls_required_certifier_sparsify),
          previous_generator_dense_seed(tls_required_generator_dense_seed) {
        
        if (rs == RequiredStrategy::None) {
            tls_required_exact_strategy = RequiredStrategy::None;
            tls_required_named_priority = false;
            tls_required_generic_suppression = false;
            tls_required_pattern_anchor_protection = false;
            tls_required_pattern_single_protection = false;
            tls_required_dual_clue_policy = false;
            tls_required_certifier_sparsify = false;
            tls_required_generator_dense_seed = false;
            return;
        }

        tls_required_exact_strategy = rs;
        tls_required_named_priority = strategy_prefers_named_structures_before_generic(rs);
        
        // Zabezpieczenie wygłuszania: jeżeli to wzorzec Named/Exact, dynamicznie tniemy łańcuchy Proxy
        tls_required_generic_suppression = strategy_suppress_equivalent_generic_families(rs) || 
                                           strategy_requires_exact_only(rs);
                                           
        tls_required_pattern_anchor_protection = required_strategy_should_protect_pattern(rs);
        tls_required_pattern_single_protection = strategy_prefers_named_structures_before_generic(rs) ||
                                                 strategy_requires_exact_only(rs);
        tls_required_dual_clue_policy = strategy_prefers_generator_certifier_split(rs);
        tls_required_certifier_sparsify = strategy_prefers_generator_certifier_split(rs);
        tls_required_generator_dense_seed = strategy_prefers_generator_certifier_split(rs) ||
                                            strategy_prefers_named_structures_before_generic(rs);
    }

    RequiredExactStrategyScope(RequiredStrategy rs, const GenerateRunConfig& cfg)
        : previous_strategy(tls_required_exact_strategy),
          previous_named_priority(tls_required_named_priority),
          previous_generic_suppression(tls_required_generic_suppression),
          previous_anchor_protection(tls_required_pattern_anchor_protection),
          previous_single_protection(tls_required_pattern_single_protection),
          previous_dual_clue_policy(tls_required_dual_clue_policy),
          previous_certifier_sparsify(tls_required_certifier_sparsify),
          previous_generator_dense_seed(tls_required_generator_dense_seed) {
        
        if (rs == RequiredStrategy::None) {
            tls_required_exact_strategy = RequiredStrategy::None;
            tls_required_named_priority = false;
            tls_required_generic_suppression = false;
            tls_required_pattern_anchor_protection = false;
            tls_required_pattern_single_protection = false;
            tls_required_dual_clue_policy = false;
            tls_required_certifier_sparsify = false;
            tls_required_generator_dense_seed = false;
            return;
        }

        tls_required_exact_strategy = rs;
        tls_required_named_priority = cfg.prefer_required_named_first &&
                                      strategy_prefers_named_structures_before_generic(rs);
                                      
        // Uruchomienie twardego wyciszenia P6/P7 (Dynamic Mute) zgodnie z konfiguracją
        tls_required_generic_suppression = cfg.suppress_equivalent_generics_for_required &&
                                           (strategy_suppress_equivalent_generic_families(rs) || 
                                            strategy_requires_exact_only(rs));
                                           
        tls_required_pattern_anchor_protection = cfg.preserve_required_pattern_anchors &&
                                                 required_strategy_should_protect_pattern(rs, cfg);
        tls_required_pattern_single_protection = cfg.protect_required_pattern_from_singles &&
                                                 required_strategy_should_protect_pattern(rs, cfg);
        tls_required_dual_clue_policy = cfg.dual_clue_policy_enforced &&
                                        strategy_prefers_generator_certifier_split(rs);
        tls_required_certifier_sparsify = cfg.certifier_sparsify_for_required_named &&
                                          strategy_prefers_generator_certifier_split(rs);
        tls_required_generator_dense_seed = cfg.generator_dense_seed_for_required_named &&
                                            (strategy_prefers_generator_certifier_split(rs) ||
                                             strategy_prefers_named_structures_before_generic(rs));
    }

    ~RequiredExactStrategyScope() {
        tls_required_exact_strategy = previous_strategy;
        tls_required_named_priority = previous_named_priority;
        tls_required_generic_suppression = previous_generic_suppression;
        tls_required_pattern_anchor_protection = previous_anchor_protection;
        tls_required_pattern_single_protection = previous_single_protection;
        tls_required_dual_clue_policy = previous_dual_clue_policy;
        tls_required_certifier_sparsify = previous_certifier_sparsify;
        tls_required_generator_dense_seed = previous_generator_dense_seed;
    }

    RequiredExactStrategyScope(const RequiredExactStrategyScope&) = delete;
    RequiredExactStrategyScope& operator=(const RequiredExactStrategyScope&) = delete;

private:
    RequiredStrategy previous_strategy;
    bool previous_named_priority;
    bool previous_generic_suppression;
    bool previous_anchor_protection;
    bool previous_single_protection;
    bool previous_dual_clue_policy;
    bool previous_certifier_sparsify;
    bool previous_generator_dense_seed;
};

} // namespace sudoku_hpc::logic::shared