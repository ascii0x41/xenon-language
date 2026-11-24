cmake_minimum_required(VERSION 3.20)

project(
    XenonCompiler
    VERSION 0.1.1
    LANGUAGES CXX
)

# --------------------------------------------------
# Options
# --------------------------------------------------

option(ENABLE_ASAN "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(
    DEFAULT_TARGET
    "unknown"
    CACHE STRING
    "Default Xenon target"
)

set(
    XEC_VERSION
    "unknown"
    CACHE STRING
    "Xenon compiler version"
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
    "${PROJECT_SOURCE_DIR}/xec/src/*.cpp"
)

add_executable(
    xec
    "${PROJECT_SOURCE_DIR}/xec/src/main.cpp"
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
        "${PROJECT_SOURCE_DIR}/xec/src"
)

# --------------------------------------------------
# Compile definitions
# --------------------------------------------------

target_compile_definitions(
    xec
    PRIVATE
        XEC_VERSION="${PROJECT_VERSION}"
        DEFAULT_TARGET="${DEFAULT_TARGET}"
        DEFAULT_HOME="${CMAKE_INSTALL_PREFIX}/share/xenon"
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

if((ENABLE_ASAN OR CMAKE_BUILD_TYPE MATCHES "Debug") AND NOT MSVC)

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
message(STATUS "Default target:   ${DEFAULT_TARGET}")

# Lexer golden-test CLI
add_executable(xenon-lex tools/xenon-lex.cpp)
target_compile_features(xenon-lex PRIVATE cxx_std_20)
target_include_directories(xenon-lex PRIVATE "${PROJECT_SOURCE_DIR}/xec/src")
target_sources(xenon-lex PRIVATE
    "${PROJECT_SOURCE_DIR}/xec/src/common/source/source_file.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/common/source/source_file_manager.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/common/diagnostics/diagnostic_collector.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/lexer/lexer.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/lexer/token.cpp")
target_link_libraries(xenon-lex PRIVATE tomlplusplus::tomlplusplus)

if(NOT MSVC)
    target_compile_options(xenon-lex PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE MATCHES "Debug" OR ENABLE_ASAN)
        target_compile_options(xenon-lex PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(xenon-lex PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# AST library smoke test
enable_testing()
add_executable(test-xenon-ast test/xenon_ast.cpp
    "${PROJECT_SOURCE_DIR}/xec/src/common/source/source_file.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/common/source/source_file_manager.cpp")
set_target_properties(test-xenon-ast PROPERTIES
    OUTPUT_NAME xenon-ast)
target_compile_features(test-xenon-ast PRIVATE cxx_std_20)
target_include_directories(test-xenon-ast PRIVATE "${PROJECT_SOURCE_DIR}/xec/src")
if(NOT MSVC)
    target_compile_options(test-xenon-ast PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE MATCHES "Debug" OR ENABLE_ASAN)
        target_compile_options(test-xenon-ast PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(test-xenon-ast PRIVATE -fsanitize=address,undefined)
    endif()
endif()
add_test(NAME xenon-ast COMMAND test-xenon-ast "${PROJECT_SOURCE_DIR}/tests/astlib_smoke.xe")