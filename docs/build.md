# Build

## Build modes and maturity

The repository has a portable C ABI/lifecycle target on main. The experimental Vulkan runtime is on feature branches. Build success is not evidence of physical-device support or production image quality.

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

Run aroma_fg_device_smoke only in an environment with a suitable Vulkan ICD. CI forces Mesa lavapipe. The smoke executable creates its own device; it is not a Winlator in-host test. Validation layers are enabled only when exposed by the runtime.

## Android arm64-v8a cross-build

The CI job pins Android NDK r27d (27.2.12479018), targets Android API 26 and compiles the shared library, contract tests and standalone Vulkan device smoke executable:

~~~sh
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
cmake -S . -B build-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DAROMA_FG_ENABLE_VULKAN=ON \
  -DAROMA_FG_COMPILE_SHADERS=ON \
  -DAROMA_FG_BUILD_TESTS=ON \
  -DAROMA_FG_BUILD_DEVICE_TESTS=ON
cmake --build build-android --parallel
~~~

A cross-build is a compilation check only; it does not execute tests on an Android device. The Android platform supplies the Vulkan loader/ICD. AromaFG must not bundle, replace or load a Vulkan driver.

## Run the standalone smoke on a connected Android device

With USB debugging enabled and the device authorized by adb:

~~~sh
tools/run_android_device_smoke.sh build-android
~~~

The script records Android build/SoC properties and kernel metadata, pushes libaroma_fg.so plus aroma_fg_device_smoke, runs the standalone Vulkan graph against the device's system Vulkan implementation and writes aromafg-device-report.txt. The executable creates its own Vulkan instance/device/queue; this is not an integration test against a running game, Winlator or an existing host VkDevice. Preserve the report with the exact AromaFG commit and driver configuration.

## Shader toolchain

Compile GLSL offline to SPIR-V during build/release. Runtime shader compilation/file lookup is not required. Validate the SPIR-V target environment consistently with the Vulkan runtime contract.
