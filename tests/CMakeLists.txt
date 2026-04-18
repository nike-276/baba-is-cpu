add_executable(unit_tests
    unit/check.hpp
    unit/loader_test.cpp
    unit/main.cpp
    unit/world_test.cpp
)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(unit_tests PRIVATE core)

# Scenario runner: reuse the same babaiwt binary from src/app.
# Add a CMake test that runs all .test files.
enable_testing()

file(GLOB SCENARIO_FILES "${CMAKE_SOURCE_DIR}/tests/scenarios/*.test")
foreach(scenario ${SCENARIO_FILES})
    get_filename_component(name ${scenario} NAME_WE)
    add_test(
        NAME scenario_${name}
        COMMAND babaiwt ${scenario}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endforeach()

add_test(
    NAME unit_tests
    COMMAND unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
