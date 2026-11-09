# Physical device validation report — 2026-11-09

## Result

**PASS** — direct local Termux execution, no ADB.

This report records a standalone Vulkan smoke run on an ASUS AI2205 / Snapdragon SM8550 / Adreno 740 device. It does not establish Winlator/game integration or identify the exact Vulkan driver build.

## Device and runtime

| Field | Value |
|---|---|
| Manufacturer / model | ASUS / ASUS_AI2205_C |
| Device | ASUS_AI2205 |
| SoC | Qualcomm SM8550 |
| Board platform | Kalama |
| GPU reported by Vulkan | Adreno (TM) 740 |
| Android | 15 / API 35 |
| ABI | arm64-v8a |
| Vulkan API | 1.3.128 |
| Vulkan vendor ID | 5143 |
| Vulkan device ID | 43050a01 |
| Vulkan driverVersion | 2150252610 |
| Driver name / info | Not emitted by this smoke executable |
| Khronos validation | Off |
| Synchronization validation | Off |
| Execution | Direct Termux process; no ADB |

The report also recorded build fingerprint, security patch and kernel details locally. They are intentionally omitted here because they are not required to identify the GPU execution path.

## Build provenance

- AromaFG source commit: `fef098fa96a82a4a5c09fd50d901593f2ddf50cc`
- Android arm64 artifact SHA-256: `41c0882db8bc8d7e2772f0f7fc3814143173e317118a306f8a6be648c3f3abf8`
- Capture time: `2026-11-09T21:15:21Z`

GitHub Actions push run #42700505141 completed successfully for the same source commit. Its Android artifact metadata reported the same SHA-256.

## Test results

The Vulkan smoke workload uses **320×180** source/output images. Its recorded GPU timings were preprocessing `0.210 ms`, flow/refine `5.322 ms`, synthesis `0.205 ms`. These are measurements of this small synthetic workload only; do not extrapolate them as 1080p/1440p game performance.

| Test | Result |
|---|---:|
| `aroma_fg_contract_tests` | PASS — exit 0 |
| `aroma_fg_vulkan_plan_tests` | PASS — exit 0 |
| Vulkan loader preflight | Completed |
| `aroma_fg_device_smoke` | PASS — exit 0 |
| Runtime slot reuse | PASS — slots `[1,0]`, fence wait before pool/slot reuse |
| Frame generation smoke | PASS — generated 3 phases |
| Overall result | **0** |

Smoke output:

```text
DEVICE Adreno (TM) 740 vendor=5143 device=43050a01 driver=2150252610 api=1.3.128
RUNTIME_SLOT_REUSE=PASS slots=[1,0]
FG status=PASS generated=3 timing_status=0
```

The original report printed three generated phase color samples and stage timings. These are synthetic smoke-test outputs, not a perceptual quality benchmark against high-frame-rate ground truth.

## What this proves

- The exact Android arm64 artifact identified above loaded and ran in a direct Termux process.
- The Vulkan loader created a device on Adreno 740 and AromaFG recorded/submitted its standalone compute graph.
- Contract and Vulkan planner tests ran successfully on the same device.
- The tested runtime exercised cancellation/retry and sequential fence-backed slot reuse.
- The smoke test generated three chronological output phases and passed its readback checks.

## What this does not prove

- It is **not** a Mesa Turnip test: the process used the Android system Vulkan implementation.
- The driver name and driver info were not captured; do not label this result as a particular Qualcomm proprietary-driver revision.
- Validation layers and synchronization validation were disabled. A zero exit code is not evidence that validation-layer hazard checks passed.
- The smoke executable creates its own Vulkan instance, device, queue, command buffer and fence. It does not borrow a game's or Winlator's Vulkan device.
- No DXVK, Wine, Winlator, compositor, swapchain, acquire/present or physical scanout integration was tested.
- No sustained-load, thermal, resize, device-loss, output-format matrix, image-quality or latency benchmark was performed.
- This artifact predates the later per-slot timing-output fix commits; those changes require their own CI and device regression run.

## Next validation gates

1. Re-run on this device with Khronos validation and synchronization validation enabled, if the image supports them.
2. Capture `VkPhysicalDeviceDriverProperties` (`driverName`, `driverInfo`, `conformanceVersion`) and relevant format/queue/timestamp capabilities.
3. Run against pinned Mesa Turnip revisions as a separate driver configuration.
4. Run a borrowed-device host integration sample with a host-owned VkDevice/queue/command buffer and host-owned images.
5. Only then begin Winlator/DXVK-specific adapter work and sustained quality/performance validation.
