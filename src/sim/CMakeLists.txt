add_library(sim STATIC
    simulator.cpp
    simulator.hpp
    undo_buffer.hpp
)
target_include_directories(sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sim PUBLIC core)
