# Build

## Current state

The repository is being bootstrapped documentation-first. Do not treat the documentation or planned C API as a working Vulkan implementation until the corresponding targets and tests are present.

## Planned host build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Android NDK target (planned)

Use Android NDK CMake toolchain and arm64-v8a. Pin NDK, CMake, Vulkan-Headers and shader compiler versions in CI. The host supplies the Vulkan loader/ICD; AromaFG must not bundle or load a driver.

## Shader toolchain

Compile GLSL offline to SPIR-V during build/release. Validate with spirv-val for the selected Vulkan target environment. Runtime compilation is not required.

## Validation

A successful cross-compile proves only compilation. Physical Turnip/Adreno validation is a separate milestone.