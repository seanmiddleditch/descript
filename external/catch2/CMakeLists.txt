add_library(descript_catch2 STATIC EXCLUDE_FROM_ALL
    "catch_amalgamated.hpp"
    "catch_amalgamated.cpp"
)
target_include_directories(descript_catch2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
