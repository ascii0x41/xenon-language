cmake_minimum_required(VERSION 3.20)

project(
    XenonCompiler
    VERSION 0.1.0
    LANGUAGES CXX
)

# --------------------------------------------------
# Global settings
# --------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------
# Options
# --------------------------------------------------

option(ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse
    GIT_TAG v3.0
)


FetchContent_MakeAvailable(tomlplusplus argparse)

# --------------------------------------------------
# Sources
# --------------------------------------------------

file(
    GLOB_RECURSE XENON_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/xenon/xec/*.cpp"
)

add_executable(
    xec
    "${PROJECT_SOURCE_DIR}/xenon/main.cpp"
    ${XENON_SOURCES}
)

# --------------------------------------------------
# Includes
# --------------------------------------------------

target_include_directories(
    xec
    PRIVATE
        "${PROJECT_SOURCE_DIR}/xenon/xec"
)

# --------------------------------------------------
# Libraries
# --------------------------------------------------

target_link_libraries(
    xec
    PRIVATE
    tomlplusplus::tomlplusplus
    argparse
)

# --------------------------------------------------
# Compiler warnings
# --------------------------------------------------

if(MSVC)

    target_compile_options(
        xec PRIVATE
        /W4
        /permissive-
    )

    if(WARNINGS_AS_ERRORS)
        target_compile_options(xec PRIVATE /WX)
    endif()

else()

    target_compile_options(
        xec PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wnon-virtual-dtor
        -Wold-style-cast
    )

    if(WARNINGS_AS_ERRORS)
        target_compile_options(xec PRIVATE -Werror)
    endif()

endif()

# --------------------------------------------------
# Build type flags
# --------------------------------------------------

target_compile_options(
    xec PRIVATE
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O2>
)

# --------------------------------------------------
# Sanitizers
# --------------------------------------------------

if(ENABLE_ASAN)

    if(NOT MSVC)

        target_compile_options(
            xec PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        target_link_options(
            xec PRIVATE
            -fsanitize=address,undefined
        )

    endif()

endif()