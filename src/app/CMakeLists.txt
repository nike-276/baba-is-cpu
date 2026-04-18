add_executable(babaiwt
    main.cpp
    gui_loop.cpp
    gui_loop.hpp
    input_handler.hpp
)
target_include_directories(babaiwt PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(babaiwt PRIVATE cli editor render)

# macOS: raylib requires system frameworks.
if(APPLE)
    target_link_libraries(babaiwt PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework AppKit"
        "-framework Metal"
        "-framework OpenGL"
    )
endif()
