// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: sudoku_logic_engine.h
// Opis: Centralny dyspozytor logiki (Fasada). Deleguje testy łamigłówki 
//       do poszczególnych modułów. Łączy nową, rozbitą architekturę w całość.
//       Gwarantuje wywołania strategii od Poziomu 1 (Easy) do Poziomu 8 
//       (Theoretical) bez pominięcia żadnego wariantu. Zero-Allocation.
//       Przebudowany rygorystyczny Dispatch Order ("Named Structures First").
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "../config/run_config.h"
#include "../core/board.h"
#include "../core/candidate_state.h"
#include "../generator/core_engines/dlx_solver.h" // dla SearchAbortControl
#include "../generator/pattern_forcing/pattern_planter.h"
#include "logic_result.h"
#include "shared/required_strategy_gate.h"

// ============================================================================
// DOŁĄCZENIE WSZYSTKICH SPECJALISTYCZNYCH MODUŁÓW (P1 - P8)
// ============================================================================
#include "p1_easy/naked_hidden_single.h"
#include "p2_intersections/intersections.h"
#include "p3_subsets/house_subsets.h"
#include "p4_hard/fish_basic.h"
#include "p4_hard/skyscraper_kite.h"
#include "p4_hard/empty_rectangle.h"
#include "p4_hard/y_wing_remote_pairs.h"
#include "p5_expert/finned_fish.h"
#include "p5_expert/simple_coloring.h"
#include "p5_expert/bug_plus_one.h"
#include "p5_expert/unique_rectangle_t1.h"
#include "p5_expert/xyz_w_wing.h"
#include "p6_diabolical/finned_jelly_sword.h" // Zawiera Jellyfish
#include "p6_diabolical/chains_basic.h"
#include "p6_diabolical/wxyz_wing.h"
#include "p6_diabolical/als_xz.h"
#include "p6_diabolical/unique_loop_avoidable_oddagon.h" // Nowe uzupełnienia
#include "p6_diabolical/ur_extended.h"
#include "p6_diabolical/bug_variants.h"
#include "p6_diabolical/borescoper_qiu.h"
#include "p7_nightmare/medusa_3d.h"
#include "p7_nightmare/aic_grouped_aic.h"
#include "p7_nightmare/grouped_x_cycle.h"
#include "p7_nightmare/continuous_nice_loop.h"
#include "p7_nightmare/als_xy_wing_chain.h" // Zawiera ALS-AIC
#include "p7_nightmare/sue_de_coq.h"
#include "p7_nightmare/death_blossom.h"
#include "p7_nightmare/franken_mutant_fish.h"
#include "p7_nightmare/kraken_fish.h"
#include "p7_nightmare/squirmbag.h" // Nowa ryba 5x5
#include "p7_nightmare/aligned_exclusion.h"
#include "p8_theoretical/msls.h"
#include "p8_theoretical/exocet_family.h"
#include "p8_theoretical/sk_loop.h"
#include "p8_theoretical/pattern_overlay.h"
#include "p8_theoretical/forcing_chains_dynamic.h"

namespace sudoku_hpc::logic {

struct GenericLogicCertify {
    enum StrategySlot : size_t {
        SlotNakedSingle = 0,
        SlotHiddenSingle = 1,
        SlotPointingPairs = 2,
        SlotBoxLineReduction = 3,
        SlotNakedPair = 4,
        SlotHiddenPair = 5,
        SlotNakedTriple = 6,
        SlotHiddenTriple = 7,
        SlotNakedQuad = 8,
        SlotHiddenQuad = 9,
        SlotXWing = 10,
        SlotYWing = 11,
        SlotSkyscraper = 12,
        SlotTwoStringKite = 13,
        SlotEmptyRectangle = 14,
        SlotRemotePairs = 15,
        SlotSwordfish = 16,
        SlotFinnedXWingSashimi = 17,
        SlotSimpleColoring = 18,
        SlotBUGPlusOne = 19,
        SlotUniqueRectangle = 20,
        SlotXYZWing = 21,
        SlotWWing = 22,
        SlotJellyfish = 23,
        SlotXChain = 24,
        SlotXYChain = 25,
        SlotWXYZWing = 26,
        SlotFinnedSwordfishJellyfish = 27,
        SlotALSXZ = 28,
        SlotUniqueLoop = 29,
        SlotAvoidableRectangle = 30,
        SlotBivalueOddagon = 31,
        SlotMedusa3D = 32,
        SlotAIC = 33,
        SlotGroupedAIC = 34,
        SlotGroupedXCycle = 35,
        SlotContinuousNiceLoop = 36,
        SlotALSXYWing = 37,
        SlotALSChain = 38,
        SlotSueDeCoq = 39,
        SlotDeathBlossom = 40,
        SlotFrankenFish = 41,
        SlotMutantFish = 42,
        SlotKrakenFish = 43,
        SlotMSLS = 44,
        SlotExocet = 45,
        SlotSeniorExocet = 46,
        SlotSKLoop = 47,
        SlotPatternOverlayMethod = 48,
        SlotForcingChains = 49,
        SlotSquirmbag = 50,
        SlotURExtended = 51,
        SlotHiddenUR = 52,
        SlotBUGType2 = 53,
        SlotBUGType3 = 54,
        SlotBUGType4 = 55,
        SlotBorescoperQiuDeadlyPattern = 56,
        SlotAlignedPairExclusion = 57,
        SlotAlignedTripleExclusion = 58,
        SlotALSAIC = 59,
        SlotDynamicForcingChains = 60
    };

    struct StrategyMeta {
        const char* id = "unknown";
        uint8_t level = 0;
        StrategyImplTier impl_tier = StrategyImplTier::Disabled;
        bool supports_asymmetric = true;
        uint8_t max_n_verified = 64;
        const char* proof_tag = "none";
        StrategyCoverageGrade coverage_grade = StrategyCoverageGrade::TextbookFull;
        PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;
        StrategyZeroAllocGrade zero_alloc_grade = StrategyZeroAllocGrade::HotpathZeroAllocOk;
        StrategyAuditDecision audit_decision = StrategyAuditDecision::Keep;
        bool asymmetry_verified = false;
    };

