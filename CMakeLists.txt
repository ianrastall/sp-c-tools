cmake_minimum_required(VERSION 3.15)
project(spct C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Vendored, in-process pgn-extract (GPLv3; see vendor/pgn-extract/PATCHES.md),
# plus the shared SPCT core (driver + classifiers).
file(GLOB PGNX_SRC "${CMAKE_SOURCE_DIR}/vendor/pgn-extract/*.c")

add_library(spctcore STATIC
    ${PGNX_SRC}
    ${CMAKE_SOURCE_DIR}/core/pgnx.c
    ${CMAKE_SOURCE_DIR}/core/pgnu.c
    ${CMAKE_SOURCE_DIR}/core/corpus.c)
target_include_directories(spctcore PUBLIC
    ${CMAKE_SOURCE_DIR}/vendor/pgn-extract
    ${CMAKE_SOURCE_DIR}/core)
# pgn-extract uses POSIX regex; mingw needs -lregex, plus -lm.
target_link_libraries(spctcore PUBLIC regex m)
# Vendored source is warning-noisy upstream code; silence it.
target_compile_options(spctcore PRIVATE -w)

# The unified tool: one binary, a subcommand per report.
add_executable(spct
    ${CMAKE_SOURCE_DIR}/tools/spct/spct.c
    ${CMAKE_SOURCE_DIR}/tools/eas/eas.c
    ${CMAKE_SOURCE_DIR}/tools/iws/iws.c)
target_link_libraries(spct PRIVATE spctcore)

add_executable(smoke ${CMAKE_SOURCE_DIR}/tools/smoke/smoke.c)
target_link_libraries(smoke PRIVATE spctcore)
