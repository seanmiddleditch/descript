add_executable(descript_extra_tests)
set_target_properties(descript_extra_tests PROPERTIES CXX_STANDARD 20)
target_sources(descript_extra_tests PRIVATE
    "test_uuid.cpp"
)
target_include_directories(descript_extra_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../source")
target_link_libraries(descript_extra_tests PRIVATE descript_catch2 descript_extra)

add_test(NAME descript_extra_tests COMMAND descript_extra_tests)
