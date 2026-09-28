cmake_minimum_required(VERSION 3.15)
file(READ "${CMAKE_SOURCE_DIR}/VERSION" SPCT_VERSION)
string(STRIP "${SPCT_VERSION}" SPCT_VERSION)
project(spct VERSION ${SPCT_VERSION} LANGUAGES C)

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
# pgn-extract uses POSIX regex: mingw's libc lacks it (-lregex), glibc has it.
if(MINGW)
    target_link_libraries(spctcore PUBLIC regex)
endif()
target_link_libraries(spctcore PUBLIC m)
# Last-resort data/ location for binaries built outside the source tree.
target_compile_definitions(spctcore PRIVATE
    SPCT_DEFAULT_DATA="${CMAKE_SOURCE_DIR}/data")
# Vendored source is warning-noisy upstream code; silence it.
target_compile_options(spctcore PRIVATE -w)

# The unified tool: one binary, a subcommand per report.
add_executable(spct
    ${CMAKE_SOURCE_DIR}/tools/spct/spct.c
    ${CMAKE_SOURCE_DIR}/tools/eas/eas.c
    ${CMAKE_SOURCE_DIR}/tools/iws/iws.c
    ${CMAKE_SOURCE_DIR}/tools/sgs/sgs.c
    ${CMAKE_SOURCE_DIR}/tools/sga/sga.c
    ${CMAKE_SOURCE_DIR}/tools/gp/gp.c)
target_link_libraries(spct PRIVATE spctcore)
target_compile_definitions(spct PRIVATE SPCT_VERSION="${SPCT_VERSION}")

add_executable(smoke ${CMAKE_SOURCE_DIR}/tools/smoke/smoke.c)
target_link_libraries(smoke PRIVATE spctcore)
