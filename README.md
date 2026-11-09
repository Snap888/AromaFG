# AromaFG

**Aroma Frame Generation** is a clean-room, Vulkan-based optical-flow frame-generation library for Android (Qualcomm Adreno + Mesa Turnip/proprietary drivers), Winlator/WinNative, DXVK-based hosts and native renderers.

> **Development status: experimental Vulkan runtime milestone.** The main branch remains the portable C ABI/lifecycle bootstrap. Feature branches contain the baseline Vulkan compute graph, host transaction hardening and fence-backed slot tests. CI includes Mesa lavapipe software execution and an Android arm64-v8a NDK cross-build. A standalone Vulkan smoke test has passed on a physical ASUS AI2205 / Snapdragon SM8550 / Adreno 740 using the system Vulkan implementation (validated binary commit `fef098f`). Khronos validation and synchronization validation were disabled; the report does not identify the driver as Turnip and does not validate a borrowed Winlator/game device. This is not yet a production-ready SDK.

## Goals

- Small embeddable .so / .dll with a stable, versioned C ABI.
- 2× / 3× / 4× temporal synthesis with distinct chronological phases.
- Bidirectional motion estimation and forward/backward consistency.
- Host-owned Vulkan interop: no private device, queue submission, WSI ownership or synchronous GPU waits.
- Adreno/Turnip-aware compute and memory behavior.
- Optional low-overhead diagnostics and deterministic workload policy.
- Native host-side integration first; Vulkan Layer path is optional and secondary.

## Current implementation snapshot

| Area | Current state |
|---|---|
| Portable C ABI / lifecycle | Present on main |
| GLSL optical-flow passes | Present on feature branches |
| Vulkan runtime graph | Present on feature branches |
| ×2 / ×3 / ×4 phase dispatch | Smoke-tested on Mesa lavapipe and standalone Adreno 740 system Vulkan; not a quality benchmark |
| Linux CI GPU validation | Mesa lavapipe only |
| Android arm64-v8a | NDK cross-build; direct Termux tests and standalone Vulkan smoke passed on ASUS AI2205 / Adreno 740 (binary `fef098f`) |
| Physical Adreno | Standalone system-driver smoke passed on Adreno 740; validation layers were off |
| Mesa Turnip / exact Qualcomm driver identity | Not independently identified or tested; report records Vulkan driverVersion only |
| Android SDK / Winlator adapters / Vulkan Layer | Not implemented |
| AI inference / learned refinement | Research only; not implemented |
| Production image quality and latency | Not established |

The Vulkan runtime branch currently supports RGBA8_UNORM and RGBA16_SFLOAT output slots. It requires the host-created device to have shaderStorageImageExtendedFormats enabled. See docs/vulkan-backend.md for the complete contract and limitations.

## Architecture principles

1. Estimate motion once per consecutive source-frame pair; reuse it for each generated phase.
2. Keep capture, command submission, synchronization, swapchain acquisition and presentation owned by the host.
3. A successful Vulkan aroma_fg_process() means commands were recorded, not submitted. The host must call aroma_fg_commit_frame() after successful queue submission, or abandon/reset the command buffer and call aroma_fg_cancel_frame().
4. Frame IDs advance only on transaction commit; cancelled recordings may be retried with the same ID.
5. Borrowed Vulkan handles must remain alive until recorded commands complete; callback payload lifetime and GPU resource lifetime are distinct.
6. Bypass safely on invalid input, unsupported format/capability, scene discontinuity or backend failure.
7. Separate compute completion, swapchain acquisition, successful present and physical scanout in metrics.
8. Treat confidence/risk as measured proxies until calibrated against true high-frame-rate ground truth.
9. Classical optical flow and warping remain the default; learned refinement is optional research only.

## Baseline quality pipeline

luma → pyramid → forward/backward block flow → consistency refinement → phase-specific synthesis

Planned experiments include global-motion pre-warp (C1), edge-preserving flow regularization (C2), occlusion-aware candidate reconstruction, anti-ghost fallback and sharpening. Each must be independently switchable and measured.

## Repository map

- include/ — public C ABI and Vulkan interop API.
- src/core/ — lifecycle and portable engine.
- src/vulkan/ — Vulkan helpers and compute runtime.
- shaders/glsl/ — GLSL compute sources; offline SPIR-V compilation.
- tests/ — contract, planner and Vulkan smoke tests.
- tools/ — shader utilities and Android device smoke runner.
- docs/ — architecture, integration, validation and research notes.

## Documentation

- [Architecture](docs/architecture.md)
- [Vulkan host contract](docs/vulkan-host-contract.md)
- [Vulkan runtime backend](docs/vulkan-backend.md)
- [Adreno + Turnip test plan](docs/adreno-turnip-test-plan.md)
- [Integration guide](docs/integration.md)
- [Engineer Mode and telemetry](docs/engineer-mode.md)
- [Testing and benchmark plan](docs/testing.md)
- [Build](docs/build.md)
- [Roadmap](ROADMAP.md)
- [Phase 1 documentation baseline](docs/phase-1-status.md)
- [Phase 2 technology research](docs/phase-2-research.md)
- [Phase 3 lifecycle hardening status](docs/phase-3-status.md)
- [Phase 3.2 ABI and fence-backed runtime slot validation](docs/phase-3-2-status.md)
- [Phase 3.3 Android arm64 build and device validation](docs/phase-3-3-status.md)
- [Vulkan host transaction contract](docs/vulkan-host-contract.md)
- [Physical Adreno 740 smoke report (2026-11-09)](docs/physical-device-validation-2026-11-09.md)
- [Repository audit (2026-11-09)](docs/audit-2026-11-09.md)

## Build and test

The portable baseline:

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
~~~

This currently validates the portable lifecycle contract skeleton. The Vulkan runtime branch additionally requires Vulkan development headers/loader, glslangValidator, Python 3 and (for SPIR-V validation) spirv-val. See docs/build.md. A successful lavapipe test is not evidence of Adreno compatibility.

## Clean-room and licensing

AromaFG is an independent implementation. External repositories are studied for architecture, invariants and experiments; their code, shaders, model weights and datasets are not automatically reusable. Each imported artifact requires file-level provenance and license review. Commercial or obfuscated frame-generation dumps are not used.

AromaFG is licensed under MPL-2.0.
