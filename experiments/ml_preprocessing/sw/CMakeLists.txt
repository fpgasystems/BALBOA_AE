# CMake configuration
cmake_minimum_required(VERSION 3.5)
project(ml_preprocessing_rdma)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Enable GPU support in the Coyote SW library (sets compiler to hipcc, finds ROCm)
set(EN_GPU "1" CACHE STRING "AMD GPU enabled" FORCE)

add_subdirectory(../../../Coyote/sw ${CMAKE_BINARY_DIR}/coyote)

# Add source files
set(INSTANCE "client" CACHE STRING "RDMA software build target: client or server")
if(INSTANCE STREQUAL "server")
    set(TARGET_DIR "${CMAKE_SOURCE_DIR}/src/server")
    message("*** ML Preprocessing RDMA: Server [Software] ***")
    include_directories("${CMAKE_SOURCE_DIR}/src/include")
endif()
if(INSTANCE STREQUAL "client")
    set(TARGET_DIR "${CMAKE_SOURCE_DIR}/src/client")
    message("*** ML Preprocessing RDMA: Client [Software] ***")
    include_directories("${CMAKE_SOURCE_DIR}/src/include")
endif()

# Create build targets and link against required libraries
set(EXEC test)
add_executable(${EXEC} ${TARGET_DIR}/main.cpp)
target_link_libraries(${EXEC} PUBLIC Coyote)

find_package(Boost REQUIRED COMPONENTS program_options)
target_link_libraries(${EXEC} PUBLIC Boost::program_options)

set(AMD_GPU "gfx90a" CACHE STRING "Target GPU architecture (e.g. gfx90a for MI210)")
