cmake_minimum_required(VERSION 3.21)
# Run with cmake -P after linking, never during initial configuration.
if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "Missing runtime dependency input: ${INPUT}")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(LOCK "${OUTPUT_DIR}/.runtime-copy.lock" GUARD PROCESS TIMEOUT 120)
if(IS_MINGW)
    if(NOT EXISTS "${OBJDUMP}")
        message(FATAL_ERROR "MinGW objdump not found; check the selected toolchain")
    endif()
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${OBJDUMP}")
endif()
get_filename_component(extension "${INPUT}" EXT)
string(TOLOWER "${extension}" extension)
if(extension STREQUAL ".exe")
    set(input_kind EXECUTABLES)
else()
    set(input_kind LIBRARIES)
endif()
file(GET_RUNTIME_DEPENDENCIES
    ${input_kind} "${INPUT}"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    CONFLICTING_DEPENDENCIES_PREFIX conflicts
    DIRECTORIES "${OUTPUT_DIR}" "${TOOLCHAIN_DIR}"
    PRE_EXCLUDE_REGEXES "^api-ms-.*" "^ext-ms-.*"
    POST_EXCLUDE_REGEXES
        ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
        ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\].*")
if(conflicts_FILENAMES)
    message(FATAL_ERROR "Conflicting DLLs: ${conflicts_FILENAMES}. Use one toolchain and a clean bin.")
endif()
if(unresolved)
    message(FATAL_ERROR "Unresolved DLLs: ${unresolved}. Add their directories to PATH and rebuild.")
endif()
file(REAL_PATH "${OUTPUT_DIR}" output_real)
string(TOLOWER "${output_real}" output_lower)
foreach(dependency IN LISTS resolved)
    get_filename_component(directory "${dependency}" DIRECTORY)
    file(REAL_PATH "${directory}" directory_real)
    string(TOLOWER "${directory_real}" directory_lower)
    if(NOT directory_lower STREQUAL output_lower)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${dependency}" "${OUTPUT_DIR}"
            RESULT_VARIABLE copy_result)
        if(NOT copy_result EQUAL 0)
            message(FATAL_ERROR "Could not copy DLL: ${dependency}")
        endif()
    endif()
endforeach()

