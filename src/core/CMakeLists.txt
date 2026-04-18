add_library(core STATIC
    change.hpp
    coord.hpp
    direction.hpp
    kind.cpp
    kind.hpp
    loader.cpp
    loader.hpp
    object.hpp
    ruleset.cpp
    ruleset.hpp
    tick.cpp
    tick.hpp
    world.cpp
    world.hpp
)
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/src)
# core has NO external dependencies — stdlib only. This is enforced by the
# absence of any target_link_libraries call here.
