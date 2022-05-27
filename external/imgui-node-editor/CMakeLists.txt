add_library(descript_imgui_node_editor STATIC EXCLUDE_FROM_ALL
    "crude_json.cpp"
    "crude_json.h"
    "imgui_bezier_math.h"
    "imgui_bezier_math.inl"
    "imgui_canvas.cpp"
    "imgui_canvas.h"
    "imgui_extra_math.h"
    "imgui_extra_math.inl"
    "imgui_node_editor.cpp"
    "imgui_node_editor.h"
    "imgui_node_editor_api.cpp"
    "imgui_node_editor_internal.h"
    "imgui_node_editor_internal.inl"
)
target_include_directories(descript_imgui_node_editor INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(descript_imgui_node_editor PUBLIC descript_imgui)
target_link_options(descript_imgui_node_editor PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/NATVIS:${CMAKE_CURRENT_SOURCE_DIR}/misc/imgui_node_editor.natvis>")
