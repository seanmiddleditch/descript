cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(descript VERSION 0)

option(BUILD_SHARED_LIBS "Build Descript as a shared library" OFF)

option(DESCRIPT_BUILD_SAMPLES "Build Descript samples" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

enable_testing()
add_subdirectory(external)
add_subdirectory(descript)

if(DESCRIPT_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()
