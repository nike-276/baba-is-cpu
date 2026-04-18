add_library(editor STATIC
    editor.cpp
    editor.hpp
    palette.hpp
)
target_include_directories(editor PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(editor PUBLIC sim)
