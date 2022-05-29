find_package(glfw3 CONFIG REQUIRED)

add_library(descript_imgui STATIC EXCLUDE_FROM_ALL
    "imconfig.h"
    "imgui.h"
    "imgui.cpp"
    "imgui_demo.cpp"
    "imgui_draw.cpp"
    "imgui_internal.h"
    "imgui_tables.cpp"
    "imgui_widgets.cpp"
    "imstb_rectpack.h"
    "imstb_textedit.h"
    "imstb_truetype.h"
    "misc/cpp/imgui_stdlib.cpp"
    "misc/cpp/imgui_stdlib.h"
)

target_include_directories(descript_imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/misc/cpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/misc/cpp>
)

add_library(descript_imgui_glfw STATIC EXCLUDE_FROM_ALL
    "backends/imgui_impl_glfw.cpp"
    "backends/imgui_impl_glfw.h"
)
target_link_libraries(descript_imgui_glfw PRIVATE descript_imgui glfw ${CMAKE_DL_LIBS})
target_include_directories(descript_imgui_glfw PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/backends>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/backends>
)

add_library(descript_imgui_opengl3 STATIC EXCLUDE_FROM_ALL
    "backends/imgui_impl_opengl3.cpp"
    "backends/imgui_impl_opengl3.h"
    "backends/imgui_impl_opengl3_loader.h"
)
target_link_libraries(descript_imgui_opengl3 PRIVATE descript_imgui)
target_include_directories(descript_imgui_opengl3 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/backends>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/backends>
)
