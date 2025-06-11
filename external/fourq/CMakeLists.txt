# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

cmake_minimum_required(VERSION 3.24)

project(FourQ VERSION 3.0.0 LANGUAGES CXX C)

include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(CheckLanguage)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY OFF)

option(SECURE_COMPILE_OPTIONS "Enable Control Flow Guard and Spectre mitigations (MSVC only)" OFF)
mark_as_advanced(SECURE_COMPILE_OPTIONS)

option(USE_AVX "Use AVX extension if available" ON)
mark_as_advanced(USE_AVX)

option(USE_AVX2 "Use AVX2 extensions if available" ON)
mark_as_advanced(USE_AVX2)

option(USE_ASM "Use ASM implementation (static UNIX builds only)" ON)
mark_as_advanced(USE_ASM)

add_library(fourq STATIC)
add_library(FourQ::fourq ALIAS fourq)

# Configurations for FourQlib: system, arch, SIMD, and assembler
target_compile_options(fourq PRIVATE -DUSE_ENDO=true)

# Set system
if(MSVC)
    target_compile_options(fourq PUBLIC -D__WINDOWS__)
elseif (UNIX)
    target_compile_options(fourq PUBLIC -D__LINUX__)
endif()

# Detect architecture
include(DetectArch)
if(FOURQ_ARM64)
    # _ARM64_ needs to be set if the ARM64 optimizations are used
    # (in UNIX) or the generic implementation is used (Windows)
    target_compile_options(fourq PUBLIC -D_ARM64_)
    set(_ARM64_ ON)
endif()
if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86)
    target_compile_options(fourq PUBLIC -D_X86_)
    set(_X86_ ON)
endif()

if(FOURQ_AMD64)
    target_compile_options(fourq PUBLIC -D_AMD64_)
    set(_AMD64_ ON)
    message(STATUS "FourQ optimization: arch=AMD64")
elseif(FOURQ_ARM64 AND UNIX)
    message(STATUS "FourQ optimization: arch=ARM64")
else()
    target_compile_options(fourq PUBLIC -D_GENERIC_)
    set(_GENERIC_ ON)
    message(STATUS "FourQ optimization: arch=GENERIC")
endif()

# Detect AVX instructions
if(FOURQ_AMD64 AND (USE_AVX OR USE_AVX2))
    include(FindAVX)
    check_for_avx(fourq)
    if(HAVE_AVX2_EXTENSIONS AND USE_AVX2)
        target_compile_options(fourq PUBLIC -D_AVX2_)
        set(_AVX2_ ON)
        set(USE_AVX OFF CACHE BOOL "Use AVX extension if available" FORCE)
        message(STATUS "FourQ optimization: simd=AVX2")
    elseif(HAVE_AVX_EXTENSIONS AND USE_AVX)
        target_compile_options(fourq PUBLIC -D_AVX_)
        set(_AVX_ ON)
        set(USE_AVX2 OFF CACHE BOOL "Use AVX2 extensions if available" FORCE)
        message(STATUS "FourQ optimization: simd=AVX")
    else()
        set(USE_AVX OFF CACHE BOOL "Use AVX extensions if available" FORCE)
        set(USE_AVX2 OFF CACHE BOOL "Use AVX2 extensions if available" FORCE)
        message(STATUS "FourQ optimization: simd=OFF")
    endif()
else()
    set(USE_AVX OFF CACHE BOOL "Use AVX extensions if available" FORCE)
    set(USE_AVX2 OFF CACHE BOOL "Use AVX2 extensions if available" FORCE)
    message(STATUS "FourQ optimization: simd=OFF")
endif()

# Use optimized assembly on UNIX
if(USE_ASM AND UNIX AND NOT APPLE AND NOT CYGWIN AND NOT MINGW)
    check_language(ASM)
    if(CMAKE_ASM_COMPILER)
        enable_language(ASM)
        target_compile_options(fourq PUBLIC -D_ASM_)
        set(_ASM_ ON)
        message(STATUS "FourQ optimization: asm=ON")
    endif()
else()
    set(USE_ASM OFF CACHE BOOL "Use ASM implementation (static UNIX builds only)" FORCE)
    message(STATUS "FourQ optimization: asm=OFF")
endif()

add_subdirectory(fourq)
add_subdirectory(random)
add_subdirectory(sha512)

if(MSVC)
    set(__WINDOWS__ ON)
endif()
if(UNIX)
    set(__LINUX__ ON)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(_MSC_VER ON)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(__GNUC__ ON)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(__clang__ ON)
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/fourq/FourQ_define.h.in ${CMAKE_CURRENT_LIST_DIR}/fourq/FourQ_define.h)

