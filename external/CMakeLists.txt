add_subdirectory(catch2)

if(DESCRIPT_BUILD_SAMPLES)
    add_subdirectory(imgui)
    add_subdirectory(imgui-node-editor)
endif()
