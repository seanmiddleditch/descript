add_library(descript_imnodes STATIC EXCLUDE_FROM_ALL
    "imnodes.cpp"
    "imnodes.h"
    "imnodes_internal.h"
)
target_include_directories(descript_imnodes INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(descript_imnodes PUBLIC descript_imgui)
