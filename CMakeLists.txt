# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2018-2019 Mutant Industries ltd.
cmake_minimum_required(VERSION 3.2)

project(PrimerOS VERSION 0.1 LANGUAGES C)

add_library(PrimerOS
        src/kernel.c
        src/resource.c
        src/collection/deque.c
        src/collection/sorted/queue.c
        src/action.c
        src/action/queue.c
        src/action/schedule.c
        src/process.c
        src/scheduler.c
        src/sync/semaphore.c
        src/sync/mutex.c)

target_include_directories(PrimerOS
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
        PRIVATE src)

export(TARGETS PrimerOS FILE PrimerOS.cmake)
