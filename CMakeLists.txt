cmake_minimum_required(VERSION 3.20)
project(AromaFG VERSION 0.1.0 LANGUAGES C CXX)

option(AROMA_FG_BUILD_TESTS "Build AromaFG portable contract tests" ON)
option(AROMA_FG_BUILD_SHARED "Build shared AromaFG library" ON)
option(AROMA_FG_COMPILE_SHADERS "Compile GLSL shaders when glslangValidator is available" ON)
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
  target_sources(aroma_fg PRIVATE src/vulkan/vulkan_backend.cpp)
  target_link_libraries(aroma_fg PUBLIC Vulkan::Vulkan)
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

if(AROMA_FG_BUILD_TESTS)
  enable_testing()
  add_executable(aroma_fg_contract_tests tests/test_engine.cpp)
  target_link_libraries(aroma_fg_contract_tests PRIVATE AromaFG::AromaFG)
  target_compile_options(aroma_fg_contract_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>")
  add_test(NAME aroma_fg_contract_tests COMMAND aroma_fg_contract_tests)
  if(AROMA_FG_ENABLE_VULKAN)
    add_executable(aroma_fg_vulkan_plan_tests tests/test_vulkan_plan.cpp src/vulkan/vulkan_backend.cpp)
    target_include_directories(aroma_fg_vulkan_plan_tests PRIVATE src)
    target_compile_definitions(aroma_fg_vulkan_plan_tests PRIVATE AROMA_FG_BUILD=1)
    target_compile_options(aroma_fg_vulkan_plan_tests PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>" "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>")
    target_link_libraries(aroma_fg_vulkan_plan_tests PRIVATE AromaFG::AromaFG)
    add_test(NAME aroma_fg_vulkan_plan_tests COMMAND aroma_fg_vulkan_plan_tests)
  endif()
endif()

if(AROMA_FG_COMPILE_SHADERS)
  find_program(AROMA_FG_GLSLANG_VALIDATOR glslangValidator)
  find_program(AROMA_FG_SPIRV_VAL spirv-val)
  if(AROMA_FG_GLSLANG_VALIDATOR)
    file(GLOB AROMA_FG_GLSL_SHADERS CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl/*.comp")
    set(AROMA_FG_SPIRV_OUTPUTS)
    foreach(SHADER IN LISTS AROMA_FG_GLSL_SHADERS)
      get_filename_component(SHADER_NAME "${SHADER}" NAME)
      set(SPIRV_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
      add_custom_command(OUTPUT "${SPIRV_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${AROMA_FG_GLSLANG_VALIDATOR}" -V --target-env vulkan1.1 -S comp
                "${SHADER}" -o "${SPIRV_FILE}"
        COMMAND ${CMAKE_COMMAND} -E echo "Compiled ${SHADER_NAME}"
        DEPENDS "${SHADER}"
        VERBATIM)
      if(AROMA_FG_SPIRV_VAL)
        add_custom_command(OUTPUT "${SPIRV_FILE}.validated"
          COMMAND "${AROMA_FG_SPIRV_VAL}" --target-env vulkan1.1 "${SPIRV_FILE}"
          COMMAND ${CMAKE_COMMAND} -E touch "${SPIRV_FILE}.validated"
          DEPENDS "${SPIRV_FILE}"
          VERBATIM)
        list(APPEND AROMA_FG_SPIRV_OUTPUTS "${SPIRV_FILE}.validated")
      else()
        list(APPEND AROMA_FG_SPIRV_OUTPUTS "${SPIRV_FILE}")
      endif()
    endforeach()
    add_custom_target(aroma_fg_shaders ALL DEPENDS ${AROMA_FG_SPIRV_OUTPUTS})
  else()
    message(STATUS "glslangValidator not found; shader compilation target is disabled")
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS aroma_fg EXPORT AromaFGTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