    struct StrategyAuditRow {
        size_t slot = 0;
        RequiredStrategy required_strategy = RequiredStrategy::None;
        const char* id = "unknown";
        uint8_t level = 0;
        StrategyImplTier impl_tier = StrategyImplTier::Disabled;
        bool supports_asymmetric = false;
        bool asymmetry_verified = false;
        uint8_t max_n_verified = 0;
        const char* proof_tag = "none";
        StrategyCoverageGrade coverage_grade = StrategyCoverageGrade::Untested;
        PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;
        StrategyZeroAllocGrade zero_alloc_grade = StrategyZeroAllocGrade::NeedsScratchpadRefactor;
        StrategyAuditDecision audit_decision = StrategyAuditDecision::Rewrite;
        bool parser_selectable = false;
        bool certifier_wired = false;
        bool mcts_dispatch_wired = false;
        bool generator_dispatch_wired = false;
        bool generator_exact_template_wired = false;
        bool generator_family_fallback_wired = false;
        bool smoke_profile_present = false;
        bool asymmetric_smoke_profile_present = false;
        bool canonical_full = false;
        bool proxy_or_disabled = true;
        bool family_fallback_only = false;
        bool exact_required_but_missing = false;
        bool exact_contract_required = false;
    };

    struct StrategyAuditSummary {
        size_t total_slots = 0;
        size_t canonical_full = 0;
        size_t proxy_or_disabled = 0;
        size_t asymmetry_verified = 0;
        size_t textbook_full = 0;
        size_t family_approx = 0;
        size_t partial = 0;
        size_t wired_only = 0;
        size_t untested = 0;
        size_t exact_required = 0;
        size_t exact_preferred_fallback_family = 0;
        size_t family_only = 0;
        size_t unsupported = 0;
        size_t hotpath_zero_alloc_ok = 0;
        size_t tls_alloc_only = 0;
        size_t vector_in_hotpath = 0;
        size_t needs_scratchpad_refactor = 0;
        size_t keep = 0;
        size_t tighten = 0;
        size_t rewrite = 0;
        size_t exact_template_missing = 0;
        size_t parser_selectable = 0;
        size_t certifier_wired = 0;
        size_t mcts_dispatch_wired = 0;
        size_t generator_dispatch_wired = 0;
        size_t generator_exact_template_wired = 0;
        size_t generator_family_fallback_wired = 0;
        size_t smoke_profile_present = 0;
        size_t asymmetric_smoke_profile_present = 0;
        size_t family_fallback_only = 0;
        size_t exact_required_but_missing = 0;
        size_t rewrite_candidates = 0;
        size_t tighten_candidates = 0;
    };

    static constexpr size_t strategy_slot_count() {
        return kStrategySlotCount;
    }

