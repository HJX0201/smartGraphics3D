cmake_minimum_required(VERSION 3.20)

project(smartGraphics3D VERSION 0.1.0 LANGUAGES CXX)

set(SMARTGRAPHICS3D_VERSION_SUFFIX "-beta.1")
set(SMARTGRAPHICS3D_VERSION "${PROJECT_VERSION}${SMARTGRAPHICS3D_VERSION_SUFFIX}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SMARTGRAPHICS3D_BUILD_TESTS "Build smartGraphics3D tests" ON)
option(SMARTGRAPHICS3D_BUILD_BENCHMARKS "Build performance benchmarks" ON)
set(SMARTGRAPHICS3D_OCCT_ROOT "" CACHE PATH "OpenCascade 7.7.0 install root")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "smartGraphics3D supports Windows x64 only")
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_SOURCE_DIR}/sgraphCore/s_version.h.in"
    "${CMAKE_BINARY_DIR}/generated/s_version.h"
    @ONLY
)

find_package(Qt5 5.12.10 EXACT REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    Svg
)

if(SMARTGRAPHICS3D_BUILD_TESTS)
    find_package(Qt5 5.12.10 EXACT REQUIRED COMPONENTS Test)
endif()

if(SMARTGRAPHICS3D_OCCT_ROOT)
    find_package(OpenCASCADE 7.7.0 EXACT CONFIG REQUIRED
        PATHS "${SMARTGRAPHICS3D_OCCT_ROOT}/cmake"
        NO_DEFAULT_PATH
    )
else()
    find_package(OpenCASCADE 7.7.0 EXACT CONFIG REQUIRED)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(sgraphCore)
add_subdirectory(sgraphKernel)
add_subdirectory(sgraphDocument)
add_subdirectory(sgraphCommands)
add_subdirectory(sgraphIo)
add_subdirectory(sgraphRender)
add_subdirectory(sgraphGui)
add_subdirectory(sgraphApp)

if(SMARTGRAPHICS3D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(sgraphTests)
endif()

if(SMARTGRAPHICS3D_BUILD_BENCHMARKS)
    add_subdirectory(sgraphBenchmarks)
endif()
