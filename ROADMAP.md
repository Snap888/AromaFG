# AromaFG Roadmap

Milestones are targets, not claims of implementation or hardware validation. Branch-specific implementation status is identified explicitly.

## Phase 0 — repository and contracts

- [x] Audit repository, branches, pull requests and CI evidence.
- [x] Record clean-room scope and architecture decisions.
- [x] Review portable C ABI, Vulkan host ownership and runtime implementation.
- [x] Identify command-buffer cancellation/submission, memory and capability risks.
- [x] Record external-project and licensing questions for Phase 2.

## Phase 1 — documentation baseline

- [x] Align README with branch-specific implementation maturity and evidence.
- [x] Document architecture and separate CPU callback payload lifetime from Vulkan GPU-object lifetime.
- [x] Document Vulkan host ownership, synchronization and submission/cancellation gap.
- [x] Align build instructions with actual CMake options and CI path.
- [x] Update Adreno/Turnip validation plan and software-vs-hardware evidence boundary.
- [x] Record known limitations and release gates.
- [ ] Review and approve documentation PR #4; resolve factual comments.

## Phase 2 — research and technology selection

- [x] Re-verify candidate FG/VFI repositories, immutable review revisions and provenance sources.
- [x] Build an initial source/subtree license matrix; explicitly mark model checkpoints and datasets that are not cleared.
- [x] Compare classical optical flow: block matching, LK, DIS, Farneback and pyramid variants.
- [x] Evaluate mobile inference runtimes and model candidates without assuming realtime suitability.
- [x] Establish benchmark dataset roles, high-FPS ground-truth protocol and quality metrics.
- [x] Select Win-FG SAD/block-search and WFG DIS/Farneback hybrid as A/B references; retain block matching as control and define proposed MVP budgets.
- [ ] Complete exact checkpoint/dataset rights review before any such artifact is imported or redistributed.
- [ ] Review and approve Phase 2 research PR #5.

## Phase 3 — API and host lifecycle hardening

- [x] Add explicit backend record/submit-commit/cancel callbacks and public frame transaction API.
- [x] Advance frame order only on successful commit; preserve frame ID on cancelled/failed recording.
- [x] Block concurrent process/config mutation while a recording transaction is pending.
- [x] Separate Vulkan slot recording state from submitted timestamp state.
- [x] Make timing collection an explicit post-fence operation; require host fence before slot reuse.
- [x] Validate source linear-filter feature support as well as sampled-image support.
- [x] Reset temporal history on scene cuts and frame-ID gaps.
- [x] Document callback payload vs GPU object lifetime, queue order and cancellation preconditions.
- [x] CI build, transaction contract tests and full Vulkan graph on Mesa lavapipe with synchronization validation (run #42521641315).
- [x] Add a separately compiled C client using the frozen pre-transaction V1 ABI header; CTest passes on lavapipe (run #42522045599).
- [x] Add a real Vulkan host transaction test covering abandon/reset/cancel/retry/submit/commit/fence and command-pool reuse (run #42522045599; test backend double).
- [x] Extend the full Vulkan FG runtime smoke test to use slot 1 and then reuse slot 0 only after each submission fence (run #42523914455; Mesa lavapipe).
- [x] Add pinned Android NDK arm64-v8a cross-build and ADB smoke runner (Phase 3.3; Linux + Android arm64 CI PASS, runs #42565368872 and #42565464843).
- [x] Run standalone direct-Termux Vulkan smoke on physical ASUS AI2205 / Snapdragon SM8550 / Adreno 740 using the system Vulkan implementation; record report for binary commit `fef098f` (validation layers off).
- [ ] Identify the exact Vulkan driver and repeat with pinned Mesa Turnip revisions; test Qualcomm proprietary driver separately where available.
- [ ] Review/approve Phase 3 PR stack; resolve PR #9 merge conflict state.
- [ ] Test borrowed-device integration with a real host/compositor; standalone smoke is not sufficient.
- [ ] Define robust device-loss recovery and a complete output format conversion/fallback policy.

## Phase 3.4 — borrowed-device host integration validation (next gate)

- [ ] Add a host-owned Vulkan interop example that passes borrowed instance/device/queue/command buffer and host images through the public C API.
- [ ] Verify command recording, submit/commit, abandon/reset/cancel, fence retirement and slot reuse without AromaFG-owned submission.
- [ ] Run with Khronos validation + synchronization validation enabled where available.
- [ ] Capture Vulkan driver properties, format features, queue limits and timestamp support.
- [ ] Keep Winlator/DXVK adapter work blocked until the host-side sample and lifecycle contract pass.

## Phase 4 — lightweight Frame Generation MVP

- [ ] Implement and validate a low-cost classical flow algorithm.
- [ ] Reuse one flow solution for all chronological synthesis phases.
- [ ] Establish real-frame fallback and scene-cut handling.
- [ ] Compare against true midpoint ground truth on a reproducible dataset.
- [ ] Measure per-stage GPU time, memory, latency and thermal behavior.

## Phase 5 — Vulkan backend stabilization

- [ ] Fix or redesign command-buffer cancellation and query/resource state tracking.
- [ ] Add regression coverage for last-completed per-slot timings in `AromaFGFrameOutput`.
- [ ] Reduce full-resolution flow/seed intermediate memory; implement configurable flow resolution and reuse scratch buffers.\n- [ ] Add memory suballocation and memory-budget-aware resource pooling.
- [ ] Add robust capability/format checks and device-loss recovery.
- [ ] Validate descriptor/query slot reuse and resize on real drivers.
- [ ] Add in-host Android AHardwareBuffer/synchronization integration tests.

## Phase 6 — Adreno / Turnip optimization

- [ ] Test on named Adreno devices and pinned Turnip revisions.
- [ ] Profile workgroup sizes, search radius, flow resolution and bandwidth at 720p/1080p/1440p on Adreno.\n- [ ] Establish a measured performance baseline beyond the 320×180 smoke workload.
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
- [ ] Calibrate confidence/risk metrics against true high-frame-rate ground truth.

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
