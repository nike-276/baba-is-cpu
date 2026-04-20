// gui_stub.cpp — headless placeholder for run_gui().
// Included in the Makefile build (no raylib). CMake links gui_loop.cpp instead.
#include "gui_loop.hpp"
#include <cstdio>

namespace baba::app {
int run_gui(std::string const& /*level_path*/) {
    std::fprintf(stderr, "babaiwt: GUI requires the CMake build with raylib.\n"
                         "  cmake -B build_cmake && cmake --build build_cmake\n"
                         "  build_cmake/babaiwt --edit\n");
    return 2;
}
}  // namespace baba::app
