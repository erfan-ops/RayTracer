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

# ============================================================
# Application sources
# ============================================================

set(APP_SOURCES
    src/main.cpp
)

# ============================================================
# Executable target
# ============================================================

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ============================================================
# Compiler-specific settings
# ============================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")

    # Clang and GCC-style options
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -ffast-math
        -mavx2
    )

    # Release configuration
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:
            -O3
            -DNDEBUG
        >
    )

    # Debug configuration
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            -O0
            -g
        >
    )

    # Link-time optimization
    # CMake's INTERPROCEDURAL_OPTIMIZATION_RELEASE property
    # enables the appropriate LTO flags for the toolchain.

elseif(MSVC)

    # MSVC-compatible Clang (clang-cl) or Microsoft compiler
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _UNICODE
        UNICODE
        NOMINMAX
    )

    target_compile_options(${PROJECT_NAME} PRIVATE
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

endif()
