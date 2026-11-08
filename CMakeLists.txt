cmake_minimum_required(VERSION 3.20)
project(AromaFG VERSION 0.1.0 LANGUAGES C CXX)

option(AROMA_FG_BUILD_TESTS "Build AromaFG portable contract tests" ON)
option(AROMA_FG_BUILD_SHARED "Build shared AromaFG library" ON)
option(AROMA_FG_COMPILE_SHADERS "Compile GLSL shaders when glslangValidator is available" ON)
option(AROMA_FG_BUILD_DEVICE_TESTS "Build the physical Vulkan device smoke test (not run by CI)" OFF)
option(AROMA_FG_ENABLE_VULKAN "Build Vulkan interop and orchestration helpers" ON)

if(AROMA_FG_ENABLE_VULKAN)
  find_package(Vulkan REQUIRED)
endif()

set(AROMA_FG_LIBRARY_TYPE STATIC)
if(AROMA_FG_BUILD_SHARED)
  set(AROMA_FG_LIBRARY_TYPE SHARED)
endif()

add_library(aroma_fg ${AROMA_FG_LIBRARY_TYPE}
  src/api/aroma_fg.cpp
  src/core/engine.cpp
)

if(AROMA_FG_ENABLE_VULKAN)
  if(NOT AROMA_FG_COMPILE_SHADERS)
    message(FATAL_ERROR "AROMA_FG_ENABLE_VULKAN requires AROMA_FG_COMPILE_SHADERS=ON")
  endif()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(AROMA_FG_GLSLANG_VALIDATOR glslangValidator REQUIRED)
  find_program(AROMA_FG_SPIRV_VAL spirv-val)
  target_sources(aroma_fg PRIVATE
    src/vulkan/vulkan_backend.cpp
    src/vulkan/vulkan_runtime.cpp)
  target_link_libraries(aroma_fg PUBLIC Vulkan::Vulkan)
  target_include_directories(aroma_fg PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(GLOB AROMA_FG_RUNTIME_SHADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl/*.comp")
  set(AROMA_FG_RUNTIME_SPV)
  set(AROMA_FG_EMBED_ARGS)
  foreach(SHADER IN LISTS AROMA_FG_RUNTIME_SHADERS)
    get_filename_component(SHADER_NAME "${SHADER}" NAME_WE)
    set(SPV "${CMAKE_CURRENT_BINARY_DIR}/generated/spv/${SHADER_NAME}.spv")
    add_custom_command(OUTPUT "${SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/spv"
      COMMAND "${AROMA_FG_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1 -S comp "${SHADER}" -o "${SPV}"
    DEPENDS "${SHADER}"
      VERBATIM)
    if(AROMA_FG_SPIRV_VAL)
      add_custom_command(OUTPUT "${SPV}.validated"
        COMMAND "${AROMA_FG_SPIRV_VAL}" --target-env vulkan1.1 "${SPV}"
        COMMAND ${CMAKE_COMMAND} -E touch "${SPV}.validated"
        DEPENDS "${SPV}"
        VERBATIM)
      list(APPEND AROMA_FG_RUNTIME_SPV "${SPV}.validated")
    else()
      list(APPEND AROMA_FG_RUNTIME_SPV "${SPV}")
    endif()
    list(APPEND AROMA_FG_EMBED_ARGS "${SHADER_NAME}_comp=${SPV}")
  endforeach()
  set(AROMA_FG_EMBEDDED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_shaders.hpp")
  add_custom_command(OUTPUT "${AROMA_FG_EMBEDDED_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_spv.py"
            "${AROMA_FG_EMBEDDED_HEADER}" ${AROMA_FG_EMBED_ARGS}
    DEPENDS ${AROMA_FG_RUNTIME_SPV} "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_spv.py"
    VERBATIM)
  add_custom_target(aroma_fg_shaders DEPENDS "${AROMA_FG_EMBEDDED_HEADER}")
  add_dependencies(aroma_fg aroma_fg_shaders)
endif()
add_library(AromaFG::AromaFG ALIAS aroma_fg)
target_include_directories(aroma_fg PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(aroma_fg PUBLIC cxx_std_20)
target_compile_definitions(aroma_fg PRIVATE AROMA_FG_BUILD=1)
set_target_properties(aroma_fg PROPERTIES
  OUTPUT_NAME aroma_fg
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
if(MSVC)
  target_compile_options(aroma_fg PRIVATE /W4 /permissive-)
else()
  target_compile_options(aroma_fg PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AROMA_FG_BUILD_DEVICE_TESTS)
  if(NOT AROMA_FG_ENABLE_VULKAN)
    message(FATAL_ERROR "AROMA_FG_BUILD_DEVICE_TESTS requires AROMA_FG_ENABLE_VULKAN=ON")
  endif()
  add_executable(aroma_fg_device_smoke tests/device_vulkan_smoke.cpp)
  target_link_libraries(aroma_fg_device_smoke PRIVATE AromaFG::AromaFG Vulkan::Vulkan)
  target_compile_features(aroma_fg_device_smoke PRIVATE cxx_std_20)
endif()

if(AROMA_FG_BUILD_TESTS)
  enable_testing()
  add_executable(aroma_fg_contract_tests tests/test_engine.cpp)
  target_link_libraries(aroma_fg_contract_tests PRIVATE AromaFG::AromaFG)
  target_compile_options(aroma_fg_contract_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>")
  add_test(NAME aroma_fg_contract_tests COMMAND aroma_fg_contract_tests)

  # Compile this client against the frozen pre-transaction V1 header in a
  # separate C translation unit to exercise the real binary ABI boundary.
  add_executable(aroma_fg_legacy_abi_client tests/legacy_abi/legacy_abi_client.c)
  target_include_directories(aroma_fg_legacy_abi_client PRIVATE tests/legacy_abi)
  target_link_libraries(aroma_fg_legacy_abi_client PRIVATE AromaFG::AromaFG)
  set_target_properties(aroma_fg_legacy_abi_client PROPERTIES LINKER_LANGUAGE CXX)
  add_test(NAME aroma_fg_legacy_abi_client COMMAND aroma_fg_legacy_abi_client)

  if(AROMA_FG_ENABLE_VULKAN)
    add_executable(aroma_fg_vulkan_host_transaction tests/vulkan_host_transaction.cpp)
    target_link_libraries(aroma_fg_vulkan_host_transaction PRIVATE AromaFG::AromaFG Vulkan::Vulkan)
    target_compile_features(aroma_fg_vulkan_host_transaction PRIVATE cxx_std_20)
    target_compile_options(aroma_fg_vulkan_host_transaction PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>")
    add_test(NAME aroma_fg_vulkan_host_transaction COMMAND aroma_fg_vulkan_host_transaction)
    add_executable(aroma_fg_vulkan_plan_tests tests/test_vulkan_plan.cpp)
    target_include_directories(aroma_fg_vulkan_plan_tests PRIVATE src)
    target_compile_options(aroma_fg_vulkan_plan_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>")
    target_link_libraries(aroma_fg_vulkan_plan_tests PRIVATE AromaFG::AromaFG)
    add_test(NAME aroma_fg_vulkan_plan_tests COMMAND aroma_fg_vulkan_plan_tests)
  endif()
endif()

# Runtime shaders are compiled and embedded above when Vulkan support is enabled.

include(GNUInstallDirs)
install(TARGETS aroma_fg EXPORT AromaFGTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
