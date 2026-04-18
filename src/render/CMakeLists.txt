add_library(render STATIC
    renderer.cpp
    renderer.hpp
    tile_colors.hpp
)
target_include_directories(render PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(render PUBLIC sim raylib)
