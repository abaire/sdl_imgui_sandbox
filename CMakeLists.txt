cmake_minimum_required(VERSION 3.30)
project(sandbox C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(ExternalProject)

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.4
)
set(SDL_SHARED OFF CACHE BOOL "Build SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
FetchContent_MakeAvailable(SDL3)

# ---------------------------------------------------------------------------
# ImGui
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7
)
FetchContent_MakeAvailable(imgui)

# ---------------------------------------------------------------------------
# GLM
# ---------------------------------------------------------------------------
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
)
FetchContent_MakeAvailable(glm)

# ---------------------------------------------------------------------------
# libepoxy — built via Meson under ExternalProject_Add so users need only
# have `meson` and `ninja` installed; no manual build step is required.
# ---------------------------------------------------------------------------
find_program(MESON_EXECUTABLE meson REQUIRED
    DOC "Meson build system (required to build libepoxy)")
find_program(NINJA_EXECUTABLE ninja REQUIRED
    DOC "Ninja build tool (required to build libepoxy)")

set(_EPOXY_INSTALL_DIR "${CMAKE_BINARY_DIR}/epoxy-install")

# Determine the static library path. We force --libdir=lib so the path is
# the same across distros (avoiding Debian's lib/x86_64-linux-gnu layout).
if(WIN32 OR (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    set(_EPOXY_LIB "${_EPOXY_INSTALL_DIR}/lib/libepoxy.a")
elseif(APPLE)
    set(_EPOXY_LIB "${_EPOXY_INSTALL_DIR}/lib/libepoxy.a")
else()
    set(_EPOXY_LIB "${_EPOXY_INSTALL_DIR}/lib/libepoxy.a")
endif()

# When cross-compiling for Windows, generate a Meson cross-file at configure
# time pointing to the llvm-mingw compilers (set by the toolchain file).
set(_EPOXY_MESON_EXTRA_ARGS "")
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_MESON_CROSS_FILE "${CMAKE_BINARY_DIR}/meson-cross-windows.ini")
    file(WRITE "${_MESON_CROSS_FILE}" "\
[binaries]
c = '${CMAKE_C_COMPILER}'
cpp = '${CMAKE_CXX_COMPILER}'
ar = '${CMAKE_AR}'
strip = '${CMAKE_STRIP}'
windres = '${CMAKE_RC_COMPILER}'

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
sys_root = '${CMAKE_SYSROOT}'
")
    set(_EPOXY_MESON_EXTRA_ARGS --cross-file "${_MESON_CROSS_FILE}")
endif()

ExternalProject_Add(libepoxy_ext
    GIT_REPOSITORY https://github.com/anholt/libepoxy.git
    GIT_TAG        1.5.10
    GIT_SHALLOW    TRUE

    PREFIX         "${CMAKE_BINARY_DIR}/epoxy"
    INSTALL_DIR    "${_EPOXY_INSTALL_DIR}"

    CONFIGURE_COMMAND
        "${MESON_EXECUTABLE}" setup
            <BINARY_DIR>
            <SOURCE_DIR>
            "--prefix=<INSTALL_DIR>"
            --libdir=lib
            --buildtype=release
            -Ddefault_library=static
            -Dtests=false
            ${_EPOXY_MESON_EXTRA_ARGS}

    BUILD_COMMAND   "${NINJA_EXECUTABLE}" -C <BINARY_DIR>
    INSTALL_COMMAND "${NINJA_EXECUTABLE}" -C <BINARY_DIR> install

    # Declare the output so Ninja knows this target produces this file.
    BUILD_BYPRODUCTS "${_EPOXY_LIB}"
)

# Pre-create the include directory so CMake's INTERFACE_INCLUDE_DIRECTORIES
# existence check passes at configure time (the directory is populated later
# when ExternalProject_Add actually builds and installs libepoxy).
file(MAKE_DIRECTORY "${_EPOXY_INSTALL_DIR}/include")

add_library(epoxy::epoxy STATIC IMPORTED GLOBAL)
add_dependencies(epoxy::epoxy libepoxy_ext)
set_target_properties(epoxy::epoxy PROPERTIES
    IMPORTED_LOCATION             "${_EPOXY_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_EPOXY_INSTALL_DIR}/include"
)

# ---------------------------------------------------------------------------
# ImGui library target
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static epoxy::epoxy)

# ---------------------------------------------------------------------------
# gloffscreen (thin SDL3-based offscreen GL helper)
# ---------------------------------------------------------------------------
add_subdirectory(third_party/gloffscreen)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(
        sandbox
        src/debug_settings.cpp
        src/gui.cpp
        src/main.cpp
        src/renderer.cpp
)

if(APPLE)
    set_target_properties(sandbox PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Sandbox"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.sdl-imgui-sandbox"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    )
endif()

target_link_libraries(sandbox PRIVATE imgui SDL3::SDL3-static glm::glm gloffscreen epoxy::epoxy)
