# CMake configuration
cmake_minimum_required(VERSION 3.5)
project(example_09_perf_rdma)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(../../../Coyote/sw ${CMAKE_BINARY_DIR}/coyote)

# Add source files
set(INSTANCE "server_scaling" CACHE STRING "RDMA software build target: server_scaling or client_scaling")
if(INSTANCE STREQUAL "server_scaling")
    set(TARGET_DIR "${CMAKE_SOURCE_DIR}/src/server_scaling")
    message("*** Coyote Example 9: RDMA Server [Software Build] ***")
    include_directories("${CMAKE_SOURCE_DIR}/src/include")
endif()
if(INSTANCE STREQUAL "client_scaling")
    set(TARGET_DIR "${CMAKE_SOURCE_DIR}/src/client_scaling")
    message("*** Coyote Example 9: RDMA Client [Software Build] ***")
    include_directories("${CMAKE_SOURCE_DIR}/src/include")
endif()

# Expose Coyote headers directly (they live under include/coyote/)
include_directories("${CMAKE_SOURCE_DIR}/../../../Coyote/sw/include/coyote")

# Create build targets and link against required libraries
set(EXEC test)
add_executable(${EXEC} ${TARGET_DIR}/main.cpp)
target_link_libraries(${EXEC} PUBLIC Coyote)
find_package(Boost REQUIRED COMPONENTS program_options)
target_link_libraries(${EXEC} PUBLIC Boost::program_options)
