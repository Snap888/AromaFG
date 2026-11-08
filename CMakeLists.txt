cmake_minimum_required(VERSION 3.20)
project(AromaFG VERSION 0.1.0 LANGUAGES C CXX)

option(AROMA_FG_BUILD_TESTS "Build AromaFG portable contract tests" ON)
option(AROMA_FG_BUILD_SHARED "Build shared AromaFG library" ON)

set(AROMA_FG_LIBRARY_TYPE STATIC)
if(AROMA_FG_BUILD_SHARED)
  set(AROMA_FG_LIBRARY_TYPE SHARED)
endif()

add_library(aroma_fg ${AROMA_FG_LIBRARY_TYPE}
  src/api/aroma_fg.cpp
  src/core/engine.cpp
)
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
  add_test(NAME aroma_fg_contract_tests COMMAND aroma_fg_contract_tests)
endif()

include(GNUInstallDirs)
install(TARGETS aroma_fg EXPORT AromaFGTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
