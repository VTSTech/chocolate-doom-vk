set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.7.2)
project("Chocolate Doom VK" VERSION 3.0.4 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

# Autotools variables
set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})

# AC_INIT variables
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "chocolate-doom")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/VTSTech/chocolate-doom-vk/issues")

# "Chocolate" on purpose, not derived from PACKAGE_NAME: PACKAGE_NAME
# ("Chocolate Doom VK") no longer ends in "Doom", and PROGRAM_PREFIX
# below derives from this — executable / config / savegame names must
# stay chocolate-*.
set(PACKAGE_SHORTNAME "Chocolate")
set(PACKAGE_COPYRIGHT "Copyright (C) 1993-2025")
set(PACKAGE_LICENSE "GNU General Public License, version 2")

# Any settings that should apply to all targets in this directory and all
# subdirectories should go here.  Use judiciously.
if(MSVC)
    add_definitions("/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_SECURE_NO_DEPRECATE"
                    "/D_CRT_NONSTDC_NO_DEPRECATE")
else()
    add_compile_options("-Wall" "-Wdeclaration-after-statement"
                        "-Wredundant-decls")
endif()

option(CMAKE_FIND_PACKAGE_PREFER_CONFIG
       "Lookup package config files before using find modules" On)

option(ENABLE_SDL2_NET "Enable SDL2_net" On)
option(ENABLE_SDL2_MIXER "Enable SDL2_mixer" On)

if (NOT DEFINED EMSCRIPTEN)
    find_package(SDL2 2.0.14)
endif()
if(ENABLE_SDL2_MIXER)
    if (NOT DEFINED EMSCRIPTEN)
        find_package(SDL2_mixer 2.0.2)
    endif()
else()
    add_compile_definitions(DISABLE_SDL2MIXER=1)
endif()

if(ENABLE_SDL2_NET)
    if (NOT DEFINED EMSCRIPTEN)
        find_package(SDL2_net 2.0.0)
    endif()
else()
    add_compile_definitions(DISABLE_SDL2NET=1)
endif()

