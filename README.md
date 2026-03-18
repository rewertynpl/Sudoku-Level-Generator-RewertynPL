# Sudoku Level Generator RewertynPL 🚀

An ultra-fast, highly optimized, multithreaded Sudoku puzzle generator written in modern **C++20**.

Designed with **High-Performance Computing (HPC)** in mind, this engine can generate and validate massive amounts of Sudoku grids ranging from basic **4x4** up to complex **36x36** layouts, including both classic symmetric and asymmetric geometries.

The project focuses on:
- very high raw throughput,
- strict uniqueness validation,
- named logical-strategy targeting,
- difficulty-aware generation,
- telemetry-heavy benchmarking and audit workflows.

---

## Table of Contents

- [Overview](#overview)
- [Current Project Status](#current-project-status)
- [Difficulty Model](#difficulty-model)
- [Strategy Implementation Status](#strategy-implementation-status)
- [Pattern Forcing and Advanced Pattern Coverage](#pattern-forcing-and-advanced-pattern-coverage)
- [Geometry Notes](#geometry-notes)
- [Key Features](#key-features)
- [Technology Stack](#technology-stack)
- [Getting Started](#getting-started)
- [CLI Examples](#cli-examples)
- [Validation, Smoke Audits and Benchmarking](#validation-smoke-audits-and-benchmarking)
- [Project Philosophy](#project-philosophy)
- [License](#license)

---

## Overview

**Sudoku Level Generator RewertynPL** is a performance-oriented Sudoku generation engine built from scratch in modern C++20, with no external dependencies.

It combines:
- a high-speed generator,
- strict uniqueness validation,
- a logic certifier,
- named-strategy targeting,
- geometry-aware difficulty tuning,
- and extensive profiling/reporting infrastructure.

This is not just a generic Sudoku creator. The project is designed as a **difficulty-aware generation system** capable of targeting specific logical structures, certifying named solving strategies, and benchmarking generation quality under strict contracts.

---

## Current Project Status

### GUI Language
The native **WinAPI GUI** is currently available only in **Polish**.

### CLI / Internal Reporting
CLI commands, debug logs, benchmark reports, and most internal audit output are **English** or **mixed PL/EN**.

### Implementation State
The project is **well beyond “Level 1 only”**.

Level 1 logic is mature, and a growing subset of **Level 2–5** strategies is already implemented, visible in the generator/certifier pipeline, and validated by strict smoke/exact-contract runs.

At the same time, the highest tiers are still under active development. Some advanced families are already wired into the engine and selectable in the strategy system, but their coverage is still partial, template-driven, geometry-restricted, or experimental.

---

## Difficulty Model

The project currently uses the following strategy-tier model:

### 🟢 Level 1 — Easy
- Naked Single
- Hidden Single
- Pointing Pairs / Triples
- Box / Line Reduction

### 🟡 Level 2 — Medium
- Naked Pair
- Hidden Pair
- Naked Triple
- Hidden Triple

### 🟠 Level 3 — Hard
- Naked Quad
- Hidden Quad
- X-Wing
- Y-Wing (XY-Wing)
- Skyscraper
- 2-String Kite
- Empty Rectangle
- Remote Pairs

### 🔴 Level 4 — Expert
- Swordfish
- XYZ-Wing
- Finned X-Wing / Sashimi
- Unique Rectangle (Type 1)
- BUG+1
- W-Wing
- Simple Coloring

### 🟣 Level 5 — Diabolical
- Jellyfish
- WXYZ-Wing
- Finned Swordfish / Jellyfish
- X-Chain
- XY-Chain
- ALS-XZ
- Unique Loop
- Avoidable Rectangle
- Bivalue Oddagon
- Extended / Hidden Unique Rectangle
- BUG Type 2/3/4
- Borescoper / Qiu Deadly Pattern

### ⚫ Level 6 — Nightmare
- Medusa 3D
- AIC
- Grouped AIC
- Grouped X-Cycle
- Continuous Nice Loop
- ALS-XY-Wing
- ALS Chain
- Aligned Pair Exclusion
- Aligned Triple Exclusion
- ALS-AIC
- Sue de Coq
- Death Blossom
- Franken Fish
- Mutant Fish
- Kraken Fish
- Squirmbag

### 🧠 Level 7 — Theoretical / Pattern-Driven
- MSLS
- Exocet
- Senior Exocet
- SK Loop
- Pattern Overlay Method
- Forcing Chains
- Dynamic Forcing Chains

### 🛠 Level 8 — Brute Force
- Backtracking

---

## Strategy Implementation Status

### Strong and already practical
The strongest currently validated coverage is in the lower and middle tiers:
- singles,
- intersections,
- subsets,
- selected fish,
- selected wings,
- selected uniqueness families.

Recent strict smoke/exact runs show very strong results for strategies such as:
- Naked Single
- Hidden Single
- Pointing Pairs
- Box/Line Reduction
- Naked Pair / Hidden Pair
- Naked Triple / Hidden Triple
- Naked Quad / Hidden Quad
- X-Wing
- 2-String Kite
- Swordfish
- XYZ-Wing

Several of these now reach full **10/10 accepted** in current strict smoke runs, with especially strong behavior in the P1–P4 core families.

### Partially mature / mixed maturity
Some expert and diabolical families are already present and can work, but their maturity varies depending on:
- geometry,
- clue density,
- exact-template availability,
- fallback policy,
- and certifier interaction.

### Still incomplete / experimental
The hardest families remain the least complete area:
- AIC-like chain systems,
- grouped and continuous loop variants,
- heavy fish,
- theoretical exact patterns,
- forcing-chain families.

These are already represented in the strategy catalog and in the engine architecture, but practical generation coverage is still narrower than for the lower tiers.

---

## Pattern Forcing and Advanced Pattern Coverage

The hardest strategy families are currently tied to the **`generator/pattern_forcing/`** subsystem.

This is an important design detail:

the generator does **not** attempt exhaustive discovery of all mathematically possible realizations of the hardest patterns.

Instead, it focuses on:
- exact templates,
- named structural motifs,
- pattern anchors,
- family-level fallbacks,
- and controlled pattern-driven generation policies.

That means support for advanced families such as:
- **MSLS**
- **Exocet / Senior Exocet**
- **SK Loop**
- **Pattern Overlay Method**
- **Forcing Chains**
- **Dynamic Forcing Chains**

should be understood as **template-oriented coverage**, not full combinatorial coverage of every theoretically possible board realization.

In practice, this means the engine currently covers only a **small fraction** of the full mathematical pattern space for the hardest families.

---

## Geometry Notes

This generator supports both:
- **classic symmetric geometries** (for example 9x9 with 3x3 boxes),
- and **asymmetric geometries** (for example 12x12 with 4x3 boxes).

That matters a lot for advanced strategy generation.

### Some strategies are effectively larger-board strategies
A number of advanced families are only enabled from **N >= 12** upward, because smaller boards do not provide enough structural space or candidate complexity for robust generation.

Examples include:
- Franken Fish
- Mutant Fish
- Kraken Fish
- Squirmbag
- MSLS

### Some theoretical patterns are allowed on 9x9, but are fragile there
The engine allows some theoretical-exact strategies on classic 9x9, including:
- Exocet
- Senior Exocet
- SK Loop
- Pattern Overlay Method

However, these are generally much rarer and more brittle on 9x9 than on larger or asymmetric boards.

### Some families prefer asymmetric geometries
Certain advanced families benefit from asymmetric layouts because those boards tend to preserve richer box/row/column interactions and offer a denser structural search space.

This is especially relevant for patterns such as:
- Grouped AIC
- Grouped X-Cycle
- Continuous Nice Loop
- Exocet / Senior Exocet
- SK Loop
- Pattern Overlay Method
- heavy fish families

---

## Key Features

### Extreme Performance
- Designed for very high throughput generation.
- Makes heavy use of **bitwise operations** and performance-oriented data handling.
- CPU backend selection is built into the runtime configuration.

### Advanced Threading
- Multithreaded generation engine.
- Built with a low-latency mindset suitable for HPC-style batch generation workflows.

### Zero-Allocation Hot Paths
- Large parts of the strategy and runtime infrastructure are explicitly tracked for zero-allocation hot-path safety.
- The codebase includes internal audit metadata for zero-allocation grading and strategy hot-path quality.

### Uniqueness Validation
- Includes a highly optimized uniqueness solver pipeline.
- Uses strict validation and contract-style reporting during smoke and benchmark runs.

### Flexible Geometries
- Supports standard and asymmetric Sudoku box layouts.
- Geometry selection is a first-class part of generation and validation.

### Difficulty Targeting
- Strategy-aware difficulty targeting is built into the generator and certifier.
- The engine contains named-strategy selection, level gating, exact-contract checks, clue-family windows, and geometry-aware difficulty policies.

### Telemetry and Auditability
- Built-in smoke audits
- VIP scoring / VIP gates
- quality benchmarking
- geometry gates
- exact-contract tracking
- clue-policy debugging
- replay/distribution validation options

---

## Technology Stack

- **Language:** C++20
- **Platform:** Windows
- **GUI:** Native WinAPI
- **Dependencies:** None

This project relies purely on:
- the **C++ Standard Library**
- the **Windows API**

No external libraries such as Qt, Boost, SDL, or other third-party frameworks are required.

---

## Getting Started

### Prerequisites

- A **C++20-compatible compiler** such as:
  - MSVC
  - GCC
  - Clang
- **Windows OS** for the native GUI version
- A CPU with **AVX2 / AVX512** support is highly recommended for maximum performance

### Basic Usage

You can run the generator from the command line with configurable geometry, difficulty, strategy, benchmarking, and reporting options.

---

## CLI Examples

### Generate 100 classic 9x9 puzzles at difficulty level 1 using 8 threads

```bash
sudoku.exe --box-rows 3 --box-cols 3 --target 100 --difficulty 1 --threads 8
```

### List all supported geometries

```bash
sudoku.exe --list-geometries
```

### Run a smoke / benchmark-style audit

```bash
sudoku.exe --smoke-mode --run-quality-benchmark
```

### Generate puzzles targeting a named strategy

```bash
sudoku.exe --box-rows 3 --box-cols 3 --difficulty 3 --required-strategy xwing --target 10
```

### Enable pattern forcing

```bash
sudoku.exe --pattern-forcing --pattern-forcing-tries 6
```

---

## Validation, Smoke Audits and Benchmarking

A major strength of this project is that it is not only a generator, but also an **instrumented validation environment**.

The codebase includes support for:
- smoke/benchmark mode,
- quality benchmark runs,
- geometry gates,
- pre-difficulty gates,
- VIP benchmarks,
- VIP gates,
- strict canonical strategy checks,
- clue-policy diagnostics,
- and exact-contract reporting.

This makes the project especially useful not only for generation, but also for:
- tuning strategy pipelines,
- validating certifier behavior,
- stress-testing geometry-dependent generation,
- and profiling strategy maturity over time.

---

## Project Philosophy

This project aims to combine:
- **high raw performance**,
- **strict validation discipline**,
- **named logical difficulty control**,
- and **research-style experimentation** in advanced Sudoku generation.

The lower and mid tiers are already becoming practical and measurable.
The upper tiers are still evolving, especially where the engine enters template-driven theoretical pattern generation.

That trade-off is intentional:
the project prefers **auditable, reproducible, pattern-aware generation** over vague claims of full coverage for extremely difficult theoretical families.

---

## License

This project is licensed under the **MIT License**.

You are free to use this software for:
- private use,
- commercial use,
- modification,
- redistribution.

See the `LICENSE` file for details.

### Note regarding third-party code
At the current stage, this project does **not** include third-party libraries or dependencies requiring separate licensing or purchase.

Everything is built using:
- standard C++,
- native OS APIs,
- and project-owned code.

---

**Ultra-fast C++20 Sudoku generator with strict uniqueness validation, named-strategy targeting, asymmetric geometry support, and HPC-style benchmarking.**
