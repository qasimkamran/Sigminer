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
  test/bpftrace.cpp
)

target_include_directories(bpftrace
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(bpftrace PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(bpftrace PRIVATE sigminer)

add_executable(rich_bpftrace_mapper
  test/rich_bpftrace_mapper.cpp
)

target_include_directories(rich_bpftrace_mapper
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(rich_bpftrace_mapper PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(rich_bpftrace_mapper PRIVATE sigminer)

add_library(mock_lib SHARED
  test/mock_lib.c
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
  test/mock_live.cpp
)

target_link_libraries(mock PRIVATE mock_lib)

add_executable(mock_static_func
  test/mock_static_func.c
)

target_compile_options(mock_static_func PRIVATE
  -O0
  -g
  -fno-omit-frame-pointer
  -fno-inline
)
