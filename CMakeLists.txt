cmake_minimum_required(VERSION 3.20)
project(baba-is-true CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the warning flags used in the Phase 1 Makefile.
add_compile_options(
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
    -Werror=return-type
)

# ── raylib via FetchContent (no system install required) ──────────────────
include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ── Modules (layer order: core → sim → render/editor/cli → app) ──────────
add_subdirectory(src/core)
add_subdirectory(src/sim)
add_subdirectory(src/render)
add_subdirectory(src/editor)
add_subdirectory(src/cli)
add_subdirectory(src/app)
add_subdirectory(tests)

# ── Architectural layer-enforcement smoke test ────────────────────────────
# CI: `grep -r '#include.*raylib' src/core/ src/sim/ && exit 1 || exit 0`
# (checked in CI scripts, not enforced here at configure time)
