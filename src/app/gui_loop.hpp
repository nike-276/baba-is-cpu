#pragma once
#include <string>
namespace baba::app {
// Run the raylib GUI editor loop. Optionally loads level_path on startup.
// Returns 0 on clean exit.
int run_gui(std::string const& level_path);
}  // namespace baba::app
