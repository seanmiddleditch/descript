find_package(glfw3 CONFIG REQUIRED)

add_executable(descript_sample WIN32
    "graph.cpp"
    "main.cpp"
)
set_target_properties(descript_sample PROPERTIES CXX_STANDARD 20 LINKER_LANGUAGE CXX)
target_link_libraries(descript_sample PRIVATE
    descript
    descript_imgui
    descript_imgui_glfw
    descript_imgui_opengl3
    descript_imnodes
    glfw)
target_link_options(descript_sample PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/ENTRY:mainCRTStartup>")
