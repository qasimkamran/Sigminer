cmake_minimum_required(VERSION 3.20)

project(Sigminer
  VERSION 0.1.0
  DESCRIPTION "Mining type info from shared object files"
  LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM CONFIG REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_library(sigminer STATIC
  src/dwarf_session.cpp
  src/sigminer.cpp
  src/sigminer_c.cpp
  src/signature_builder.cpp
  src/subprogram_finder.cpp
  src/type_classifier.cpp
  src/type_resolver.cpp
  src/printf_specifier_mapper.cpp
  src/bpftrace_mapper.cpp
  src/line_table_lookup.cpp
  src/return_site_finder.cpp
)

target_include_directories(sigminer
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(sigminer PRIVATE ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(SIGMINER_LLVM_LIBS
  Object
  DebugInfoDWARF
  Support
  MC
  MCDisassembler
  X86Desc
  X86Disassembler
  X86Info
  AArch64Desc
  AArch64Disassembler
  AArch64Info
)

target_link_libraries(sigminer PRIVATE ${SIGMINER_LLVM_LIBS})

add_executable(proto
  src/prototype.cpp
)

target_include_directories(proto
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(proto PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(proto PRIVATE sigminer)

add_executable(bpftrace
  test/integration/bpftrace_probe_runner.cpp
)

target_include_directories(bpftrace
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(bpftrace PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(bpftrace PRIVATE sigminer)

add_library(mock_lib SHARED
  test/fixture/mock_lib.c
)

set_target_properties(mock_lib PROPERTIES
  OUTPUT_NAME mock_lib
)

target_compile_options(mock_lib PRIVATE
  -O0
  -g
  -fno-omit-frame-pointer
  -fno-inline
)

add_executable(mock
  test/fixture/mock_live.cpp
)

target_link_libraries(mock PRIVATE mock_lib)

add_executable(mock_static_func
  test/fixture/mock_static_func.c
)

target_compile_options(mock_static_func PRIVATE
  -O0
  -g
  -fno-omit-frame-pointer
  -fno-inline
)

add_library(dwarf_fixture_lib SHARED
  test/fixture/dwarf_types_fixture.cpp
)

set_target_properties(dwarf_fixture_lib PROPERTIES
  OUTPUT_NAME dwarf_fixture_lib
)

target_compile_options(dwarf_fixture_lib PRIVATE
  -O0
  -g
  -fno-omit-frame-pointer
  -fno-inline
)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(sigminer_tests
        test/integration/dwarf_session_test.cpp
        test/unit/bpftrace_mapper_test.cpp
        test/unit/printf_specifier_mapper_test.cpp
        test/unit/return_site_finder_test.cpp
        test/unit/signature_builder_test.cpp
        test/unit/sigminer_c_test.cpp
        test/unit/subprogram_finder_test.cpp
        test/unit/type_classifier_test.cpp
        test/unit/type_resolver_test.cpp
    )

    target_include_directories(sigminer_tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LLVM_INCLUDE_DIRS}
    )

    target_compile_definitions(sigminer_tests PRIVATE ${LLVM_DEFINITIONS})

    target_link_libraries(sigminer_tests
        PRIVATE
        sigminer
        GTest::gtest_main
    )

    add_dependencies(sigminer_tests
        dwarf_fixture_lib
        mock_lib
    )

    include(GoogleTest)
    gtest_discover_tests(sigminer_tests)
endif()
