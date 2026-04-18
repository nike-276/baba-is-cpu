#pragma once

// Fixed pixel dimensions for the palette panel.
// These are independent of the viewport zoom (tile_px).
// Both renderer.cpp and app/input_handler.hpp use these values.
namespace baba::render {
    inline constexpr int PALETTE_PX       = 40;  // fixed tile size inside the palette
    inline constexpr int PALETTE_W        = PALETTE_PX + 8;   // total panel width
    inline constexpr int PALETTE_ENTRY_H  = PALETTE_PX + 4;   // one entry height
    inline constexpr int PALETTE_SEARCH_H = 22;                // search box height
    inline constexpr int PALETTE_HEADER_Y = 20;                // y where entries/search start
    inline constexpr int PALETTE_ENTRY_TOP = PALETTE_HEADER_Y + PALETTE_SEARCH_H + 4;
}
