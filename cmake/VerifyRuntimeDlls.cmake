# Fails a build when a required runtime library did not reach the executable's
# directory, so a build cannot silently produce an executable that will not
# start on a machine without FFmpeg installed system-wide.
#
# Invoked from a POST_BUILD step with:
#   SEARCH_DIR    - directory holding the built executable
#   EXECUTABLE    - file name of the built executable
#   REQUIRED_DLLS - semicolon-separated list of required DLL file names

if(NOT DEFINED SEARCH_DIR OR NOT DEFINED EXECUTABLE OR NOT DEFINED REQUIRED_DLLS)
    message(FATAL_ERROR
        "VerifyRuntimeDlls.cmake requires SEARCH_DIR, EXECUTABLE and REQUIRED_DLLS")
endif()

# The DLLs the deployment step is responsible for, checked by name so that the
# failure message can name the one that is missing.
set(missing_dlls "")
foreach(dll IN LISTS REQUIRED_DLLS)
    if(NOT EXISTS "${SEARCH_DIR}/${dll}")
        list(APPEND missing_dlls "${dll}")
    endif()
endforeach()

if(missing_dlls)
    string(REPLACE ";" "\n  " missing_text "${missing_dlls}")
    message(FATAL_ERROR
        "Runtime libraries are missing from ${SEARCH_DIR}:\n"
        "  ${missing_text}\n"
        "The executable would fail to start. Check FFMPEG_RUNTIME_DIR in CMakeLists.txt."
    )
endif()

# Backstop: walk the executable's import graph and require everything that is
# not a Windows system library to sit next to it. This reaches dependencies of
# dependencies -- swresample, for instance, is imported by avcodec rather than
# by the executable -- and would catch a future FFmpeg build pulling in a DLL
# that the list above does not name yet.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${SEARCH_DIR}/${EXECUTABLE}"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
    DIRECTORIES "${SEARCH_DIR}"
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ww]indows.*" ".*[Ss]ystem32.*"
)

if(unresolved_dependencies)
    string(REPLACE ";" "\n  " unresolved_text "${unresolved_dependencies}")
    message(FATAL_ERROR
        "${EXECUTABLE} depends on libraries that are not next to it in ${SEARCH_DIR}:\n"
        "  ${unresolved_text}\n"
        "The executable would fail to start. Add them to FFMPEG_RUNTIME_DLLS in "
        "CMakeLists.txt."
    )
endif()

message(STATUS "Runtime libraries verified next to ${EXECUTABLE} (${REQUIRED_DLLS})")
