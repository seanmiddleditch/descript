add_library(descript
    "include/descript/alloc.hh"
    "include/descript/assembly.hh"
    "include/descript/compile_types.hh"
    "include/descript/context.hh"
    "include/descript/evaluate.hh"
    "include/descript/export.hh"
    "include/descript/expression_compiler.hh"
    "include/descript/graph_compiler.hh"
    "include/descript/hash.hh"
    "include/descript/key.hh"
    "include/descript/meta.hh"
    "include/descript/node.hh"
    "include/descript/runtime.hh"
    "include/descript/types.hh"
    "include/descript/value.hh"
    "source/alloc.cpp"
    "source/array.hh"
    "source/assembly_internal.hh"
    "source/assembly.cpp"
    "source/assert.hh"
    "source/bit.hh"
    "source/expression_compiler.cpp"
    "source/evaluate.cpp"
    "source/event.hh"
    "source/fnv.hh"
    "source/graph_compiler.cpp"
    "source/hash.cpp"
    "source/index.hh"
    "source/instance.hh"
    "source/ops.hh"
    "source/rel.hh"
    "source/runtime.cpp"
    "source/storage.hh"
    "source/string.hh"
    "source/value.cpp"
    "descript.natvis"
)

set_target_properties(descript PROPERTIES
    CXX_STANDARD 20
    LINKER_LANGUAGE CXX
    DEFINE_SYMBOL DS_EXPORT
    VISIBILITY_INLINES_HIDDEN 1
    CXX_VISIBILITY_PRESET hidden)
target_compile_options(descript PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_include_directories(descript PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)
target_link_options(descript PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/NATVIS:${CMAKE_CURRENT_SOURCE_DIR}/descript.natvis>")

add_subdirectory(tests)
