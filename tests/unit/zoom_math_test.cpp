// #include "check.hpp"
// #include "app/zoom_math.hpp"  // does not exist yet — compile-RED

// #include <cmath>

// // Zoom anchor invariant: after apply_zoom_anchor, the world tile coordinate
// // under the cursor must be the same as before the zoom.
// static float world_tile(float mouse_px, float scroll, int tile_px) {
//     return mouse_px / static_cast<float>(tile_px) + scroll;
// }

// TEST(ZoomMath, single_zoom_in_preserves_anchor) {
//     float sx = 10.0f, sy = 5.0f;
//     int tile_px = 48;
//     float mx = 200.0f, my = 100.0f;

//     float before_x = world_tile(mx, sx, tile_px);
//     float before_y = world_tile(my, sy, tile_px);

//     baba::app::apply_zoom_anchor(sx, sy, tile_px, mx, my, /*delta=*/+4);

//     float after_x = world_tile(mx, sx, tile_px);
//     float after_y = world_tile(my, sy, tile_px);

//     // Tolerance: floating-point arithmetic ≪ 0.001 tiles.
//     CHECK((std::abs(after_x - before_x) < 0.001f));
//     CHECK((std::abs(after_y - before_y) < 0.001f));
// }

// TEST(ZoomMath, single_zoom_out_preserves_anchor) {
//     float sx = 3.7f, sy = 1.2f;
//     int tile_px = 64;
//     float mx = 150.0f, my = 80.0f;

//     float before_x = world_tile(mx, sx, tile_px);
//     float before_y = world_tile(my, sy, tile_px);

//     baba::app::apply_zoom_anchor(sx, sy, tile_px, mx, my, /*delta=*/-4);

//     float after_x = world_tile(mx, sx, tile_px);
//     float after_y = world_tile(my, sy, tile_px);

//     CHECK((std::abs(after_x - before_x) < 0.001f));
//     CHECK((std::abs(after_y - before_y) < 0.001f));
// }

// TEST(ZoomMath, ten_round_trip_zooms_stay_stable) {
//     // Accumulate 10 zoom-in then 10 zoom-out steps at the same cursor.
//     // Float scroll must return within 0.01 tiles of the starting scroll.
//     float sx = 7.5f, sy = 2.3f;
//     int tile_px = 48;
//     float mx = 320.0f, my = 240.0f;

//     float init_world_x = world_tile(mx, sx, tile_px);

//     for (int i = 0; i < 10; ++i)
//         baba::app::apply_zoom_anchor(sx, sy, tile_px, mx, my, +4);
//     for (int i = 0; i < 10; ++i)
//         baba::app::apply_zoom_anchor(sx, sy, tile_px, mx, my, -4);

//     float final_world_x = world_tile(mx, sx, tile_px);
//     CHECK((std::abs(final_world_x - init_world_x) < 0.01f));
// }

// TEST(ZoomMath, tile_px_clamped_at_bounds) {
//     float sx = 0.0f, sy = 0.0f;
//     int tile_px = 8;  // already at minimum
//     baba::app::apply_zoom_anchor(sx, sy, tile_px, 100.0f, 100.0f, -8);
//     CHECK_EQ(tile_px, 8);  // must not go below 8

//     tile_px = 128;  // already at maximum
//     baba::app::apply_zoom_anchor(sx, sy, tile_px, 100.0f, 100.0f, +8);
//     CHECK_EQ(tile_px, 128);  // must not exceed 128
// }
