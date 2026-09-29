cmake_minimum_required(VERSION 3.20)

project(
    XenonCompiler
    VERSION 0.1.0
    LANGUAGES CXX
)

# --------------------------------------------------
# Options
# --------------------------------------------------

option(ENABLE_ASAN "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(
    DEFAULT_TRIPLE
    "unknown"
    CACHE STRING
    "Default Xenon target triple"
)

# --------------------------------------------------
# Global settings
# --------------------------------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(tomlplusplus)

# --------------------------------------------------
# Sources
# --------------------------------------------------

file(
    GLOB_RECURSE XENON_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/compiler/*.cpp"
)

add_executable(
    xec
    "${PROJECT_SOURCE_DIR}/compiler/main.cpp"
    ${XENON_SOURCES}
)

# --------------------------------------------------
# C++ standard
# --------------------------------------------------

target_compile_features(
    xec
    PRIVATE
        cxx_std_20
)

# --------------------------------------------------
# Includes
# --------------------------------------------------

target_include_directories(
    xec
    PRIVATE
        "${PROJECT_SOURCE_DIR}/compiler"
)

# --------------------------------------------------
# Compile definitions
# --------------------------------------------------

target_compile_definitions(
    xec
    PRIVATE
        XENON_DEFAULT_TRIPLE="${DEFAULT_TRIPLE}"
)

# --------------------------------------------------
# Libraries
# --------------------------------------------------

target_link_libraries(
    xec
    PRIVATE
        tomlplusplus::tomlplusplus
)

# --------------------------------------------------
# Compiler warnings
# --------------------------------------------------

if(MSVC)

    target_compile_options(
        xec
        PRIVATE
            /W4
            /permissive-
    )

    if(WARNINGS_AS_ERRORS)
        target_compile_options(
            xec
            PRIVATE
                /WX
        )
    endif()

else()

    target_compile_options(
        xec
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wnon-virtual-dtor
            -Wold-style-cast
    )

    if(WARNINGS_AS_ERRORS)
        target_compile_options(
            xec
            PRIVATE
                -Werror
        )
    endif()

endif()

# --------------------------------------------------
# Build configuration
# --------------------------------------------------

target_compile_options(
    xec
    PRIVATE
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-g>
        $<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-O2>
)

# --------------------------------------------------
# Sanitizers
# --------------------------------------------------

if(ENABLE_ASAN AND NOT MSVC)

    target_compile_options(
        xec
        PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
    )

    target_link_options(
        xec
        PRIVATE
            -fsanitize=address,undefined
    )

endif()

# --------------------------------------------------
# Configuration summary
# --------------------------------------------------

message(STATUS "Xenon compiler version: ${PROJECT_VERSION}")
message(STATUS "Default Xenon target:   ${DEFAULT_TRIPLE}")