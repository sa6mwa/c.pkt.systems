if(NOT DEFINED WHISPER_SOURCE_DIR)
  message(FATAL_ERROR "WHISPER_SOURCE_DIR is required")
endif()

set(_build_info "${WHISPER_SOURCE_DIR}/cmake/build-info.cmake")
if(NOT EXISTS "${_build_info}")
  message(FATAL_ERROR "missing whisper build-info generator: ${_build_info}")
endif()

file(READ "${_build_info}" _content)
set(_old [=[# Look for git
find_package(Git)
if(NOT Git_FOUND)
    find_program(GIT_EXECUTABLE NAMES git git.exe)
    if(GIT_EXECUTABLE)
        set(Git_FOUND TRUE)
        message(STATUS "Found Git: ${GIT_EXECUTABLE}")
    else()
        message(WARNING "Git not found. Build info will not be accurate.")
    endif()
endif()

# Get the commit count and hash
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE HEAD
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE RES
    )
    if (RES EQUAL 0)
        set(BUILD_COMMIT ${HEAD})
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE RES
    )
    if (RES EQUAL 0)
        set(BUILD_NUMBER ${COUNT})
    endif()
endif()
]=])
set(_new [=[# Git metadata is intentionally disabled for reproducible vendored builds.
]=])

string(FIND "${_content}" "${_new}" _already_patched)
if(_already_patched LESS 0)
  string(FIND "${_content}" "${_old}" _patch_site)
  if(_patch_site LESS 0)
    message(FATAL_ERROR "whisper build-info generator does not match expected content")
  endif()
  string(REPLACE "${_old}" "${_new}" _content "${_content}")
  file(WRITE "${_build_info}" "${_content}")
endif()

set(_ggml_cmake "${WHISPER_SOURCE_DIR}/ggml/CMakeLists.txt")
if(NOT EXISTS "${_ggml_cmake}")
  message(FATAL_ERROR "missing ggml CMakeLists: ${_ggml_cmake}")
endif()

file(READ "${_ggml_cmake}" _ggml_content)
set(_ggml_old [=[find_program(GIT_EXE NAMES git git.exe NO_CMAKE_FIND_ROOT_PATH)
if(GIT_EXE)
    # Get current git commit hash
    execute_process(COMMAND ${GIT_EXE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GGML_BUILD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Check if the working directory is dirty (i.e., has uncommitted changes)
    execute_process(COMMAND ${GIT_EXE} diff-index --quiet HEAD -- .
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GGML_GIT_DIRTY
        ERROR_QUIET
    )
endif()
]=])
set(_ggml_new [=[set(GGML_BUILD_COMMIT "unknown")
]=])

string(FIND "${_ggml_content}" "${_ggml_old}" _ggml_patch_site)
if(NOT _ggml_patch_site LESS 0)
  string(REPLACE "${_ggml_old}" "${_ggml_new}" _ggml_content "${_ggml_content}")
endif()

set(_ggml_dirty_old [=[# Build the commit string with optional dirty flag
if(DEFINED GGML_GIT_DIRTY AND GGML_GIT_DIRTY EQUAL 1)
    set(GGML_BUILD_COMMIT "${GGML_BUILD_COMMIT}-dirty")
endif()
]=])
string(FIND "${_ggml_content}" "${_ggml_dirty_old}" _ggml_dirty_patch_site)
if(NOT _ggml_dirty_patch_site LESS 0)
  string(REPLACE "${_ggml_dirty_old}" "" _ggml_content "${_ggml_content}")
endif()

file(WRITE "${_ggml_cmake}" "${_ggml_content}")
