cmake_minimum_required(VERSION 3.21)
project(MinaxCompilerAndEvaluator LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

get_filename_component(MINAX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(MINAX_BIN_DIR "${MINAX_ROOT}/bin" CACHE PATH "DLL and executable output directory")
file(MAKE_DIRECTORY "${MINAX_BIN_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MINAX_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MINAX_BIN_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${MINAX_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} "${MINAX_BIN_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/lib")
endforeach()

if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

get_filename_component(MINAX_TOOLCHAIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(MINAX_RUNTIME_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake")
function(minax_copy_runtime target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DINPUT=$<TARGET_FILE:${target}>"
                "-DOUTPUT_DIR=${MINAX_BIN_DIR}"
                "-DTOOLCHAIN_DIR=${MINAX_TOOLCHAIN_DIR}"
                "-DOBJDUMP=${CMAKE_OBJDUMP}"
                "-DIS_MINGW=${MINGW}"
                -P "${MINAX_RUNTIME_SCRIPT}"
            VERBATIM
            COMMENT "Collecting runtime DLLs for ${target}")
    endif()
endfunction()

add_library(minax_compiler SHARED
    src/Lexical.cpp src/Syntax.cpp src/Semantic.cpp)
target_include_directories(minax_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(WIN32)
    set_target_properties(minax_compiler PROPERTIES PREFIX "")
endif()
if(MINGW)
    target_link_options(minax_compiler PRIVATE "-Wl,--export-all-symbols")
endif()
minax_copy_runtime(minax_compiler)

option(MINAX_BUILD_COMPILER_MAIN "Build compiler/src/main.cpp" OFF)
if(MINAX_BUILD_COMPILER_MAIN)
    add_executable(minax_compiler_cli src/main.cpp)
    target_link_libraries(minax_compiler_cli PRIVATE minax_compiler)
    minax_copy_runtime(minax_compiler_cli)
endif()

include(CTest)
option(MINAX_BUILD_LEGACY_TESTS "Build existing compiler test executables" ON)
if(BUILD_TESTING AND MINAX_BUILD_LEGACY_TESTS)
    foreach(test_name LexicalTest SyntaxSimpleTest SyntaxComplicatedTest SemanticTest)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/${test_name}.cpp")
            add_executable(${test_name} "test/${test_name}.cpp")
            target_link_libraries(${test_name} PRIVATE minax_compiler)
            minax_copy_runtime(${test_name})
            if(test_name STREQUAL "SyntaxComplicatedTest" OR test_name STREQUAL "SemanticTest")
                add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
            endif()
        endif()
    endforeach()
endif()
add_subdirectory("${MINAX_ROOT}/evaluator" "${CMAKE_BINARY_DIR}/evaluator")

add_executable(minax_calculator "${MINAX_ROOT}/main.cpp")
target_link_libraries(minax_calculator PRIVATE minax_evaluator)
minax_copy_runtime(minax_calculator)