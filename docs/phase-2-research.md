# Phase 2 — Research and Technology Selection

**Snapshot date:** 2026-11-08  
**Branch:** `research/phase-2-technology-selection`  
**Scope:** research and documentation only. No AromaFG source, shader, test, build or runtime files are changed.

## 1. Executive decision

1. Keep the existing block-matching implementation as the deterministic reference and fallback while the next algorithm is evaluated.
2. Select **DIS-style dense optical flow as the first alternative to prototype and benchmark**. Do not copy WFG/OpenCV source or shaders in this phase. Implement independently from the published algorithm description, with file-level provenance.
3. Keep Farneback as an optional coarse-level/hybrid experiment, not the default standalone estimator.
4. Keep Lucas–Kanade for sparse/global-motion estimation (C1), not as the sole dense-flow engine.
5. Do not add a neural runtime or model to the MVP. AI remains an optional later refinement path. ncnn is the first runtime to investigate for a Vulkan-oriented prototype; QNN is a device-specific alternative only after the exact SoC is identified. MNN and ONNX Runtime remain comparison candidates, not dependencies.
6. MVP target: **×2 first**, host-side integration, real-frame fallback, one flow solution per source pair, no CPU full-frame readback, and measurable quality/performance gates. ×3/×4 follow only after ×2 passes.

This is a research selection, not a claim that DIS has already been implemented or tested in AromaFG.

## 2. Repository and revision register

The revisions below are immutable review anchors checked for this report. Re-check upstream before any later import or implementation. A repository-level license is not sufficient to clear model weights, datasets, generated binaries or third-party subtrees.

