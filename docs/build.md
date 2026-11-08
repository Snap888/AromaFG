# Build

## Build modes and maturity

The repository has a portable C ABI/lifecycle target on main. The experimental Vulkan runtime is on feature/vulkan-engine-runtime and its dependent feature branches. Build success is not evidence of physical-device support or production image quality.

## Portable build

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAROMA_FG_ENABLE_VULKAN=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
~~~

This validates the portable lifecycle/contract layer only.

## Vulkan runtime build

Requirements:

- CMake 3.20 or newer.
- C++20 compiler.
- Vulkan development headers and loader for the build/test host.
- glslangValidator for offline GLSL → SPIR-V compilation.
- Python 3 for embedding generated SPIR-V.
- spirv-val is used when available for SPIR-V validation.
- Mesa lavapipe and Khronos Validation Layers are needed for the same software smoke path used in CI.

~~~sh
cmake -S . -B build-vk -DCMAKE_BUILD_TYPE=Release \
  -DAROMA_FG_ENABLE_VULKAN=ON \
  -DAROMA_FG_COMPILE_SHADERS=ON \
  -DAROMA_FG_BUILD_TESTS=ON
cmake --build build-vk --parallel
ctest --test-dir build-vk --output-on-failure
~~~

## Vulkan device smoke test

~~~sh
cmake -S . -B build-smoke -DCMAKE_BUILD_TYPE=Release \
  -DAROMA_FG_ENABLE_VULKAN=ON \
  -DAROMA_FG_COMPILE_SHADERS=ON \
  -DAROMA_FG_BUILD_TESTS=ON \
  -DAROMA_FG_BUILD_DEVICE_TESTS=ON
cmake --build build-smoke --parallel
~~~

Run aroma_fg_device_smoke only in an environment with a suitable Vulkan ICD and validation layers. CI forces Mesa lavapipe. The smoke executable creates its own device; it is not a Winlator in-host test.

## Android NDK

An Android cross-build is a compilation check, not a runtime validation. Pin NDK, CMake, Vulkan-Headers and shader compiler versions in CI. The host supplies the Vulkan loader/ICD; AromaFG must not bundle or load a driver. Use the exact NDK toolchain and ANDROID_ABI=arm64-v8a supported by the selected environment. Physical Turnip and Qualcomm tests are separate release gates.

## Shader toolchain

Compile GLSL offline to SPIR-V during build/release. Runtime shader compilation/file lookup is not required. Validate the SPIR-V target environment consistently with the Vulkan runtime contract.