# Chocolate Doom VK: run libretro slang shader presets (.slangp) through
# Vulkan via VTSTech/libslang-vk (pure-C API, C++23 library behind it). The
# library is fetched at configure time and linked into the game binaries;
# the games' own sources stay C99. No Vulkan SDK needed anywhere: the
# library loads the Vulkan runtime through volk, and the presenter resolves
# its few direct entry points through SDL_Vulkan_GetVkGetInstanceProcAddr.
option(ENABLE_VK_SHADERS "libretro slang shader presets via Vulkan (VTSTech/libslang-vk)" On)
if(ENABLE_VK_SHADERS)
    if(DEFINED EMSCRIPTEN)
        message(FATAL_ERROR "ENABLE_VK_SHADERS is not supported on Emscripten")
    endif()
    add_compile_definitions(ENABLE_VK_SHADERS=1)
    # The library's targets require cxx_std_23; the games' project() only
    # enables C, so turn on CXX for the embedded library's targets.
    enable_language(CXX)
    include(FetchContent)
    set(SLANGVK_BUILD_TESTS OFF)    # the upstream suite runs in its own repo
    set(SLANGVK_BUILD_EXAMPLES OFF)
    # Shared, not static: the games link against a libslang-vk.dll that
    # lands next to each executable (see src/CMakeLists.txt) — the exes
    # stay small and the shader library can be swapped or updated
    # without relinking all four games. Set OFF to go back to the
    # single static link into each game.
    set(SLANGVK_BUILD_SHARED ON)
    # Pinned by immutable commit hash, not by a mutable release tag: the
    # exact commit the M4.07 milestone tag points at (d176d1b — SPIR-V
    # cache hooks, PERF-09 render path, SEC-07 C API validation; upstream
    # suite green). A tag can be re-pointed upstream; this hash cannot
    # drift. To bump, swap in the new full hash — always deliberate.
    FetchContent_Declare(libslang-vk
    GIT_REPOSITORY https://github.com/VTSTech/libslang-vk.git
    GIT_TAG ba2b7983be5ed295120ce67fbc5b3acb65eca8e7 # M4.08
    )
    FetchContent_MakeAvailable(libslang-vk)
    # User-facing artifact names: libslang-vk.dll on Windows (the repo's
    # name), libslang-vk.so / libslang-vk.dylib elsewhere (CMake adds
    # the "lib" prefix there). PREFIX "" is for MinGW: its toolchain
    # default would combine with the OUTPUT_NAME into a double-prefixed
    # liblibslang-vk.dll; MSVC's prefix is empty anyway, so the DLL is
    # named identically on every Windows toolchain.
    if(WIN32)
        set_target_properties(slangvk-shared PROPERTIES
            OUTPUT_NAME "libslang-vk"
            PREFIX "")
        # The upstream C API (slangvk/c.h, src/c_api.cpp) carries no
        # SLANGVK_API annotation — export.h only annotates the C++
        # headers — so on Windows the shared build would export none of
        # the C surface the games link against. A module-definition
        # file exports it explicitly at link time; keep the list in
        # sync with the pinned c.h when bumping GIT_TAG (a missing
        # name is a loud link error, not a silent miss). GCC/Clang
        # hosts export the C symbols via default visibility already.
        target_sources(slangvk-shared PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libslang-vk-c-api.def")
    else()
        # $ORIGIN keeps a relocated libslang-vk.so able to resolve
        # siblings placed next to it (macOS spells it @loader_path).
        if(APPLE)
            set(SLANGVK_RELOC_RPATH "@loader_path")
        else()
            set(SLANGVK_RELOC_RPATH "$ORIGIN")
        endif()
        set_target_properties(slangvk-shared PROPERTIES
            OUTPUT_NAME "slang-vk"
            BUILD_RPATH "${SLANGVK_RELOC_RPATH}")
    endif()
    # The library's targets require C++23 (its scope sets it for its own
    # sources); harmless here — the game code stays C99.
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()


# Check for libsamplerate.
find_package(SampleRate)
if(SampleRate_FOUND)
    set(HAVE_LIBSAMPLERATE TRUE)
endif()

# Check for libpng.
find_package(PNG)
if(PNG_FOUND)
    set(HAVE_LIBPNG TRUE)
endif()

# Check for FluidSynth.
find_package(FluidSynth 2.2.0)
if(FluidSynth_FOUND AND ENABLE_SDL2_MIXER)
    set(HAVE_FLUIDSYNTH TRUE)
endif()

find_package(m)

include(CheckSymbolExists)
include(CheckIncludeFile)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_include_file("dirent.h" HAVE_DIRENT_H)

string(CONCAT WINDOWS_RC_VERSION "${PROJECT_VERSION_MAJOR}, "
    "${PROJECT_VERSION_MINOR}, ${PROJECT_VERSION_PATCH}, 0")

# Without a hyphen. This is used for the bash-completion scripts.
string(TOLOWER "${PACKAGE_SHORTNAME}" PROGRAM_SPREFIX)

# With a hyphen, used almost everywhere else.
set(PROGRAM_PREFIX "${PROGRAM_SPREFIX}-")

configure_file(cmake/config.h.cin config.h)

configure_file(src/doom-res.rc.in src/doom-res.rc)
configure_file(src/heretic-res.rc.in src/heretic-res.rc)
configure_file(src/hexen-res.rc.in src/hexen-res.rc)
configure_file(src/setup-res.rc.in src/setup-res.rc)
configure_file(src/setup/setup-manifest.xml.in src/setup/setup-manifest.xml)
configure_file(src/strife-res.rc.in src/strife-res.rc)

foreach(SUBDIR textscreen opl pcsound src)
    add_subdirectory("${SUBDIR}")
endforeach()

# Install the .slangp demo presets shipped with libslang-vk? No — this fork
# ships none by design (see README-VK.md); presets live in the user's
# slang-shaders clone and -shader accepts any path.
