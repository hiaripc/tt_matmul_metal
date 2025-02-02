cmake_minimum_required(VERSION 3.16)
project(metal-matmul CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_LIBCPP OFF)

if("$ENV{TT_METAL_HOME}" STREQUAL "")
    message(FATAL_ERROR "TT_METAL_HOME is not set")
endif()
if("$ENV{ARCH_NAME}" STREQUAL "")
    message(FATAL_ERROR "ARCH_NAME is not set")
endif()

set(NORMALIZED_ARCH_NAME $ENV{ARCH_NAME})
if("$ENV{ARCH_NAME}" STREQUAL "wormhole_b0")
    set(NORMALIZED_ARCH_NAME "wormhole")
endif()

if(DEFINED ENV{CMAKE_C_COMPILER} AND DEFINED ENV{CMAKE_CXX_COMPILER})
    message(STATUS "Setting C and C++ compiler from environment variables")
    set(CMAKE_C_COMPILER $ENV{CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER $ENV{CMAKE_CXX_COMPILER})
endif()

if(CMAKE_CXX_COMPILER AND CMAKE_C_COMPILER)
    message(STATUS "Using specifed C++ compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "Using specifed C compiler: ${CMAKE_C_COMPILER}")
else()
    message(STATUS "No C or C++ compiler specified, using system default compiler")
endif()

if(NOT DEFINED CPM_SOURCE_CACHE)
    message(STATUS "Setting CPM_SOURCE_CACHE to ${PROJECT_SOURCE_DIR}/.cpmcache")
    set(CPM_SOURCE_CACHE "${PROJECT_SOURCE_DIR}/.cpmcache")
else()
    message(STATUS "CPM_SOURCE_CACHE is set to: ${CPM_SOURCE_CACHE}")
endif()

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CPM)

if(CMAKE_VERSION VERSION_LESS 3.25)
    add_subdirectory(dependencies EXCLUDE_FROM_ALL)
else()
    add_subdirectory(dependencies EXCLUDE_FROM_ALL SYSTEM)
endif()

message($ENV{TT_METAL_HOME}/tt_metal/hw/inc/${NORMALIZED_ARCH_NAME})
add_executable(metal-matmul matmul_single_core.cpp)

target_include_directories(metal-matmul PRIVATE
    $ENV{TT_METAL_HOME}
    $ENV{TT_METAL_HOME}/tt_metal
    $ENV{TT_METAL_HOME}/tt_metal/third_party/umd
    $ENV{TT_METAL_HOME}/tt_metal/third_party/umd/device
    $ENV{TT_METAL_HOME}/tt_metal/third_party/umd/device/api/
    $ENV{TT_METAL_HOME}/tt_metal/third_party/taskflow/3rd-party/
    $ENV{TT_METAL_HOME}/tt_metal/third_party/tracy/public/
    $ENV{TT_METAL_HOME}/tt_metal/hw/inc/${NORMALIZED_ARCH_NAME}/
    $ENV{TT_METAL_HOME}/tt_metal/hw/inc/
    $ENV{TT_METAL_HOME}/tt_metal/third_party/umd/src/firmware/riscv/${NORMALIZED_ARCH_NAME}
    $ENV{TT_METAL_HOME}/tt_metal/hostdevcommon/api/hostdevcommon/
    $ENV{TT_METAL_HOME}/tt_metal/hostdevcommon/api/
    $ENV{TT_METAL_HOME}/build/ttnn
    $ENV{TT_METAL_HOME}/tt_metal/build

    # TTNN
    $ENV{TT_METAL_HOME}/ttnn/cpp
    $ENV{TT_METAL_HOME}/ttnn/cpp/ttnn/deprecated
    $ENV{TT_METAL_HOME}/tt_metal/third_party/magic_enum
)

##### mine ######
add_library(libttnn SHARED IMPORTED GLOBAL)
# Provide the full path to the library, so CMake knows where to find it.
set_target_properties(libttnn PROPERTIES IMPORTED_LOCATION $ENV{TT_METAL_HOME}/build/ttnn/_ttnn.so)

add_library(libttmetal SHARED IMPORTED GLOBAL)
# Provide the full path to the library, so CMake knows where to find it.
set_target_properties(libttmetal  PROPERTIES IMPORTED_LOCATION $ENV{TT_METAL_HOME}/build/tt_metal/libtt_metal.so)

#################

target_link_directories(metal-matmul PRIVATE
    $ENV{TT_METAL_HOME}/build/lib
)

target_link_libraries(metal-matmul PRIVATE
    fmt
    magic_enum
    Reflect::Reflect
    yaml-cpp
    Boost::core
    Boost::container
    libttmetal
    libttnn
    $ENV{TT_METAL_HOME}/build/lib/libdevice.so
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND USE_LIBCPP)
    target_compile_options(metal-matmul PRIVATE -stdlib=libc++)
endif()

target_compile_definitions(metal-matmul PRIVATE
    FMT_HEADER_ONLY
)

target_precompile_headers(metal-matmul PRIVATE pch.hpp)
