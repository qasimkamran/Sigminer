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
  src/sigminer.cpp
  src/signature_builder.cpp
  src/type_classifier.cpp
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
