# AromaFG

**Aroma Frame Generation** is a clean-room, Vulkan-based optical-flow frame-generation library for Android (Qualcomm Adreno + Mesa Turnip/proprietary drivers), Winlator/WinNative, DXVK-based hosts and native renderers.

> Status: early bootstrap. The repository contains architecture documentation and a portable C ABI/lifecycle skeleton. It does not yet contain a production Vulkan optical-flow backend. Hardware support and image quality claims require physical-device validation.

## Goals

- Small embeddable `.so` / `.dll` with a stable, versioned C ABI.
- 2× / 3× / 4× temporal synthesis with distinct chronological phases.
- Bidirectional motion estimation, confidence and forward/backward consistency.
- Host-owned Vulkan interop: no private device, queue submission, WSI ownership or synchronous GPU waits.
- Adreno/Turnip-aware compute and memory behavior.
- Deterministic adaptive workload policy and low-overhead diagnostics.
- Native host-side integration first; Vulkan Layer path is optional and secondary.

## Architecture principles

1. Estimate motion once per consecutive source-frame pair; reuse it for each generated phase.
2. Keep capture, command submission, synchronization, swapchain acquisition and presentation owned by the host.
3. Keep all borrowed Vulkan handles scoped to the callback lifetime.
4. Bypass safely on invalid input, unsupported format/capability, scene discontinuity or backend failure.
5. Separate compute completion, swapchain acquisition, successful present and physical scanout in metrics.
6. Treat confidence/risk as measured proxies until calibrated against true high-frame-rate ground truth.
7. Classical optical flow and warping remain the default; learned refinement is optional research only.

## Quality pipeline

`luma → pyramid → forward/backward block flow → flow fusion → consistency → phase-specific warp/candidate reconstruction → composite`

Planned quality experiments: global-motion pre-warp (C1), edge-preserving flow regularization (C2), occlusion-aware candidate reconstruction, anti-ghost fallback and sharpening. Every experiment must be independently switchable and measured.

## Repository map

- `include/` — public C ABI.
- `src/core/` — lifecycle and portable engine skeleton.
- `src/vulkan/` — planned Vulkan interop and compute helpers.
- `src/telemetry/` — planned bounded presentation recorder and metrics.
- `shaders/glsl/` — planned compute shaders; offline SPIR-V compilation.
- `android/`, `examples/`, `tools/`, `scripts/` — planned platform, samples and tooling.
- `docs/` — architecture, integration, validation and research notes.

## Documentation

- [Architecture](docs/architecture.md)
- [Snap_AI_Engine branch analysis](docs/analysis-snap-ai-engine.md)
- [Algorithm decisions](docs/algorithm-decisions.md)
- [Host/Vulkan ownership contract](docs/vulkan-host-contract.md)
- [Integration guide](docs/integration.md)
- [Engineer mode and telemetry](docs/engineer-mode.md)
- [Validation and benchmark plan](docs/testing.md)
- [Roadmap](ROADMAP.md)

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

This currently validates only the portable lifecycle contract skeleton, not frame synthesis or Vulkan device execution. See [build.md](docs/build.md).

## Clean-room and attribution

AromaFG is an independent implementation. The analysis of [Snap888/Snap_AI_Engine](https://github.com/Snap888/Snap_AI_Engine/tree/SnapFrameBackendV1) is used to select architecture, invariants and experiments; this repository does not import its source files or claim its implementation is part of AromaFG. The analyzed upstream branch is MIT-licensed; see its [license](https://github.com/Snap888/Snap_AI_Engine/blob/SnapFrameBackendV1/LICENSE). AromaFG is licensed under [MPL-2.0](LICENSE).

Commercial or obfuscated frame-generation dumps are not used.