    static const StrategyMeta& strategy_meta_for_slot(size_t slot) {
        static constexpr std::array<StrategyMeta, kStrategySlotCount> kMeta = {{
            {"NakedSingle", 1, StrategyImplTier::Full, true, 64, "P1.NakedSingle",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"HiddenSingle", 1, StrategyImplTier::Full, true, 64, "P1.HiddenSingle",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"PointingPairs", 2, StrategyImplTier::Full, true, 64, "P2.PointingPairs",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BoxLineReduction", 2, StrategyImplTier::Full, true, 64, "P2.BoxLineReduction",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"NakedPair", 2, StrategyImplTier::Full, true, 64, "P2.NakedPair",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"HiddenPair", 2, StrategyImplTier::Full, true, 64, "P2.HiddenPair",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"NakedTriple", 2, StrategyImplTier::Full, true, 64, "P2.NakedTriple",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"HiddenTriple", 2, StrategyImplTier::Full, true, 64, "P2.HiddenTriple",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"NakedQuad", 3, StrategyImplTier::Full, true, 64, "P3.NakedQuad",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"HiddenQuad", 3, StrategyImplTier::Full, true, 64, "P3.HiddenQuad",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"XWing", 3, StrategyImplTier::Full, true, 64, "P3.XWing",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"YWing", 3, StrategyImplTier::Full, true, 64, "P3.YWing",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"Skyscraper", 3, StrategyImplTier::Full, true, 64, "P3.Skyscraper",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"TwoStringKite", 3, StrategyImplTier::Full, true, 64, "P3.TwoStringKite",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"EmptyRectangle", 3, StrategyImplTier::Full, true, 64, "P3.EmptyRectangle",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"RemotePairs", 3, StrategyImplTier::Full, true, 64, "P3.RemotePairs",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"Swordfish", 4, StrategyImplTier::Full, true, 64, "P4.Swordfish",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"FinnedXWingSashimi", 4, StrategyImplTier::Full, true, 64, "P4.FinnedXWingSashimi",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"SimpleColoring", 4, StrategyImplTier::Full, true, 64, "P4.SimpleColoring",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BUGPlusOne", 4, StrategyImplTier::Full, true, 64, "P4.BUGPlusOne",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"UniqueRectangle", 4, StrategyImplTier::Full, true, 64, "P4.UniqueRectangle",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"XYZWing", 4, StrategyImplTier::Full, true, 64, "P4.XYZWing",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"WWing", 4, StrategyImplTier::Full, true, 64, "P4.WWing",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"Jellyfish", 5, StrategyImplTier::Full, true, 64, "P5.Jellyfish",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"XChain", 5, StrategyImplTier::Full, true, 64, "P5.XChain",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"XYChain", 5, StrategyImplTier::Full, true, 64, "P5.XYChain",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"WXYZWing", 5, StrategyImplTier::Full, true, 64, "P5.WXYZWing",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"FinnedSwordfishJellyfish", 5, StrategyImplTier::Full, true, 64, "P5.FinnedSwordfishJellyfish",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"ALSXZ", 5, StrategyImplTier::Full, true, 64, "P5.ALSXZ",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"UniqueLoop", 5, StrategyImplTier::Full, true, 64, "P5.UniqueLoop",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"AvoidableRectangle", 5, StrategyImplTier::Full, true, 64, "P5.AvoidableRectangle",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BivalueOddagon", 5, StrategyImplTier::Full, true, 64, "P5.BivalueOddagon",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"Medusa3D", 6, StrategyImplTier::Full, true, 64, "P6.Medusa3D",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"AIC", 6, StrategyImplTier::Full, true, 64, "P6.AIC",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"GroupedAIC", 6, StrategyImplTier::Full, true, 64, "P6.GroupedAIC",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"GroupedXCycle", 6, StrategyImplTier::Full, true, 64, "P6.GroupedXCycle",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"ContinuousNiceLoop", 6, StrategyImplTier::Full, true, 64, "P6.ContinuousNiceLoop",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"ALSXYWing", 6, StrategyImplTier::Full, true, 64, "P6.ALSXYWing",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"ALSChain", 6, StrategyImplTier::Full, true, 64, "P6.ALSChain",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"SueDeCoq", 6, StrategyImplTier::Full, true, 64, "P6.SueDeCoq",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"DeathBlossom", 6, StrategyImplTier::Full, true, 64, "P6.DeathBlossom",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"FrankenFish", 7, StrategyImplTier::Full, true, 64, "P6.FrankenFish",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"MutantFish", 7, StrategyImplTier::Full, true, 64, "P6.MutantFish",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"KrakenFish", 7, StrategyImplTier::Full, true, 64, "P6.KrakenFish",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"MSLS", 8, StrategyImplTier::Full, true, 64, "P7.MSLS",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"Exocet", 8, StrategyImplTier::Full, true, 64, "P7.Exocet",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"SeniorExocet", 8, StrategyImplTier::Full, true, 64, "P7.SeniorExocet",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"SKLoop", 8, StrategyImplTier::Full, true, 64, "P7.SKLoop",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"PatternOverlayMethod", 8, StrategyImplTier::Full, true, 64, "P7.PatternOverlay",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"ForcingChains", 7, StrategyImplTier::Full, true, 64, "P7.ForcingChains",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"Squirmbag", 7, StrategyImplTier::Full, true, 64, "P6.Squirmbag",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"URExtended", 6, StrategyImplTier::Full, true, 64, "P5.URExtended",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"HiddenUR", 6, StrategyImplTier::Full, true, 64, "P5.HiddenUR",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BUGType2", 5, StrategyImplTier::Full, true, 64, "P5.BUGType2",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BUGType3", 5, StrategyImplTier::Full, true, 64, "P5.BUGType3",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BUGType4", 5, StrategyImplTier::Full, true, 64, "P5.BUGType4",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"BorescoperQiuDeadlyPattern", 6, StrategyImplTier::Full, true, 64, "P5.BorescoperQiu",
                StrategyCoverageGrade::TextbookFull, PatternGeneratorPolicy::ExactPreferredFamilyFallback,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Keep, true},
            {"AlignedPairExclusion", 7, StrategyImplTier::Full, true, 64, "P6.AlignedPairExclusion",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"AlignedTripleExclusion", 7, StrategyImplTier::Full, true, 64, "P6.AlignedTripleExclusion",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"ALSAIC", 6, StrategyImplTier::Full, true, 64, "P6.ALSAIC",
                StrategyCoverageGrade::FamilyApprox, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
            {"DynamicForcingChains", 7, StrategyImplTier::Full, true, 64, "P7.DynamicForcingChains",
                StrategyCoverageGrade::Partial, PatternGeneratorPolicy::ExactRequired,
                StrategyZeroAllocGrade::HotpathZeroAllocOk, StrategyAuditDecision::Tighten, true},
        }};
        if (slot >= kMeta.size()) {
            static constexpr StrategyMeta kInvalid{"invalid", 0, StrategyImplTier::Disabled, false, 0, "invalid"};
            return kInvalid;
        }
        return kMeta[slot];
    }

    static StrategyImplTier strategy_impl_tier_for_slot(size_t slot) {
        return strategy_meta_for_slot(slot).impl_tier;
    }

    static RequiredStrategy required_strategy_for_slot(size_t slot) {
        static constexpr std::array<RequiredStrategy, kStrategySlotCount> kRequired = {{
            RequiredStrategy::NakedSingle,
            RequiredStrategy::HiddenSingle,
            RequiredStrategy::PointingPairs,
            RequiredStrategy::BoxLineReduction,
            RequiredStrategy::NakedPair,
            RequiredStrategy::HiddenPair,
            RequiredStrategy::NakedTriple,
            RequiredStrategy::HiddenTriple,
            RequiredStrategy::NakedQuad,
            RequiredStrategy::HiddenQuad,
            RequiredStrategy::XWing,
            RequiredStrategy::YWing,
            RequiredStrategy::Skyscraper,
            RequiredStrategy::TwoStringKite,
            RequiredStrategy::EmptyRectangle,
            RequiredStrategy::RemotePairs,
            RequiredStrategy::Swordfish,
            RequiredStrategy::FinnedXWingSashimi,
            RequiredStrategy::SimpleColoring,
            RequiredStrategy::BUGPlusOne,
            RequiredStrategy::UniqueRectangle,
            RequiredStrategy::XYZWing,
            RequiredStrategy::WWing,
            RequiredStrategy::Jellyfish,
            RequiredStrategy::XChain,
            RequiredStrategy::XYChain,
            RequiredStrategy::WXYZWing,
            RequiredStrategy::FinnedSwordfishJellyfish,
            RequiredStrategy::ALSXZ,
            RequiredStrategy::UniqueLoop,
            RequiredStrategy::AvoidableRectangle,
            RequiredStrategy::BivalueOddagon,
            RequiredStrategy::Medusa3D,
            RequiredStrategy::AIC,
            RequiredStrategy::GroupedAIC,
            RequiredStrategy::GroupedXCycle,
            RequiredStrategy::ContinuousNiceLoop,
            RequiredStrategy::ALSXYWing,
            RequiredStrategy::ALSChain,
            RequiredStrategy::SueDeCoq,
            RequiredStrategy::DeathBlossom,
            RequiredStrategy::FrankenFish,
            RequiredStrategy::MutantFish,
            RequiredStrategy::KrakenFish,
            RequiredStrategy::MSLS,
            RequiredStrategy::Exocet,
            RequiredStrategy::SeniorExocet,
            RequiredStrategy::SKLoop,
            RequiredStrategy::PatternOverlayMethod,
            RequiredStrategy::ForcingChains,
            RequiredStrategy::Squirmbag,
            RequiredStrategy::UniqueRectangleExtended,
            RequiredStrategy::HiddenUniqueRectangle,
            RequiredStrategy::BUGType2,
            RequiredStrategy::BUGType3,
            RequiredStrategy::BUGType4,
            RequiredStrategy::BorescoperQiuDeadlyPattern,
            RequiredStrategy::AlignedPairExclusion,
            RequiredStrategy::AlignedTripleExclusion,
            RequiredStrategy::ALSAIC,
            RequiredStrategy::DynamicForcingChains,
        }};
        if (slot >= kRequired.size()) {
            return RequiredStrategy::None;
        }
        return kRequired[slot];
    }

    static bool is_proxy_slot(size_t slot) {
        const StrategyImplTier tier = strategy_impl_tier_for_slot(slot);
        return tier == StrategyImplTier::Proxy || tier == StrategyImplTier::Disabled;
    }

    static bool is_full_canonical_slot(size_t slot) {
        const StrategyMeta& meta = strategy_meta_for_slot(slot);
        return meta.impl_tier == StrategyImplTier::Full &&
               meta.coverage_grade == StrategyCoverageGrade::TextbookFull &&
               meta.zero_alloc_grade == StrategyZeroAllocGrade::HotpathZeroAllocOk;
    }

    static StrategyAuditRow strategy_audit_row_for_slot(size_t slot) {
        const StrategyMeta& meta = strategy_meta_for_slot(slot);
        StrategyAuditRow row{};
        size_t mapped_slot = 0;
        RequiredStrategy parsed = RequiredStrategy::None;
        row.slot = slot;
        row.required_strategy = required_strategy_for_slot(slot);
        row.id = meta.id;
        row.level = meta.level;
        row.impl_tier = meta.impl_tier;
        row.supports_asymmetric = meta.supports_asymmetric;
        row.asymmetry_verified = meta.asymmetry_verified;
        row.max_n_verified = meta.max_n_verified;
        row.proof_tag = meta.proof_tag;
        row.coverage_grade = meta.coverage_grade;
        row.generator_policy = meta.generator_policy;
        row.zero_alloc_grade = meta.zero_alloc_grade;
        row.audit_decision = meta.audit_decision;
        row.canonical_full = is_full_canonical_slot(slot);
        row.proxy_or_disabled = is_proxy_slot(slot);
        row.exact_contract_required = (meta.generator_policy == PatternGeneratorPolicy::ExactRequired);
        row.parser_selectable =
            parse_required_strategy(to_string(row.required_strategy), parsed) &&
            parsed == row.required_strategy;
        const StrategySmokeProfile primary_smoke = primary_strategy_smoke_profile(row.required_strategy);
        const StrategySmokeProfile asymmetric_smoke = asymmetric_strategy_smoke_profile(row.required_strategy);
        if (row.parser_selectable && primary_smoke.enabled) {
            row.parser_selectable = required_strategy_selectable_for_geometry(
                row.required_strategy, primary_smoke.box_rows, primary_smoke.box_cols);
        }
        row.certifier_wired =
            slot_from_required_strategy(row.required_strategy, mapped_slot) && mapped_slot == slot;
        row.mcts_dispatch_wired = row.certifier_wired && row.required_strategy != RequiredStrategy::None;
        row.generator_dispatch_wired =
            pattern_forcing::pattern_dispatch_wired(row.required_strategy, meta.level);
        row.generator_exact_template_wired =
            pattern_forcing::pattern_exact_template_dispatch_wired(row.required_strategy, meta.level);
        row.generator_family_fallback_wired =
            pattern_forcing::pattern_family_fallback_dispatch_wired(row.required_strategy, meta.level);
        row.smoke_profile_present = primary_smoke.enabled;
        row.asymmetric_smoke_profile_present = row.supports_asymmetric && asymmetric_smoke.enabled;
        row.family_fallback_only =
            row.generator_family_fallback_wired && !row.generator_exact_template_wired;
        row.exact_required_but_missing =
            row.exact_contract_required && !row.generator_exact_template_wired;
        return row;
    }

    static bool strategy_audit_row_from_required_strategy(RequiredStrategy rs, StrategyAuditRow& out_row) {
        size_t slot = 0;
        if (!slot_from_required_strategy(rs, slot)) {
            return false;
        }
        out_row = strategy_audit_row_for_slot(slot);
        return true;
    }

    template <typename Fn>
    static void for_each_strategy_audit_row(Fn&& fn) {
        for (size_t slot = 0; slot < kStrategySlotCount; ++slot) {
            fn(strategy_audit_row_for_slot(slot));
        }
    }

    static StrategyAuditSummary build_audit_summary() {
        StrategyAuditSummary summary{};
        summary.total_slots = kStrategySlotCount;
        for_each_strategy_audit_row([&summary](const StrategyAuditRow& row) {
            if (row.canonical_full) ++summary.canonical_full;
            if (row.proxy_or_disabled) ++summary.proxy_or_disabled;
            if (row.asymmetry_verified) ++summary.asymmetry_verified;

            switch (row.coverage_grade) {
                case StrategyCoverageGrade::TextbookFull: ++summary.textbook_full; break;
                case StrategyCoverageGrade::FamilyApprox: ++summary.family_approx; break;
                case StrategyCoverageGrade::Partial: ++summary.partial; break;
                case StrategyCoverageGrade::WiredOnly: ++summary.wired_only; break;
                case StrategyCoverageGrade::Untested: ++summary.untested; break;
            }

            switch (row.generator_policy) {
                case PatternGeneratorPolicy::ExactRequired: ++summary.exact_required; break;
                case PatternGeneratorPolicy::ExactPreferredFamilyFallback: ++summary.exact_preferred_fallback_family; break;
                case PatternGeneratorPolicy::FamilyOnly: ++summary.family_only; break;
                case PatternGeneratorPolicy::Unsupported: ++summary.unsupported; break;
            }

            switch (row.zero_alloc_grade) {
                case StrategyZeroAllocGrade::HotpathZeroAllocOk: ++summary.hotpath_zero_alloc_ok; break;
                case StrategyZeroAllocGrade::TlsAllocOnly: ++summary.tls_alloc_only; break;
                case StrategyZeroAllocGrade::VectorInHotpath: ++summary.vector_in_hotpath; break;
                case StrategyZeroAllocGrade::NeedsScratchpadRefactor: ++summary.needs_scratchpad_refactor; break;
            }

            switch (row.audit_decision) {
                case StrategyAuditDecision::Keep: ++summary.keep; break;
                case StrategyAuditDecision::Tighten: ++summary.tighten; break;
                case StrategyAuditDecision::Rewrite: ++summary.rewrite; break;
                case StrategyAuditDecision::ExactTemplateMissing: ++summary.exact_template_missing; break;
            }

            if (row.parser_selectable) ++summary.parser_selectable;
            if (row.certifier_wired) ++summary.certifier_wired;
            if (row.mcts_dispatch_wired) ++summary.mcts_dispatch_wired;
            if (row.generator_dispatch_wired) ++summary.generator_dispatch_wired;
            if (row.generator_exact_template_wired) ++summary.generator_exact_template_wired;
            if (row.generator_family_fallback_wired) ++summary.generator_family_fallback_wired;
            if (row.smoke_profile_present) ++summary.smoke_profile_present;
            if (row.asymmetric_smoke_profile_present) ++summary.asymmetric_smoke_profile_present;
            if (row.family_fallback_only) ++summary.family_fallback_only;
            if (row.exact_required_but_missing) ++summary.exact_required_but_missing;
            if (row.audit_decision == StrategyAuditDecision::Rewrite) ++summary.rewrite_candidates;
            if (row.audit_decision == StrategyAuditDecision::Tighten) ++summary.tighten_candidates;
        });
        return summary;
    }

    static bool slot_from_required_strategy(RequiredStrategy rs, size_t& out_slot) {
        switch (rs) {
            case RequiredStrategy::NakedSingle: out_slot = SlotNakedSingle; return true;
            case RequiredStrategy::HiddenSingle: out_slot = SlotHiddenSingle; return true;
            case RequiredStrategy::PointingPairs: out_slot = SlotPointingPairs; return true;
            case RequiredStrategy::BoxLineReduction: out_slot = SlotBoxLineReduction; return true;
            case RequiredStrategy::NakedPair: out_slot = SlotNakedPair; return true;
            case RequiredStrategy::HiddenPair: out_slot = SlotHiddenPair; return true;
            case RequiredStrategy::NakedTriple: out_slot = SlotNakedTriple; return true;
            case RequiredStrategy::HiddenTriple: out_slot = SlotHiddenTriple; return true;
            case RequiredStrategy::NakedQuad: out_slot = SlotNakedQuad; return true;
            case RequiredStrategy::HiddenQuad: out_slot = SlotHiddenQuad; return true;
            case RequiredStrategy::XWing: out_slot = SlotXWing; return true;
            case RequiredStrategy::YWing: out_slot = SlotYWing; return true;
            case RequiredStrategy::Skyscraper: out_slot = SlotSkyscraper; return true;
            case RequiredStrategy::TwoStringKite: out_slot = SlotTwoStringKite; return true;
            case RequiredStrategy::EmptyRectangle: out_slot = SlotEmptyRectangle; return true;
            case RequiredStrategy::RemotePairs: out_slot = SlotRemotePairs; return true;
            case RequiredStrategy::Swordfish: out_slot = SlotSwordfish; return true;
            case RequiredStrategy::FinnedXWingSashimi: out_slot = SlotFinnedXWingSashimi; return true;
            case RequiredStrategy::SimpleColoring: out_slot = SlotSimpleColoring; return true;
            case RequiredStrategy::BUGPlusOne: out_slot = SlotBUGPlusOne; return true;
            case RequiredStrategy::UniqueRectangle: out_slot = SlotUniqueRectangle; return true;
            case RequiredStrategy::XYZWing: out_slot = SlotXYZWing; return true;
            case RequiredStrategy::WWing: out_slot = SlotWWing; return true;
            case RequiredStrategy::Jellyfish: out_slot = SlotJellyfish; return true;
            case RequiredStrategy::XChain: out_slot = SlotXChain; return true;
            case RequiredStrategy::XYChain: out_slot = SlotXYChain; return true;
            case RequiredStrategy::WXYZWing: out_slot = SlotWXYZWing; return true;
            case RequiredStrategy::FinnedSwordfishJellyfish: out_slot = SlotFinnedSwordfishJellyfish; return true;
            case RequiredStrategy::ALSXZ: out_slot = SlotALSXZ; return true;
            case RequiredStrategy::UniqueLoop: out_slot = SlotUniqueLoop; return true;
            case RequiredStrategy::AvoidableRectangle: out_slot = SlotAvoidableRectangle; return true;
            case RequiredStrategy::BivalueOddagon: out_slot = SlotBivalueOddagon; return true;
            case RequiredStrategy::UniqueRectangleExtended: out_slot = SlotURExtended; return true;
            case RequiredStrategy::HiddenUniqueRectangle: out_slot = SlotHiddenUR; return true;
            case RequiredStrategy::BUGType2: out_slot = SlotBUGType2; return true;
            case RequiredStrategy::BUGType3: out_slot = SlotBUGType3; return true;
            case RequiredStrategy::BUGType4: out_slot = SlotBUGType4; return true;
            case RequiredStrategy::BorescoperQiuDeadlyPattern: out_slot = SlotBorescoperQiuDeadlyPattern; return true;
            case RequiredStrategy::Medusa3D: out_slot = SlotMedusa3D; return true;
            case RequiredStrategy::AIC: out_slot = SlotAIC; return true;
            case RequiredStrategy::GroupedAIC: out_slot = SlotGroupedAIC; return true;
            case RequiredStrategy::GroupedXCycle: out_slot = SlotGroupedXCycle; return true;
            case RequiredStrategy::ContinuousNiceLoop: out_slot = SlotContinuousNiceLoop; return true;
            case RequiredStrategy::ALSXYWing: out_slot = SlotALSXYWing; return true;
            case RequiredStrategy::ALSChain: out_slot = SlotALSChain; return true;
            case RequiredStrategy::AlignedPairExclusion: out_slot = SlotAlignedPairExclusion; return true;
            case RequiredStrategy::AlignedTripleExclusion: out_slot = SlotAlignedTripleExclusion; return true;
            case RequiredStrategy::ALSAIC: out_slot = SlotALSAIC; return true;
            case RequiredStrategy::SueDeCoq: out_slot = SlotSueDeCoq; return true;
            case RequiredStrategy::DeathBlossom: out_slot = SlotDeathBlossom; return true;
            case RequiredStrategy::FrankenFish: out_slot = SlotFrankenFish; return true;
            case RequiredStrategy::MutantFish: out_slot = SlotMutantFish; return true;
            case RequiredStrategy::KrakenFish: out_slot = SlotKrakenFish; return true;
            case RequiredStrategy::Squirmbag: out_slot = SlotSquirmbag; return true;
            case RequiredStrategy::MSLS: out_slot = SlotMSLS; return true;
            case RequiredStrategy::Exocet: out_slot = SlotExocet; return true;
            case RequiredStrategy::SeniorExocet: out_slot = SlotSeniorExocet; return true;
            case RequiredStrategy::SKLoop: out_slot = SlotSKLoop; return true;
            case RequiredStrategy::PatternOverlayMethod: out_slot = SlotPatternOverlayMethod; return true;
            case RequiredStrategy::ForcingChains: out_slot = SlotForcingChains; return true;
            case RequiredStrategy::DynamicForcingChains: out_slot = SlotDynamicForcingChains; return true;
            case RequiredStrategy::None:
            case RequiredStrategy::Backtracking:
                return false;
        }
        return false;
    }

    static bool slot_enabled_for_dispatch(size_t slot, int max_level) {
        if (max_level <= 0) return false;
        switch (slot) {
            case SlotNakedSingle:
            case SlotHiddenSingle:
                return max_level >= 1;
            case SlotPointingPairs:
            case SlotBoxLineReduction:
            case SlotNakedPair:
            case SlotHiddenPair:
            case SlotNakedTriple:
            case SlotHiddenTriple:
                return max_level >= 2;
            case SlotNakedQuad:
            case SlotHiddenQuad:
            case SlotXWing:
            case SlotYWing:
            case SlotSkyscraper:
            case SlotTwoStringKite:
            case SlotEmptyRectangle:
            case SlotRemotePairs:
                return max_level >= 3;
            case SlotSwordfish:
            case SlotFinnedXWingSashimi:
            case SlotSimpleColoring:
            case SlotBUGPlusOne:
            case SlotUniqueRectangle:
            case SlotXYZWing:
            case SlotWWing:
                return max_level >= 4;
            case SlotJellyfish:
            case SlotXChain:
            case SlotXYChain:
            case SlotWXYZWing:
            case SlotFinnedSwordfishJellyfish:
            case SlotALSXZ:
            case SlotUniqueLoop:
            case SlotAvoidableRectangle:
            case SlotBivalueOddagon:
            case SlotURExtended:
            case SlotHiddenUR:
            case SlotBUGType2:
            case SlotBUGType3:
            case SlotBUGType4:
            case SlotBorescoperQiuDeadlyPattern:
                return max_level >= 5;
            case SlotMedusa3D:
            case SlotAIC:
            case SlotGroupedAIC:
            case SlotGroupedXCycle:
            case SlotContinuousNiceLoop:
            case SlotALSXYWing:
            case SlotALSChain:
            case SlotSueDeCoq:
            case SlotDeathBlossom:
            case SlotFrankenFish:
            case SlotMutantFish:
            case SlotKrakenFish:
            case SlotSquirmbag:
            case SlotAlignedPairExclusion:
            case SlotAlignedTripleExclusion:
            case SlotALSAIC:
                return max_level >= 6;
            case SlotMSLS:
            case SlotExocet:
            case SlotSeniorExocet:
            case SlotSKLoop:
            case SlotPatternOverlayMethod:
            case SlotForcingChains:
            case SlotDynamicForcingChains:
                return max_level >= 7;
        }
        return false;
    }

    static ApplyResult apply_strategy_slot(CandidateState& st, GenericLogicCertifyResult& result, size_t slot) {
        switch (slot) {
            case SlotNakedSingle: return p1_easy::apply_naked_single(st, result.strategy_stats[SlotNakedSingle], result);
            case SlotHiddenSingle: return p1_easy::apply_hidden_single(st, result.strategy_stats[SlotHiddenSingle], result);
            case SlotPointingPairs: return p2_intersections::apply_pointing_pairs(st, result.strategy_stats[SlotPointingPairs], result);
            case SlotBoxLineReduction: return p2_intersections::apply_box_line_reduction(st, result.strategy_stats[SlotBoxLineReduction], result);
            case SlotNakedPair: return p3_subsets::apply_house_subset(st, result.strategy_stats[SlotNakedPair], result, 2, false);
            case SlotHiddenPair: return p3_subsets::apply_house_subset(st, result.strategy_stats[SlotHiddenPair], result, 2, true);
            case SlotNakedTriple: return p3_subsets::apply_house_subset(st, result.strategy_stats[SlotNakedTriple], result, 3, false);
            case SlotHiddenTriple: return p3_subsets::apply_house_subset(st, result.strategy_stats[SlotHiddenTriple], result, 3, true);
            case SlotNakedQuad: return p3_subsets::apply_house_subset(st, result.strategy_stats[SlotNakedQuad], result, 4, false);
            case SlotHiddenQuad: return p3_subsets::apply_house_subset(st, result.strategy_stats[SlotHiddenQuad], result, 4, true);
            case SlotXWing: return p4_hard::apply_x_wing(st, result.strategy_stats[SlotXWing], result);
            case SlotYWing: return p4_hard::apply_y_wing(st, result.strategy_stats[SlotYWing], result);
            case SlotSkyscraper: return p4_hard::apply_skyscraper(st, result.strategy_stats[SlotSkyscraper], result);
            case SlotTwoStringKite: return p4_hard::apply_two_string_kite(st, result.strategy_stats[SlotTwoStringKite], result);
            case SlotEmptyRectangle: return p4_hard::apply_empty_rectangle(st, result.strategy_stats[SlotEmptyRectangle], result);
            case SlotRemotePairs: return p4_hard::apply_remote_pairs(st, result.strategy_stats[SlotRemotePairs], result);
            case SlotSwordfish: return p4_hard::apply_swordfish(st, result.strategy_stats[SlotSwordfish], result);
            case SlotFinnedXWingSashimi: return p5_expert::apply_finned_x_wing_sashimi(st, result.strategy_stats[SlotFinnedXWingSashimi], result);
            case SlotSimpleColoring: return p5_expert::apply_simple_coloring(st, result.strategy_stats[SlotSimpleColoring], result);
            case SlotBUGPlusOne: return p5_expert::apply_bug_plus_one(st, result.strategy_stats[SlotBUGPlusOne], result);
            case SlotUniqueRectangle: return p5_expert::apply_unique_rectangle(st, result.strategy_stats[SlotUniqueRectangle], result);
            case SlotXYZWing: return p5_expert::apply_xyz_wing(st, result.strategy_stats[SlotXYZWing], result);
            case SlotWWing: return p5_expert::apply_w_wing(st, result.strategy_stats[SlotWWing], result);
            case SlotJellyfish: return p6_diabolical::apply_jellyfish(st, result.strategy_stats[SlotJellyfish], result);
            case SlotXChain: return p6_diabolical::apply_x_chain(st, result.strategy_stats[SlotXChain], result);
            case SlotXYChain: return p6_diabolical::apply_xy_chain(st, result.strategy_stats[SlotXYChain], result);
            case SlotWXYZWing: return p6_diabolical::apply_wxyz_wing(st, result.strategy_stats[SlotWXYZWing], result);
            case SlotFinnedSwordfishJellyfish: return p6_diabolical::apply_finned_swordfish_jellyfish(st, result.strategy_stats[SlotFinnedSwordfishJellyfish], result);
            case SlotALSXZ: return p6_diabolical::apply_als_xz(st, result.strategy_stats[SlotALSXZ], result);
            case SlotUniqueLoop: return p6_diabolical::apply_unique_loop(st, result.strategy_stats[SlotUniqueLoop], result);
            case SlotAvoidableRectangle: return p6_diabolical::apply_avoidable_rectangle(st, result.strategy_stats[SlotAvoidableRectangle], result);
            case SlotBivalueOddagon: return p6_diabolical::apply_bivalue_oddagon(st, result.strategy_stats[SlotBivalueOddagon], result);
            case SlotMedusa3D: return p7_nightmare::apply_medusa_3d(st, result.strategy_stats[SlotMedusa3D], result);
            case SlotAIC: return p7_nightmare::apply_aic(st, result.strategy_stats[SlotAIC], result);
            case SlotGroupedAIC: return p7_nightmare::apply_grouped_aic(st, result.strategy_stats[SlotGroupedAIC], result);
            case SlotGroupedXCycle: return p7_nightmare::apply_grouped_x_cycle(st, result.strategy_stats[SlotGroupedXCycle], result);
            case SlotContinuousNiceLoop: return p7_nightmare::apply_continuous_nice_loop(st, result.strategy_stats[SlotContinuousNiceLoop], result);
            case SlotALSXYWing: return p7_nightmare::apply_als_xy_wing(st, result.strategy_stats[SlotALSXYWing], result);
            case SlotALSChain: return p7_nightmare::apply_als_chain(st, result.strategy_stats[SlotALSChain], result);
            case SlotSueDeCoq: return p7_nightmare::apply_sue_de_coq(st, result.strategy_stats[SlotSueDeCoq], result);
            case SlotDeathBlossom: return p7_nightmare::apply_death_blossom(st, result.strategy_stats[SlotDeathBlossom], result);
            case SlotFrankenFish: return p7_nightmare::apply_franken_fish(st, result.strategy_stats[SlotFrankenFish], result);
            case SlotMutantFish: return p7_nightmare::apply_mutant_fish(st, result.strategy_stats[SlotMutantFish], result);
            case SlotKrakenFish: return p7_nightmare::apply_kraken_fish(st, result.strategy_stats[SlotKrakenFish], result);
            case SlotMSLS: return p8_theoretical::apply_msls(st, result.strategy_stats[SlotMSLS], result);
            case SlotExocet: return p8_theoretical::apply_exocet(st, result.strategy_stats[SlotExocet], result);
            case SlotSeniorExocet: return p8_theoretical::apply_senior_exocet(st, result.strategy_stats[SlotSeniorExocet], result);
            case SlotSKLoop: return p8_theoretical::apply_sk_loop(st, result.strategy_stats[SlotSKLoop], result);
            case SlotPatternOverlayMethod: return p8_theoretical::apply_pattern_overlay_method(st, result.strategy_stats[SlotPatternOverlayMethod], result);
            case SlotForcingChains: return p8_theoretical::apply_forcing_chains(st, result.strategy_stats[SlotForcingChains], result);
            case SlotSquirmbag: return p7_nightmare::apply_squirmbag(st, result.strategy_stats[SlotSquirmbag], result);
            case SlotURExtended: return p6_diabolical::apply_ur_extended(st, result.strategy_stats[SlotURExtended], result);
            case SlotHiddenUR: return p6_diabolical::apply_hidden_ur(st, result.strategy_stats[SlotHiddenUR], result);
            case SlotBUGType2: return p6_diabolical::apply_bug_type2(st, result.strategy_stats[SlotBUGType2], result);
            case SlotBUGType3: return p6_diabolical::apply_bug_type3(st, result.strategy_stats[SlotBUGType3], result);
            case SlotBUGType4: return p6_diabolical::apply_bug_type4(st, result.strategy_stats[SlotBUGType4], result);
            case SlotBorescoperQiuDeadlyPattern: return p6_diabolical::apply_borescoper_qiu_deadly_pattern(st, result.strategy_stats[SlotBorescoperQiuDeadlyPattern], result);
            case SlotAlignedPairExclusion: return p7_nightmare::apply_aligned_pair_exclusion(st, result.strategy_stats[SlotAlignedPairExclusion], result);
            case SlotAlignedTripleExclusion: return p7_nightmare::apply_aligned_triple_exclusion(st, result.strategy_stats[SlotAlignedTripleExclusion], result);
            case SlotALSAIC: return p7_nightmare::apply_als_aic(st, result.strategy_stats[SlotALSAIC], result);
            case SlotDynamicForcingChains: return p8_theoretical::apply_dynamic_forcing_chains(st, result.strategy_stats[SlotDynamicForcingChains], result);
        }
        return ApplyResult::NoProgress;
    }

private:
    static bool suppress_slot_in_required_corridor(size_t slot) {
        using namespace shared;
        const RequiredStrategy rs = current_required_exact_strategy();
        if (rs == RequiredStrategy::None) {
            return false;
        }
        const RequiredStrategy slot_rs = required_strategy_for_slot(slot);
        if (slot_rs == rs) {
            return false;
        }

        // Tłumi generyczne rodziny (Generic Equivalence Families) w momencie 
        // wymuszania specyficznych (Named) struktur przez generator. Zapobiega kradzieży USE.
        if (required_generic_family_suppression_active() && strategy_is_generic_equivalence_family(slot_rs)) {
            return true;
        }

        switch (rs) {
        case RequiredStrategy::FinnedSwordfishJellyfish:
            return slot_rs == RequiredStrategy::Swordfish ||
                   slot_rs == RequiredStrategy::Jellyfish ||
                   slot_rs == RequiredStrategy::FinnedXWingSashimi;
        default:
            return false;
        }
    }

    static inline void note_strategy_slot(GenericLogicCertifyResult& result, size_t slot, ApplyResult ar) {
        if (ar == ApplyResult::NoProgress) {
            return;
        }
        const StrategyMeta& meta = strategy_meta_for_slot(slot);
        const uint16_t hit_delta = (ar == ApplyResult::Progress) ? 1U : 0U;
        result.record_step(
            static_cast<uint16_t>(slot),
            meta.impl_tier,
            ar,
            0,
            hit_delta,
            meta.proof_tag);
    }

    // GŁÓWNA PĘTLA CERTYFIKATORA LOGICZNEGO
    static ApplyResult apply_round_up_to_level(CandidateState& st, GenericLogicCertifyResult& result, int max_level) {
        using namespace shared;

        const RequiredStrategy active_required = current_required_exact_strategy();
        size_t required_slot = kStrategySlotCount;
        const bool has_required_slot =
            slot_from_required_strategy(active_required, required_slot) &&
            slot_enabled_for_dispatch(required_slot, max_level);

        auto try_slot = [&](size_t slot, bool honor_required_corridor = true) -> ApplyResult {
            if (!slot_enabled_for_dispatch(slot, max_level)) {
                return ApplyResult::NoProgress;
            }
            if (has_required_slot && slot == required_slot) {
                return ApplyResult::NoProgress;
            }
            if (honor_required_corridor && suppress_slot_in_required_corridor(slot)) {
                return ApplyResult::NoProgress;
            }
            const ApplyResult ar = apply_strategy_slot(st, result, slot);
            if (ar != ApplyResult::NoProgress) {
                note_strategy_slot(result, slot, ar);
            }
            return ar;
        };

        // Zawsze pozwalamy wymaganej strategii ocenić planszę pierwsza, pod warunkiem, 
        // że jest to "Named Structure" / "Exact Template".
        if (has_required_slot) {
            const ApplyResult ar = apply_strategy_slot(st, result, required_slot);
            if (ar != ApplyResult::NoProgress) {
                note_strategy_slot(result, required_slot, ar);
                return ar;
            }
        }

        // ====================================================================
        // GŁÓWNY, RYGORYSTYCZNY DISPATCH ORDER (Rozwiązuje Strategy Overshadowing)
        // --------------------------------------------------------------------
        // Klasyfikacja działa według reguły: Named -> Fish -> ALS -> Uniqueness -> Chains.
        // Generyczne węzły (Chains) są oceniane ZAWSZE na samym końcu na danym poziomie, 
        // jako absolutne narzędzie "ostateczne" (gdy strukturalne szablony zawodzą).
        // ====================================================================
        static constexpr std::array<size_t, kStrategySlotCount> kDispatchOrder = {{
            // LEVEL 1
            SlotNakedSingle,
            SlotHiddenSingle,

            // LEVEL 2
            SlotPointingPairs,
            SlotBoxLineReduction,
            SlotNakedPair,
            SlotHiddenPair,
            SlotNakedTriple,
            SlotHiddenTriple,

            // LEVEL 3
            SlotNakedQuad,
            SlotHiddenQuad,
            // Named / Fish P3
            SlotXWing,
            SlotYWing,
            SlotSkyscraper,
            SlotTwoStringKite,
            SlotEmptyRectangle,
            // Generic P3
            SlotRemotePairs,

            // LEVEL 4
            // Named / Fish / ALS P4
            SlotSwordfish,
            SlotFinnedXWingSashimi,
            SlotXYZWing,
            SlotWWing,
            // Uniqueness P4
            SlotUniqueRectangle,
            SlotBUGPlusOne,
            // Generic P4
            SlotSimpleColoring,

            // LEVEL 5
            // Named / Fish / ALS P5
            SlotJellyfish,
            SlotFinnedSwordfishJellyfish,
            SlotWXYZWing,
            SlotALSXZ,
            // Uniqueness P5
            SlotBUGType2,
            SlotBUGType3,
            SlotBUGType4,
            SlotUniqueLoop,
            SlotAvoidableRectangle,
            SlotBivalueOddagon,
            // Generic P5
            SlotXChain,
            SlotXYChain,

            // LEVEL 6
            // Uniqueness P6
            SlotURExtended,
            SlotHiddenUR,
            SlotBorescoperQiuDeadlyPattern,
            // Named / ALS P6
            SlotSueDeCoq,
            SlotDeathBlossom,
            SlotALSXYWing,
            SlotALSChain,
            SlotALSAIC,
            // Generic P6
            SlotMedusa3D,
            SlotGroupedXCycle,
            SlotContinuousNiceLoop,
            SlotAIC,
            SlotGroupedAIC,

            // LEVEL 7
            // Heavy Fish / Exclusions P7
            SlotFrankenFish,
            SlotMutantFish,
            SlotKrakenFish,
            SlotSquirmbag,
            SlotAlignedPairExclusion,
            SlotAlignedTripleExclusion,
            // Generic P7
            SlotForcingChains,
            SlotDynamicForcingChains,

            // LEVEL 8
            // Theoretical / Heavy P8
            SlotMSLS,
            SlotExocet,
            SlotSeniorExocet,
            SlotSKLoop,
            SlotPatternOverlayMethod
        }};

        for (const size_t slot : kDispatchOrder) {
            const ApplyResult ar = try_slot(slot, true);
            if (ar != ApplyResult::NoProgress) {
                return ar;
            }
        }

        return ApplyResult::NoProgress;
    }


public:
    GenericLogicCertifyResult certify(
        std::span<const uint16_t> puzzle,
        const GenericTopology& topo,
        core_engines::SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        return certify_up_to_level(puzzle, topo, 8, budget, capture_solution_grid);
    }

    GenericLogicCertifyResult certify(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        core_engines::SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        return certify(std::span<const uint16_t>(puzzle.data(), puzzle.size()), topo, budget, capture_solution_grid);
    }

    GenericLogicCertifyResult certify_up_to_level(
        std::span<const uint16_t> puzzle,
        const GenericTopology& topo,
        int max_level,
        core_engines::SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        
        GenericLogicCertifyResult result{};
        const bool has_budget = (budget != nullptr);
        const int level_limit = std::clamp(max_level, 1, 8);

        // Bufor dla planszy (zero-alloc między wykonaniami poszczególnych komórek)
        GenericBoard& board = generic_tls_board();
        board.topo = &topo;
        if (!board.init_from_puzzle(puzzle, false)) return result;

        // Płaski bufor dla tablicy masek (zero-alloc), wprowadzany do CandidateState
        // Zapas 4096 pozwala na bezproblemową operację na ekstremalnych siatkach (np. 64x64).
        static thread_local uint64_t tls_cands[4096];
        
        CandidateState st{};
        if (!st.init(board, topo, tls_cands)) return result;

        // Główna pętla dyspozytora. Każdy powrót "Progress" sprawia, że zaczynamy 
        // przeczesywać strategie od najszybszych i najprostszych (P1).
        while (board.empty_cells != 0) {
            if (has_budget && !budget->step()) {
                result.timed_out = true;
                result.solved = false;
                return result;
            }
            
            const ApplyResult ar = apply_round_up_to_level(st, result, level_limit);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;
            
            // Żadna ze strategii na dozwolonym poziomie nie odnalazła dedukcji (Wąskie gardło nierozwiązane)
            break;
        }

        result.solved = (board.empty_cells == 0);
        if (capture_solution_grid) {
            result.solved_grid = board.values;
        }
        
        // Zapis flag dla testów mikro-profilujących
        result.naked_single_scanned = result.strategy_stats[SlotNakedSingle].use_count > 0;
        result.hidden_single_scanned = result.strategy_stats[SlotHiddenSingle].use_count > 0;
        
        return result;
    }

    GenericLogicCertifyResult certify_up_to_level(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        int max_level,
        core_engines::SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        return certify_up_to_level(
            std::span<const uint16_t>(puzzle.data(), puzzle.size()),
            topo,
            max_level,
            budget,
            capture_solution_grid);
    }
};

} // namespace sudoku_hpc::logic