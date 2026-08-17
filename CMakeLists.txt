# @file CMakeLists.txt
# @author Jaroslav Hucel (xhucel00@vutbr.cz)
# @brief Main CMake file for compiling vkgen
# @date Created: 30. 07. 2025
# @date Modified: 01. 05. 2026
#
# @copyright Copyright (c) 2025 -> Public Domain, for more information see LICENSE

cmake_minimum_required(VERSION 3.21) # PROJECT_IS_TOP_LEVEL

set(APP_NAME vkgen)
set(CMAKE_CXX_STANDARD 23)

project(${APP_NAME})

# Default to Debug build if no profile is specified (only when standalone)
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build (Debug, RelWithDebugInfo, Release)" FORCE)
endif()

set(APP_SOURCES
   src/arena.cpp
   src/config.cpp
   src/xml/xml.cpp
   src/xml/lexer.cpp
   src/xml/parser.cpp
   src/generator/generator.cpp
   src/main.cpp
   )

set(APP_INCLUDES
   )

if(PROJECT_IS_TOP_LEVEL)
    option(SANITIZE "Enable Address and Undefined Behavior Sanitizers" OFF)
endif()

# target
add_executable(${APP_NAME} ${APP_SOURCES} ${APP_INCLUDES})

# Expose the vkg_generate() helper to this build and to any parent
# project that pulled us in via add_subdirectory().
include(${CMAKE_CURRENT_LIST_DIR}/cmake/vkgGenerate.cmake)

if(MSVC)
    # MSVC (Windows) specific base flags
    target_compile_options(${APP_NAME} PRIVATE /W4 /Zc:preprocessor)

    # MSVC Profile handling via Generator Expressions
    target_compile_options(${APP_NAME} PRIVATE
        $<$<CONFIG:Debug>:/Zi /Od>
        $<$<CONFIG:RelWithDebugInfo>:/Zi /O2>
        $<$<CONFIG:Release>:/O2>
    )
else()
    # Always applied to GCC/Clang
    target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)

    if (SANITIZE)
        # SANITIZE overrides standard profiles to force Debug + Sanitizer flags
        target_compile_options(${APP_NAME} PRIVATE
            -O0 -g -fdiagnostics-color=always -pedantic
            -fsanitize=address -fsanitize=leak -fsanitize=undefined
        )
        target_link_options(${APP_NAME} PRIVATE
            -fsanitize=address -fsanitize=leak -fsanitize=undefined
            -static-libasan -static-libubsan
        )
        message(STATUS "Build Profile: SANITIZE (Forced Debug + Sanitizers)")
    else()
        # Map requested profiles using Generator Expressions
        target_compile_options(${APP_NAME} PRIVATE
            $<$<CONFIG:Debug>:-O0 -g -fdiagnostics-color=always -pedantic>
            $<$<CONFIG:RelWithDebugInfo>:-O2 -g -fdiagnostics-color=always -pedantic>
            $<$<CONFIG:Release>:-O3>
        )
        message(STATUS "Build Profile: ${CMAKE_BUILD_TYPE}")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(${APP_NAME} PRIVATE stdc++exp) # <stacktrace>
    endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
# Debug target to check if vkg.hpp is compilable
if (MSVC)
    add_custom_target(vkg_comp COMMAND echo "Compiling vkg.hpp manually with MSVC is not supported")
else()
    # This works for both GCC (GNU) and Clang
    add_custom_target(vkg_comp
        COMMAND ${CMAKE_CXX_COMPILER} -c ${CMAKE_BINARY_DIR}/vkg.cpp -o ${CMAKE_BINARY_DIR}/vk.o -std=c++20 -Wall -Wextra -g -pedantic -fdiagnostics-color=always
        COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_BINARY_DIR}/vkg_comp.cpp -g ${CMAKE_BINARY_DIR}/vk.o -o ${CMAKE_BINARY_DIR}/vkg_comp -std=c++20 -fdiagnostics-color=always
        COMMAND echo "" && echo "Successfully compiled test file 'vkg_comp.cpp' with ${CMAKE_CXX_COMPILER_ID}!"
        COMMENT "Compiling vkg.hpp manually with ${CMAKE_CXX_COMPILER_ID} to check if it compiles"
        )
        endif()

        # Debug target to check if C++20 modules are compilable
        if (MSVC)
        add_custom_target(modules COMMAND echo "Compiling modules manually with MSVC is not supported")
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_custom_target(modules
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fmodules-ts -c ${CMAKE_BINARY_DIR}/vkg.cppm -o ${CMAKE_BINARY_DIR}/vkg_module.o -Wall -Wextra -g -pedantic -fdiagnostics-color=always
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -c ${CMAKE_BINARY_DIR}/vkg.cpp -o ${CMAKE_BINARY_DIR}/vkg.o -Wall -Wextra -g -pedantic -fdiagnostics-color=always
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fmodules-ts ${CMAKE_BINARY_DIR}/modules.cpp -g ${CMAKE_BINARY_DIR}/vkg_module.o ${CMAKE_BINARY_DIR}/vkg.o -o ${CMAKE_BINARY_DIR}/modules_test
        COMMAND echo "" && echo "Successfully compiled modules test 'modules.cpp' with GCC!"
        COMMENT "Compiling vkg.cppm and modules.cpp with GCC to test C++20 modules"
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_custom_target(modules
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 --precompile ${CMAKE_BINARY_DIR}/vkg.cppm -o ${CMAKE_BINARY_DIR}/vkg.pcm -Wall -Wextra -g -pedantic -fdiagnostics-color=always
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -c ${CMAKE_BINARY_DIR}/vkg.pcm -o ${CMAKE_BINARY_DIR}/vkg_module.o -Wall -Wextra -g -pedantic -fdiagnostics-color=always
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fprebuilt-module-path=${CMAKE_BINARY_DIR} -c ${CMAKE_BINARY_DIR}/vkg.cpp -o ${CMAKE_BINARY_DIR}/vkg.o -Wall -Wextra -g -pedantic -fdiagnostics-color=always
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fprebuilt-module-path=${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/modules.cpp -g ${CMAKE_BINARY_DIR}/vkg_module.o ${CMAKE_BINARY_DIR}/vkg.o -o ${CMAKE_BINARY_DIR}/modules_test
        COMMAND echo "" && echo "Successfully compiled modules test 'modules.cpp' with Clang!"
        COMMENT "Compiling vkg.cppm and modules.cpp with Clang to test C++20 modules"
    )
endif()
endif() # PROJECT_IS_TOP_LEVEL
