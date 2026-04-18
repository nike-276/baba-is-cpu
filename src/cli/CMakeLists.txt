add_library(cli STATIC
    test_runner.cpp
    test_runner.hpp
)
target_include_directories(cli PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cli PUBLIC sim)
