
cmake_minimum_required(VERSION 3.20)

project(
    XenonCompiler
    VERSION 0.1.1
    LANGUAGES CXX
)

# ==================================================
# Options
# ==================================================

option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(XENON_BUILD_TOOLS "Build Xenon development tools and smoke tests" OFF)

set(DEFAULT_TARGET "unknown" CACHE STRING "Default Xenon target")
set(XEC_VERSION "${PROJECT_VERSION}" CACHE STRING "Xenon compiler version")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ==================================================
# Dependencies
# ==================================================

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(tomlplusplus)

# ==================================================
# Build configuration
# ==================================================

function(xenon_configure_target target)

    target_compile_features(${target} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
        )

        if(WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wnon-virtual-dtor
            -Wold-style-cast
        )

        if(WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    # Sanitizers
    if(ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )

        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()

endfunction()

# ==================================================
# Shared sources
# ==================================================

add_library(xenon_common STATIC
    xec/src/common/source/source_file.cpp
    xec/src/common/source/source_file_manager.cpp
    xec/src/common/diagnostics/diagnostic_collector.cpp
)

target_include_directories(xenon_common PUBLIC
    "${PROJECT_SOURCE_DIR}/xec/src"
)

target_link_libraries(xenon_common PUBLIC
    tomlplusplus::tomlplusplus
)

xenon_configure_target(xenon_common)

# ==================================================
# Main compiler
# ==================================================

file(GLOB_RECURSE XEC_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/xec/src/*.cpp"
)

list(REMOVE_ITEM XEC_SOURCES
    "${PROJECT_SOURCE_DIR}/xec/src/main.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/common/source/source_file.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/common/source/source_file_manager.cpp"
    "${PROJECT_SOURCE_DIR}/xec/src/common/diagnostics/diagnostic_collector.cpp"
)

add_executable(xec
    xec/src/main.cpp
    ${XEC_SOURCES}
)

install(
    TARGETS xec
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

target_include_directories(xec PRIVATE
    "${PROJECT_SOURCE_DIR}/xec/src"
)

target_compile_definitions(xec PRIVATE
    XEC_VERSION="${XEC_VERSION}"
    DEFAULT_TARGET="${DEFAULT_TARGET}"
    DEFAULT_HOME="${CMAKE_INSTALL_PREFIX}/share/xenon"
)

target_link_libraries(xec PRIVATE
    xenon_common
    tomlplusplus::tomlplusplus
)

xenon_configure_target(xec)

# ==================================================
# Development tools
# ==================================================

if(XENON_BUILD_TOOLS)

    add_executable(xenon-parse
        tools/xenon-parse.cpp
        xec/src/lexer/lexer.cpp
        xec/src/lexer/token.cpp
        xec/src/parser/parser.cpp
    )

    target_include_directories(xenon-parse PRIVATE
        "${PROJECT_SOURCE_DIR}/xec/src"
    )
    target_link_libraries(xenon-parse PRIVATE xenon_common)
    xenon_configure_target(xenon-parse)

    # Lexer CLI
    add_executable(xenon-lex
        tools/xenon-lex.cpp
        xec/src/lexer/lexer.cpp
        xec/src/lexer/token.cpp
    )

    target_include_directories(xenon-lex PRIVATE
        "${PROJECT_SOURCE_DIR}/xec/src"
    )

    target_link_libraries(xenon-lex PRIVATE
        xenon_common
    )

    xenon_configure_target(xenon-lex)


    # AST smoke test
    add_executable(xenon-ast
        tools/xenon_ast.cpp
    )

    target_link_libraries(xenon-ast PRIVATE
        xenon_common
    )

    xenon_configure_target(xenon-ast)

    
    # ==================================================
    # Testing
    # ==================================================
    
    enable_testing()
    
    # Add actual tests here using add_test().
    # Example:
    # add_test(NAME xenon-ast COMMAND xenon-ast)
endif()

# ==================================================
# Configuration summary
# ==================================================

message(STATUS "")
message(STATUS "========== Xenon Configuration ==========")
message(STATUS "Version:          ${XEC_VERSION}")
message(STATUS "Default target:   ${DEFAULT_TARGET}")
message(STATUS "C++ standard:     C++20")
message(STATUS "Sanitizers:       ${ENABLE_ASAN}")
message(STATUS "Warnings as errors: ${WARNINGS_AS_ERRORS}")
message(STATUS "=========================================")
message(STATUS "")
