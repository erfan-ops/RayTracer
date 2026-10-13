cmake_minimum_required(VERSION 3.30)

project(RayTracer LANGUAGES CXX C)

# ============================================================
# Compiler settings
# ============================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable link-time optimization in Release builds.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

option(RAYTRACER_BUILD_TESTS "Build the RayTracer test suite" ON)

# ============================================================
# FFmpeg (pre-built shared libraries shipped in include/, lib/, bin/)
# ============================================================

set(FFMPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(FFMPEG_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(FFMPEG_RUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# The import libraries and DLLs shipped with the project are built for the
# MSVC ABI (COFF import libraries, UCRT), which is what the Clang toolchain
# targets on Windows: clang++ defaults to the x86_64-pc-windows-msvc triple
# here. Clang targeting the GNU ABI (MinGW) would need the .dll.a import
# libraries instead and will not link against these.
set(FFMPEG_COMPONENTS
    avcodec
    avformat
    avutil
    swscale
)

set(FFMPEG_LIBRARIES "")
foreach(component IN LISTS FFMPEG_COMPONENTS)
    list(APPEND FFMPEG_LIBRARIES
        ${FFMPEG_LIBRARY_DIR}/${component}${CMAKE_IMPORT_LIBRARY_SUFFIX}
    )
endforeach()

# DLL file names carry the FFmpeg soname version and so cannot be derived from
# the component names. swresample is not called directly, but avcodec-63.dll
# lists it in its import table, which Windows resolves at load time; without it
# the executable fails to start with "The code specified module could not be
# found" (Win32 error 126).
set(FFMPEG_RUNTIME_DLLS
    avcodec-63.dll
    avformat-63.dll
    avutil-61.dll
    swresample-7.dll
    swscale-10.dll
)

set(FFMPEG_MISSING_DLLS "")
foreach(dll IN LISTS FFMPEG_RUNTIME_DLLS)
    if(NOT EXISTS ${FFMPEG_RUNTIME_DIR}/${dll})
        list(APPEND FFMPEG_MISSING_DLLS ${dll})
    endif()
endforeach()

if(FFMPEG_MISSING_DLLS)
    string(REPLACE ";" "\n  " FFMPEG_MISSING_DLLS_TEXT "${FFMPEG_MISSING_DLLS}")
    message(FATAL_ERROR
        "Required FFmpeg runtime DLLs are missing from ${FFMPEG_RUNTIME_DIR}:\n"
        "  ${FFMPEG_MISSING_DLLS_TEXT}\n"
        "Restore them before configuring; the build would produce an executable "
        "that cannot start."
    )
endif()

# Everything a target needs to compile and link against FFmpeg.
add_library(RayTracerFFmpeg INTERFACE)
target_include_directories(RayTracerFFmpeg INTERFACE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(RayTracerFFmpeg INTERFACE ${FFMPEG_LIBRARIES})

# Copies the FFmpeg DLLs next to a target's executable, then verifies that they
# all arrived. Only the DLLs listed above are copied, so nothing unrelated from
# bin/ is shipped, and a missing one fails the build instead of producing an
# executable that cannot start.
function(raytracer_deploy_ffmpeg_runtime target)
    set(dll_paths "")
    foreach(dll IN LISTS FFMPEG_RUNTIME_DLLS)
        list(APPEND dll_paths ${FFMPEG_RUNTIME_DIR}/${dll})
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${dll_paths}
                "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Deploying FFmpeg runtime DLLs next to $<TARGET_FILE_NAME:${target}>"
        VERBATIM
    )

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DSEARCH_DIR=$<TARGET_FILE_DIR:${target}>"
                "-DEXECUTABLE=$<TARGET_FILE_NAME:${target}>"
                "-DREQUIRED_DLLS=${FFMPEG_RUNTIME_DLLS}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRuntimeDlls.cmake"
        COMMENT "Verifying the FFmpeg runtime DLLs deployed next to $<TARGET_FILE_NAME:${target}>"
        VERBATIM
    )
endfunction()

# ============================================================
# Compiler-specific settings
# ============================================================

# clang++ with the MSVC ABI and clang-cl both report CMAKE_CXX_COMPILER_ID
# "Clang", so the driver's frontend variant, not the compiler id, decides
# which flag syntax a target needs.
function(raytracer_configure_target target)

    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")

        # clang-cl or the Microsoft compiler
        target_compile_definitions(${target} PRIVATE
            _UNICODE
            UNICODE
            NOMINMAX
        )

        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:
                /O2
                /fp:fast
                /arch:AVX2
                /DNDEBUG
            >
            $<$<CONFIG:Debug>:
                /Od
                /Zi
                /D_DEBUG
            >
        )

    else()

        # Clang and GCC-style options
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -ffast-math
            -mavx2
        )

        # Release configuration
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Release>:
                -O3
                -DNDEBUG
            >
        )

        # Debug configuration
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:
                -O0
                -g
            >
        )

        if(WIN32)
            # FFmpeg headers can pull in <windows.h>, whose min/max macros
            # would break std::numeric_limits<...>::max().
            target_compile_definitions(${target} PRIVATE NOMINMAX)
        endif()

    endif()

    # Link-time optimization
    # CMake's INTERPROCEDURAL_OPTIMIZATION_RELEASE property
    # enables the appropriate LTO flags for the toolchain.

endfunction()

# ============================================================
# Application sources
# ============================================================

set(APP_SOURCES
    src/Camera.cpp
    src/HittableList.cpp
    src/Image.cpp
    src/Interval.cpp
    src/main.cpp
    src/Material.cpp
    src/Ray.cpp
    src/Renderer.cpp
    src/Sphere.cpp
    src/Vec3.cpp
)

set(HEADERS
    headers/Camera.hpp
    headers/Common.hpp
    headers/Hittable.hpp
    headers/HittableList.hpp
    headers/Image.hpp
    headers/Interval.hpp
    headers/Material.hpp
    headers/Ray.hpp
    headers/Renderer.hpp
    headers/Sphere.hpp
    headers/Vec3.hpp
)

# ============================================================
# Executable target
# ============================================================

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/headers
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    RayTracerFFmpeg
)

raytracer_configure_target(${PROJECT_NAME})
raytracer_deploy_ffmpeg_runtime(${PROJECT_NAME})

# ============================================================
# Tests
# ============================================================

if(RAYTRACER_BUILD_TESTS)

    enable_testing()

    add_executable(RayTracerTests
        tests/ImageTests.cpp
        src/Image.cpp
        headers/Image.hpp
    )

    target_include_directories(RayTracerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/headers
    )

    target_link_libraries(RayTracerTests PRIVATE
        RayTracerFFmpeg
    )

    raytracer_configure_target(RayTracerTests)
    raytracer_deploy_ffmpeg_runtime(RayTracerTests)

    add_test(NAME RayTracer.Image COMMAND RayTracerTests)

endif()
