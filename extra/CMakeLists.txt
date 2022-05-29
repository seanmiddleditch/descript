add_library(descript_extra
    "include/descript/uuid.hh"
    "source/uuid.cpp"
)

set_target_properties(descript_extra PROPERTIES
    CXX_STANDARD 20
    LINKER_LANGUAGE CXX
    DEFINE_SYMBOL DS_EXTRA_EXPORT
    VISIBILITY_INLINES_HIDDEN 1
    CXX_VISIBILITY_PRESET hidden)
target_compile_options(descript_extra PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_include_directories(descript_extra PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)
target_link_libraries(descript_extra PUBLIC descript)

add_subdirectory(tests)
