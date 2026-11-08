# AromaFG Roadmap

Milestones are targets, not claims of implementation or hardware validation. Branch-specific implementation status is identified explicitly.

## Phase 0 — repository and contracts

- [x] Audit repository, branches, pull requests and CI evidence.
- [x] Record clean-room scope and architecture decisions.
- [x] Review portable C ABI, Vulkan host ownership and runtime implementation.
- [x] Identify command-buffer cancellation/submission, memory and capability risks.
- [x] Record external-project and licensing questions for Phase 2.

## Phase 1 — documentation baseline

- [x] Align README with branch-specific maturity and evidence.
- [x] Document architecture and separate CPU payload lifetime from Vulkan GPU-object lifetime.
- [x] Document Vulkan host ownership, synchronization and submission/cancellation gap.
- [x] Align build instructions with actual CMake options and CI path.
- [x] Update Adreno/Turnip validation plan and software-vs-hardware evidence boundary.
- [x] Record known limitations and release gates.
- [ ] Review and approve documentation PR; resolve factual comments.

## Phase 2 — research and technology selection

- [ ] Re-verify candidate FG/VFI repositories, exact revisions and source provenance.
- [ ] Build a file-level license matrix for source, shaders, model code, weights and datasets.
- [ ] Compare classical optical flow: block matching, LK, DIS, Farneback and pyramid variants.
- [ ] Evaluate mobile inference runtimes and model candidates without assuming realtime suitability.
- [ ] Establish benchmark datasets, ground truth capture protocol and quality metrics.
- [ ] Select a lightweight baseline algorithm and define measurable MVP budgets.

## Phase 3 — API and host lifecycle hardening

- [ ] Define explicit record/submit/commit/cancel semantics for host command buffers.
- [ ] Make frame retry behavior correct for BUSY and backend errors.
- [ ] Define GPU object lifetime, slot fence ownership and resize/quiescence requirements in ABI.
- [ ] Validate formats, sampler filtering features, image usage and enabled device features.
- [ ] Define output format conversion/fallback policy.
- [ ] Add tests for command-buffer abandonment, failed recording and slot reuse.

## Phase 4 — lightweight Frame Generation MVP

- [ ] Implement and validate a low-cost classical flow algorithm.
- [ ] Reuse one flow solution for all chronological synthesis phases.
- [ ] Establish real-frame fallback and scene-cut handling.
- [ ] Compare against true midpoint ground truth on a reproducible dataset.
- [ ] Measure per-stage GPU time, memory, latency and thermal behavior.

## Phase 5 — Vulkan backend stabilization

- [ ] Fix or redesign command-buffer cancellation and query/resource state tracking.
- [ ] Add memory suballocation and memory-budget-aware resource pooling.
- [ ] Add robust capability/format checks and device-loss recovery.
- [ ] Validate descriptor/query slot reuse and resize on real drivers.
- [ ] Add Android NDK builds and host-side device integration tests.

## Phase 6 — Adreno / Turnip optimization

- [ ] Test on named Adreno devices and pinned Turnip revisions.
- [ ] Profile workgroup sizes, search radius, flow resolution and bandwidth.
- [ ] Reduce intermediate image count and memory pressure.
- [ ] Evaluate GMEM/tile behavior without assuming undocumented driver behavior.
- [ ] Compare proprietary Qualcomm and Turnip paths.

## Phase 7 — optional AI runtime and refinement

- [ ] Audit model architecture, source license, weights and dataset rights independently.
- [ ] Compare ncnn, MNN, ONNX Runtime and Qualcomm QNN on actual target devices.
- [ ] Prototype optional learned residual/refinement, never mandatory in baseline.
- [ ] Measure model load time, activation memory, latency, thermal and quality benefit.
- [ ] Reject models that fail sustained realtime budgets.

## Phase 8 — quality stack

- [ ] C1 global-motion pre-warp (Lucas–Kanade affine) behind a feature flag.
- [ ] C2 edge-preserving flow regularization.
- [ ] Occlusion-aware candidate reconstruction and anti-ghost fallback.
- [ ] HUD handling, color-space correctness and optional sharpening.
- [ ] Calibrate confidence/risk metrics against high-FPS ground truth.

## Phase 9 — pacing and latency

- [ ] Separate compute completion, acquire, present acceptance and physical scanout.
- [ ] Design host-cooperative pacing and bounded queue depth.
- [ ] Measure latency distribution and dropped/late generated frames.
- [ ] Keep WSI ownership and actual scheduling in the host.

## Phase 10 — Android SDK

- [ ] Define NDK packaging, ABI stability, sample host and lifecycle.
- [ ] Validate Android loader, AHardwareBuffer interop and synchronization.
- [ ] Test on physical Turnip and proprietary drivers.

## Phase 11 — Winlator / Wine / DXVK integration

- [ ] Pin exact Winlator/WinNative and DXVK forks/versions.
- [ ] Implement a native host-side adapter first.
- [ ] Verify generated buffers reach compositor/present path.
- [ ] Validate WineD3D, DXVK and native Vulkan separately.

## Phase 12 — optional Vulkan Layer

- [ ] Define supported WSI/present behavior and extension interactions.
- [ ] Preserve host synchronization and avoid competing presentation.
- [ ] Add explicit compatibility matrix and opt-in diagnostics.

## Phase 13 — emulator and game-host integrations

- [ ] Prioritize hosts with a maintainable native render/compositor integration.
- [ ] Treat each emulator/backend/version as a separate adapter and test target.
- [ ] Avoid unsupported claims of universal emulator compatibility.

## Phase 14 — QA, benchmark and release

- [ ] Run repeatable A/B image-quality, latency, performance and thermal tests.
- [ ] Test sustained sessions, recovery and fallback.
- [ ] Publish exact device/driver/game/host matrix and known limitations.
- [ ] Produce reproducible artifacts, ABI policy, changelog and release notes.

## Release gate

No production or superiority claim until image quality, delivered-frame cadence, latency, GPU cost, memory, thermals, recovery and host compatibility pass repeatable tests on named hardware and driver revisions.
