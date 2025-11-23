cmake_minimum_required(VERSION 3.10)

project(workloader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable warnings
add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
)

# List all sources
set(WORKLOADER_SOURCES
        workloader.c
        runners/seq_writer.c
        runners/rand_writer.c
        runners/hotspot_writer.c
        runners/append_writer.c
        runners/metadata_storm.c
        include/workloader.h
        utilites.c
)

# Build a single executable
add_executable(workloader ${WORKLOADER_SOURCES})

