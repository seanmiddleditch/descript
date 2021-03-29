add_executable(descript_tests)
set_target_properties(descript_tests PROPERTIES CXX_STANDARD 20)
target_sources(descript_tests PRIVATE
    "leak_alloc.hh"    
    "test_compiler.cpp"
    "test_expression.cpp"
    "test_runtime.cpp"
    "test_uuid.cpp"
)
target_include_directories(descript_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../source")
target_link_libraries(descript_tests PRIVATE descript_catch2 descript)

add_test(NAME descript_tests COMMAND descript_tests)
