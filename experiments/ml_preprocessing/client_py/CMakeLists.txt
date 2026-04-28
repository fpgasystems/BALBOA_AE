# CMake configuration
cmake_minimum_required(VERSION 3.5)
project(rdma_module LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Enable GPU support in the Coyote SW library (sets compiler to hipcc, finds ROCm)
set(EN_GPU "1" CACHE STRING "AMD GPU enabled" FORCE)

add_subdirectory(../../../Coyote/sw ${CMAKE_BINARY_DIR}/coyote)

# Find pybind11 via its CMake config (installed via: pip install pybind11)
execute_process(
    COMMAND python3 -m pybind11 --cmakedir
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_PREFIX_PATH ${PYBIND11_CMAKE_DIR} ${CMAKE_PREFIX_PATH})
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 REQUIRED)

include_directories("${CMAKE_SOURCE_DIR}/include")

pybind11_add_module(rdma_module MODULE src/rdma_module.cpp)
target_link_libraries(rdma_module PUBLIC Coyote)
target_compile_definitions(rdma_module PUBLIC -D__HIP_PLATFORM_AMD__)

set(AMD_GPU "gfx90a" CACHE STRING "Target GPU architecture (e.g. gfx90a for MI210)")
