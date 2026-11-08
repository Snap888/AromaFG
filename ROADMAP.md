# AromaFG Roadmap

This roadmap is based on the repository requirements and the review of SnapFrameBackendV1. Milestones are targets, not claims of implementation or hardware validation.

## Phase 0 — repository and contracts
- [x] Public repository bootstrap and project scope.
- [x] Record clean-room analysis and selected architecture decisions.
- [x] Define host/Vulkan ownership, telemetry semantics and validation gates.
- [ ] Finalize MPL-2.0 licensing and contribution policy.
- [ ] Land versioned C ABI and portable lifecycle tests.

## Phase 1 — portable engine and baseline FG
- [ ] Backend-neutral C++20 engine, frame ordering, bypass and history reset.
- [ ] Host-borrowed Vulkan interop contract.
- [ ] Luma/downsample, pyramid, bidirectional block matching, fusion and FB consistency.
- [ ] One flow solve per source pair; phase-specific ×2/×3/×4 synthesis.
- [ ] Offline shader compilation and SPIR-V validation.
- [ ] Real-frame fallback for every unsupported/error path.

## Phase 1.5 — quality stack
- [ ] C1 global-motion affine pre-warp (Lucas–Kanade) behind a feature flag.
- [ ] C2 edge-preserving flow regularization.
- [ ] Confidence-aware candidate reconstruction and occlusion/disocclusion fallback.
- [ ] Anti-ghost experiments: robust median/refinement and content-difference gate.
- [ ] Calibrate metrics against true midpoint ground truth.

## Phase 2 — native integration and pacing
- [ ] Native host-side API and example.
- [ ] Bounded frame pacing recommendations; host retains WSI ownership.
- [ ] Engineer Mode: timestamps, asynchronous scalar reductions, JSONL.
- [ ] Passive successful-present telemetry.
- [ ] Separate X11, Wayland, Winlator and DXVK adapter plans and tests.

## Phase 3 — adaptive policy and diagnostics
- [ ] Deterministic adaptive workload controller with smoothing/hysteresis.
- [ ] WSI delivery feedback based only on confirmed host events.
- [ ] Debug views with zero extra work when disabled.
- [ ] Optional learned residual only if it proves net quality/latency benefit.

## Phase 4 — platform and release validation
- [ ] Physical Adreno + Turnip validation.
- [ ] Proprietary Qualcomm driver validation.
- [ ] Exact host-fork integration builds and runtime tests.
- [ ] Repeatable A/B benchmarks; publish wins and losses.
- [ ] Release artifacts, ABI compatibility policy and reproducible build docs.

## Release gate

No production or superiority claim until image quality, delivered-frame cadence, latency, GPU cost, thermals, recovery and host compatibility pass repeatable tests on named hardware and driver revisions.