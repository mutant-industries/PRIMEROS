# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2018-2019 Mutant Industries ltd.
cmake_minimum_required(VERSION 3.2)

project(PrimerOS VERSION 0.1 LANGUAGES C)

add_library(PrimerOS
        src/kernel.c
        src/collection/list.c
        src/process.c
        src/resource.c
        src/sync/semaphore.c)

target_include_directories(PrimerOS
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
        PRIVATE src)

export(TARGETS PrimerOS FILE PrimerOS.cmake)