| Candidate | Reviewed revision | What it is useful for | Decision |
|---|---|---|---|
| [WinNative WFG](https://github.com/WinNative-Emu/WinNative/tree/239ab0ea8ee3b1cdb03e5b09aaf8e4c60228f739/app/src/main/cpp/wfg) | `239ab0ea8ee3b1cdb03e5b09aaf8e4c60228f739` (2026-10-25) | Vulkan DIS implementation, Farneback/DIS hybrid, Android/Adreno experience | Highest-priority algorithm/performance reference; no copy in this phase |
| [Practical-RIFE](https://github.com/hzwer/Practical-RIFE/tree/bbfd2ea90910789a860ea3e2b32a240cd577b75e) | `bbfd2ea90910789a860ea3e2b32a240cd577b75e` (2026-08-27) | Practical learned interpolation models, scale and multi-frame choices | Optional AI quality reference |
| [rife-ncnn-vulkan](https://github.com/nihui/rife-ncnn-vulkan/tree/a7532fc3f9f8f008cd6eecd6f2ffe2a9698e0cf7) | `a7532fc3f9f8f008cd6eecd6f2ffe2a9698e0cf7` (2022-10-23) | C++/ncnn/Vulkan inference deployment example | Runtime integration reference; old upstream revision |
| [ncnn](https://github.com/Tencent/ncnn/tree/00357e2f20ea2f8245272c6fa1e443e15a4f6bfa) | `00357e2f20ea2f8245272c6fa1e443e15a4f6bfa` (2026-11-08) | Mobile C++ inference, ARM CPU and Vulkan backends | First AI runtime to prototype if/when needed |
| [MNN](https://github.com/alibaba/MNN/tree/5b74a931887ba5b4d9f65b222cc18dd1a16599a8) | `5b74a931887ba5b4d9f65b222cc18dd1a16599a8` (2026-11-07) | Mobile inference alternative, Vulkan backend | Compare later; not an MVP dependency |
| [IFRNet](https://github.com/ltkong218/IFRNet/tree/b117bcafcf074b2de756b882f8a6ca02c3169bfe) | `b117bcafcf074b2de756b882f8a6ca02c3169bfe` (2024-02-19) | Compact encoder-decoder VFI research candidate | Optional learned-refinement candidate |
| [RAFT](https://github.com/princeton-vl/RAFT/tree/2888e15a51fa41140771d3f498ed8023cff098d1) | `2888e15a51fa41140771d3f498ed8023cff098d1` (2025-08-24) | Offline optical-flow quality/reference baseline | Research reference only; not mobile runtime |
| [AMT](https://github.com/MCG-NKU/AMT/tree/70f988fbfc0d3d458beba1ee49caf876e57968fe) | `70f988fbfc0d3d458beba1ee49caf876e57968fe` (2023-04-26) | High-quality VFI research comparison | Exclude from AromaFG distribution: non-commercial license |
| [OpenCV 4.x](https://github.com/opencv/opencv/tree/e7d32cb9db8017fa6eebb2a4e9fc8c7b87f5bed3) | `e7d32cb9db8017fa6eebb2a4e9fc8c7b87f5bed3` (2026-11-08) | Reference implementations and algorithm comparison | Reference only; Apache-2.0 for 4.5+ |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) | Moving upstream; pin exact SDK tag before any use | Frame interpolation stages, disocclusion and swapchain/pacing separation | Algorithm/API reference only |

### Revision caveats

- WFG's immutable review anchor is the WinNative commit that explicitly records the WFG submodule's MIT relicensing and a Farneback/DIS hybrid update. The enclosing WinNative project has a different license; do not infer the WFG subtree's license for the whole application.
- The RIFE ncnn repository's reviewed commit is old. It proves the implementation pattern exists, not that its dependencies, models or build scripts are current.
- The model repositories' current commits do not pin externally hosted checkpoint archives. Before any model is downloaded or redistributed, record its exact filename, URL, version, SHA-256, model card and license statement.

## 3. File/artifact license and provenance matrix

| Artifact | Observed license/status | AromaFG decision |
|---|---|---|
| WFG implementation subtree at the pinned commit | MIT license at `app/src/main/cpp/wfg/LICENSE`; license notice also attributes DIS and Farneback lineage to OpenCV | Can be considered for a later, file-by-file reuse review. Current choice is independent implementation from algorithm literature; preserve MIT notice if any file is ever imported |
| OpenCV 4.5+ reference code | Apache-2.0; older OpenCV releases used 3-clause BSD | Do not paste source into AromaFG. If any code is reused later, retain exact version and notices and audit each file |
| WinNative host/compositor | Root project is GPL-3.0; WFG is a separately licensed subtree | Do not import the application or compositor into the MPL-2.0 AromaFG library |
| LSFG-Android application | Custom license: no commercial use / no commercial app-store distribution; the bundled `lsfg-vk-android` subtree is MIT | Do not reuse the Android application. Study only documented Android capture/AHardwareBuffer integration constraints |
| Lossless Scaling shader assets | Proprietary; LSFG-Android states shaders are extracted from the user's own purchased DLL and are not redistributed | **Never copy, extract, bundle or derive AromaFG shaders from these assets** |
| Practical-RIFE source | MIT | Research candidate. MIT source license does not automatically clear all third-party dependencies or every externally hosted checkpoint |
| Practical-RIFE model downloads | README states linked trained models are under the same MIT license | Treat as a publisher assertion, not a substitute for artifact-level verification. Record the exact model/version/hash and retain the linked license context before any use |
| IFRNet source | MIT | Research candidate; source can be evaluated independently |
| IFRNet pretrained checkpoints | Downloaded separately; the reviewed README does not provide an explicit per-checkpoint license statement alongside the download | **Uncleared**; do not bundle or redistribute pending written artifact-level provenance |
| RIFE ncnn Vulkan source | MIT | Runtime example only; do not infer model-weight rights from source license |
| ncnn source | BSD 3-Clause, with separately listed third-party components/licenses | Viable optional dependency after dependency and Vulkan-device interop review |
| MNN source | Apache-2.0 | Viable comparison candidate; not selected as dependency |
| RAFT source | BSD 3-Clause | Offline reference only; pretrained weights and training data require separate review |
| AMT source/model project | CC BY-NC 4.0 | **Exclude** from any commercial-capable AromaFG implementation or distribution |
| AMD FidelityFX SDK / FSR3 | Official FidelityFX FSR3 material is published under MIT | Useful technical reference; pin the exact SDK tag and audit all bundled third-party notices before considering source reuse |
| QNN SDK | Proprietary Qualcomm SDK; not an open-source runtime dependency | Optional device-specific backend only; do not bundle SDK assets without applicable Qualcomm terms |
| Vimeo90K / Adobe240 / Sintel | Dataset-specific terms and redistribution rights must be reviewed separately from code licenses | Do not bundle datasets or checkpoints. Store local dataset paths and metadata; use only under the dataset's terms |

**License policy:** AromaFG's MPL-2.0 does not make third-party artifacts automatically compatible or clear. Maintain a `THIRD_PARTY_NOTICES.md`/provenance ledger for every imported source file, shader, binary, model and dataset. Mark modified files. Do not ship an artifact with unresolved ownership or license terms.

## 4. Optical-flow algorithm comparison

| Method | Strengths | Weaknesses / mobile risks | Recommended role |
|---|---|---|---|
| Current pyramid block matching / SAD | Simple, deterministic, easy to validate and fully control in Vulkan; good reference for regression | Naive exhaustive search grows rapidly with radius, patch area and pixel count; repeated texture patterns and occlusion confuse matching | Keep as baseline/reference and fallback while DIS is evaluated |
| Lucas–Kanade (sparse/dense variants) | Efficient local differential estimation; useful for stable feature tracks and global affine camera motion | Sparse LK does not provide a complete dense field; local linearization struggles with large displacement, blur and low texture | C1 global-motion estimate / affine pre-warp; not the sole dense-flow engine |
| Farneback | Dense flow; polynomial expansion can capture smooth motion and provide a coarse initial field | Multi-scale windowed least-squares work can be costly; may oversmooth boundaries and fail on occlusion/large discontinuities | Optional coarse pyramid level or hybrid initializer |
| DIS (Dense Inverse Search) | Patch inverse search, dense displacement aggregation and variational refinement; designed for low time complexity and competitive accuracy | Several stages/intermediate fields; GPU implementation must control bandwidth, synchronization, precision and occupancy; quality still needs game-scene tests | **First alternative to prototype and A/B against current block matching** |
| RAFT / GMFlow-class learned flow | Strong offline reference for difficult motion and quality ceiling | Model memory, all-pairs/correlation or transformer compute, training framework and deployment complexity; not an assumed mobile realtime fit | Offline quality oracle / research only |
| RIFE / IFRNet / other learned VFI | Jointly learns motion representation and synthesis; can improve difficult interpolation artifacts | Model latency, activation memory, thermal cost, operator support and checkpoint rights; may require an independent Vulkan device/runtime | Optional later refinement, not baseline |

### DIS reference evidence

The DIS paper describes three core elements: inverse patch search, dense displacement aggregation across scales, and variational refinement. Its authors report high CPU throughput in their paper; those paper numbers are not transferable to an Adreno GPU.

The reviewed WFG commit reports a Farneback/DIS hybrid and an Adreno 750 flow-time result of 2.36 ms per source pair in its own 1280×720, 180p-flow, ×4-from-30-fps benchmark. This is useful evidence that a carefully implemented DIS-family Vulkan path can be practical on one newer Adreno device. It is **not** an AromaFG benchmark, is not an Adreno 660 result, and must not be extrapolated linearly to another resolution or GPU.

WFG's hybrid in that commit uses Farneback at coarser levels, DIS patch search at finer levels, an additional Farneback initial estimate for the finest level, and a final local refinement. This is a good experiment to reproduce independently after a simpler DIS-only variant has a correctness baseline.

### Selected algorithm experiment order

1. Keep the existing AromaFG block matcher as the control.
2. Implement a minimal DIS-like coarse-to-fine GPU estimator independently, initially at reduced flow resolution.
3. Add forward/backward consistency and confidence gates; evaluate on occlusion/disocclusion cases.
4. Compare DIS-only against a Farneback-coarse + DIS-fine hybrid.
5. Only after these comparisons, decide whether the runtime branch should replace or supplement block matching.
6. Evaluate C1 affine Lucas–Kanade as a separate feature flag. Do not combine C1/C2 changes into the first DIS A/B test.
7. Evaluate TV-L1/C2 and advanced anti-ghost as separate experiments after flow accuracy and runtime are stable.

## 5. Mobile inference runtime evaluation

| Runtime | Relevant capabilities | Risks / open questions | Current decision |
|---|---|---|---|
| ncnn | C++ mobile framework; ARM CPU/NEON and Vulkan GPU paths; BSD-3-Clause; RIFE ncnn implementation demonstrates a Vulkan inference pipeline | Confirm it can safely share the host's exact VkDevice/queue/allocator and synchronization model without a second device or hidden submit/wait. Audit all model files separately | First optional prototype candidate |
| MNN | Apache-2.0; mobile inference with CPU and Vulkan support | Same external Vulkan context/lifetime question; model/operator support and Adreno performance must be measured | Keep as alternative |
| ONNX Runtime | MIT; Android CPU/XNNPACK and NNAPI EP; QNN EP on supported Qualcomm devices | NNAPI partitioning/fallback is model/device-specific; binary size and operator coverage; QNN requires proprietary SDK and compatible SoC/runtime | Evaluate only after a concrete model exists |
| Qualcomm QNN / AI Engine Direct | Hardware-accelerated Qualcomm path; ONNX Runtime documentation reports QNN EP tested with QNN 2.22.x and SM8350 among tested platforms | Proprietary SDK, chipset/backend support, device-specific deployment and operator constraints; target device SoC not yet verified | Conditional option for confirmed supported Qualcomm hardware |
| Direct Vulkan compute | Host-owned Vulkan context and explicit synchronization fit AromaFG's architectural contract | Neural operators, weights, memory and execution graph become AromaFG's maintenance burden | Preferred for classical flow; do not build a neural runtime from scratch in Phase 2 |

No runtime is selected as a required dependency. Before choosing one, prove:
- it can consume the host's existing Vulkan device/context or explicitly document a safe separate-device path;
- no hidden queue submission, device-wide wait or full-frame CPU copy occurs;
- all tensors can remain GPU-resident from input to output;
- operator coverage matches the chosen model without costly CPU fallback;
- p50/p95/p99 latency, peak activation memory and sustained thermal behavior meet the same gates as the classical pipeline.

The current device model string does not establish the SoC/GPU/driver. Capture the actual SoC, Vulkan physical-device properties, driver ID/version, extensions and format capabilities from the test device before choosing QNN or any Adreno-specific path.

## 6. Benchmark datasets and ground-truth protocol

### Dataset roles

| Dataset / source | What it measures | Limitations and use |
|---|---|---|
| Vimeo90K septuplets | Standard 2× frame-interpolation quality comparisons; multiple consecutive frames | Useful for model research and repeatable VFI metrics; review dataset terms and do not redistribute |
| Adobe240fps | Real captured high-frame-rate footage; useful for larger motion and 8× interpolation experiments | Not game content; use only under dataset terms |
| MPI Sintel | Dense optical-flow ground truth, large displacement, motion blur, reflections and unmatched/occluded regions | Strong flow-estimator validation; synthetic animated scenes are not a substitute for game captures |
| Own high-refresh game/host captures | Actual HUD, camera turns, particles, scrolling, UI and emulator/compositor behavior | Essential target-domain test; keep consent/rights and exact capture metadata |

### Ground-truth capture protocol

For true midpoint comparison, capture a deterministic scene at a higher real cadence (for example 120 Hz), then form a 60 Hz input pair by selecting every other captured frame. The omitted real 120 Hz frame is the ground-truth midpoint. Do not synthesize the ground truth by blending the two input frames.

Capture at least these scene classes:
- static camera with moving objects;
- slow pan and fast camera turn;
- forward/backward movement with strong parallax;
- thin geometry, foliage, particles, transparency and specular highlights;
- HUD/text, scrolling UI and subtitles;
- disocclusion at object boundaries;
- scene cut, pause/menu transition and frame-time spike.

Record original capture cadence, dropped/duplicated capture frames, game/host version, resolution, color format/transfer function, driver, GPU clocks/temperature where available, and the exact source frame IDs.

### Metrics

- Image quality: PSNR and SSIM for reproducible pixel metrics; LPIPS for perceptual comparison where licensing/runtime permits.
- Flow quality: endpoint error (EPE), bad-pixel rate and occlusion/unmatched-region reporting on datasets with ground-truth flow.
- Temporal quality: temporal warp error and frame-to-frame consistency; report separately for static background, moving objects and disocclusion masks.
- Artifact rates: ghost/double-edge area, hole/uncovered area, HUD corruption and scene-cut false generation.
- Runtime: per-pass GPU timestamps, total FG GPU time (p50/p95/p99), CPU submission overhead and queue wait.
- Delivery: generated frames submitted, accepted by present, dropped/late frames and cadence error. Do not call a generated frame “displayed” without host/display evidence.
- Platform: private GPU memory, peak allocation, power/energy if measurable, sustained clock and thermal throttling during a 15-minute run.

## 7. Proposed MVP budgets (targets, not measured results)

Initial acceptance target for the first physical-device MVP:

| Metric | Proposed gate | Notes |
|---|---:|---|
| Multiplier | ×2 only | ×3/×4 after ×2 correctness and pacing pass |
| Input resolution | 1280×720 first; 1920×1080 stretch target | Record exact render and flow resolution |
| FG GPU time | p95 ≤ 8.3 ms at 1080p input on the named reference device | Engineering target for a 60→120 use case, not a claim of current performance |
| FG GPU time tail | p99 ≤ 12 ms | Track separately from present/pacing |
| Additional private GPU memory | ≤ 128 MiB at 1080p | Includes all intermediate images, descriptors are excluded; report actual allocation granularity separately |
| CPU overhead | No blocking wait and no full-frame readback | Hard architectural gate |
| Correctness | No invalid command stream; deterministic real-frame fallback on unsupported/error/cut cases | Must pass validation and slot-reuse tests |
| Quality | No regression against block-match control on the agreed high-motion/occlusion suite; publish PSNR/SSIM/LPIPS and artifact masks | No single scalar metric is sufficient |
| Sustained run | 15-minute continuous test without unbounded memory growth, deadlock or repeated frame starvation | Record thermals and clocks |

These are proposed engineering gates for discussion and tuning. They are not measured on the user's device or on any AromaFG build. If the named device cannot meet them, lower resolution/flow scale or disable generation; do not silently relax correctness or fallback rules.

## 8. Clean-room and integration boundaries

- Do not import LSFG proprietary shaders, shader dumps, DLL-extracted SPIR-V, obfuscated code or reverse-engineered binary fragments.
- Do not copy the WinNative application/compositor, its GPL-3.0 code or its Android packaging into AromaFG.
- WFG's MIT subtree may be legally reviewable as a separate unit, but the current plan is to implement the selected algorithm independently and retain a provenance ledger.
- Do not make AromaFG load Turnip, Qualcomm ICDs, Vortek, libadrenotools or other drivers. The host chooses and owns the Vulkan device/driver.
- Do not add BCn decompression, DXIL translation or emulator-specific driver shims to the FG library.
- Keep native host-side integration first. Vulkan Layer and Android MediaProjection overlay paths have different ownership, latency and platform constraints and must remain separate adapters.
- A neural runtime must be optional and must not change the classical fallback path.

## 9. Phase 2 exit checklist

- [x] Candidate repositories and review revisions recorded.
- [x] Source-code license matrix started with explicit subtree boundaries.
- [x] Classical optical-flow methods compared and DIS selected as the first A/B candidate.
- [x] Mobile inference runtimes compared; no mandatory dependency selected.
- [x] Dataset roles, high-FPS ground-truth capture protocol and metrics defined.
- [x] MVP budgets proposed and explicitly labeled as unmeasured targets.
- [ ] Before any source/model import: complete per-file provenance, model/checkpoint SHA-256 and dataset-rights review for the exact artifacts to be used.
- [ ] Phase 1 documentation PR #4 reviewed/approved; Phase 2 PR reviewed/approved.

**Phase 2 recommendation:** proceed to Phase 3 API and host-lifecycle hardening before implementing the new flow algorithm. The existing command-buffer commit/cancel and retry blockers are more fundamental than selecting a better optical-flow estimator. Once the host lifecycle is safe, run the DIS-vs-block-matching A/B as the first Phase 4 algorithm experiment.

## 10. Sources

- DIS paper: https://arxiv.org/abs/1603.03590
- WFG MIT subtree and implementation: https://github.com/WinNative-Emu/WinNative/tree/239ab0ea8ee3b1cdb03e5b09aaf8e4c60228f739/app/src/main/cpp/wfg
- WinNative credits and license boundaries: https://winnative-emu.github.io/WinNative/CREDITS.html
- Practical-RIFE source and model notes: https://github.com/hzwer/Practical-RIFE/tree/bbfd2ea90910789a860ea3e2b32a240cd577b75e
- RIFE ncnn Vulkan: https://github.com/nihui/rife-ncnn-vulkan/tree/a7532fc3f9f8f008cd6eecd6f2ffe2a9698e0cf7
- ncnn: https://github.com/Tencent/ncnn/tree/00357e2f20ea2f8245272c6fa1e443e15a4f6bfa
- MNN: https://github.com/alibaba/MNN/tree/5b74a931887ba5b4d9f65b222cc18dd1a16599a8
- IFRNet: https://github.com/ltkong218/IFRNet/tree/b117bcafcf074b2de756b882f8a6ca02c3169bfe
- RAFT: https://github.com/princeton-vl/RAFT/tree/2888e15a51fa41140771d3f498ed8023cff098d1
- AMT: https://github.com/MCG-NKU/AMT/tree/70f988fbfc0d3d458beba1ee49caf876e57968fe
- OpenCV license: https://opencv.org/license/
- OpenCV optical-flow overview: https://docs.opencv.org/4.x/d4/dee/tutorial_optical_flow.html
- ONNX Runtime mobile deployment: https://onnxruntime.ai/docs/tutorials/mobile/
- ONNX Runtime NNAPI EP: https://onnxruntime.ai/docs/execution-providers/NNAPI-ExecutionProvider.html
- ONNX Runtime QNN EP: https://onnxruntime.ai/docs/execution-providers/QNN-ExecutionProvider.html
- AMD FSR3 overview and MIT licensing: https://gpuopen.com/fidelityfx-super-resolution-3/
- MPI Sintel data and ground-truth flow: https://sintel.is.tue.mpg.de/downloads
- IFRNet dataset/benchmark references (Vimeo90K, SNU-FILM, Adobe240): https://github.com/ltkong218/IFRNet
