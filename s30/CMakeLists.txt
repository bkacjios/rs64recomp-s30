# Rogue Squadron for the PocketGo S30 (ARMv7 hard-float, Alpine musl, Mali-400 GLES2, ALSA, evdev).
# RT64 needs Vulkan, which the Mali-400 lacks, so this build replaces the RT64 renderer and the SDL window.
# Run s30/host_gen.sh first (it writes the recompiled code), then:
#   cmake -S s30 -B build-s30 -G Ninja -DCMAKE_TOOLCHAIN_FILE=s30/toolchain.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-s30
cmake_minimum_required(VERSION 3.20)
project(RogueSquadronS30 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

get_filename_component(RS64_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(N64LIB "${RS64_ROOT}/../../n64" ABSOLUTE)
set(RS64_GEN "${CMAKE_CURRENT_SOURCE_DIR}/gen")

foreach(f RecompiledFuncs/funcs.h RecompiledPatches/patches.c src/game/state_table.inl s30/gen/patches_bin.c
          build/factor5_ucode/factor5_boot_recompiled.c build/factor5_ucode/musyx_audio_recompiled.c)
    if (NOT EXISTS "${RS64_ROOT}/${f}")
        message(FATAL_ERROR "${f} is missing: run s30/host_gen.sh first.")
    endif()
endforeach()

# The recompiled code type-puns RDRAM through MEM_*; strict aliasing miscompiles it.
add_compile_options(-fno-strict-aliasing)
# The device has 462 MB: commit 32 MB of rdram (game RAM plus the host tables above 8 MB).
add_compile_definitions(RECOMP_MEM_SIZE=0x2000000)

add_subdirectory(${RS64_ROOT}/lib/N64ModernRuntime ${CMAKE_BINARY_DIR}/N64ModernRuntime)

set(RS64_RECOMP_INCLUDES
    ${RS64_ROOT}/include
    ${RS64_ROOT}/lib/N64ModernRuntime/ultramodern/include
    ${RS64_ROOT}/lib/N64ModernRuntime/librecomp/include
    ${RS64_ROOT}/lib/N64ModernRuntime/N64Recomp/include
)

# ---- Recompiled game code, toml hooks and patches ----
file(GLOB RECOMP_C_SOURCES ${RS64_ROOT}/RecompiledFuncs/*.c)
file(GLOB RECOMP_CXX_SOURCES ${RS64_ROOT}/RecompiledFuncs/*.cpp)
add_library(RecompiledFuncs STATIC ${RECOMP_C_SOURCES} ${RECOMP_CXX_SOURCES} ${RS64_ROOT}/src/game/toml_hooks.c)
target_compile_definitions(RecompiledFuncs PRIVATE main=rs_main)
target_compile_options(RecompiledFuncs PRIVATE
    -Wno-unused-variable
    $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
    -include "${RS64_ROOT}/include/mem_wu.h")
target_include_directories(RecompiledFuncs PRIVATE ${RS64_RECOMP_INCLUDES})

add_library(PatchesLib OBJECT ${RS64_ROOT}/RecompiledPatches/patches.c ${RS64_GEN}/patches_bin.c)
target_compile_options(PatchesLib PRIVATE $<TARGET_PROPERTY:RecompiledFuncs,COMPILE_OPTIONS>)
target_compile_definitions(PatchesLib PRIVATE $<TARGET_PROPERTY:RecompiledFuncs,COMPILE_DEFINITIONS>)
target_include_directories(PatchesLib PRIVATE ${RS64_RECOMP_INCLUDES})

# ---- RSP ucode (MusyX synth, shared boot), fixed up by host_gen.sh ----
set(RSP_UCODE_SOURCES
    ${RS64_ROOT}/build/factor5_ucode/factor5_boot_recompiled.c
    ${RS64_ROOT}/build/factor5_ucode/musyx_audio_recompiled.c)
set_source_files_properties(${RSP_UCODE_SOURCES} PROPERTIES LANGUAGE CXX
    COMPILE_OPTIONS "-Wno-unused-variable;-Wno-missing-braces;-Wno-shift-op-parentheses;-Wno-bitwise-op-parentheses")

# ---- Host code: only what the recompiled code needs (overlays, patches, libultra shims, toml hook bodies) ----
set(RS64_HOST_SOURCES
    ${RS64_ROOT}/src/audio/audio.cpp
    ${RS64_ROOT}/src/game/upstream_compat.cpp
    ${RS64_ROOT}/src/game/hook_helpers.cpp
    ${RS64_ROOT}/src/game/game_state.cpp
    ${RS64_ROOT}/src/game/register_overlays.cpp
    ${RS64_ROOT}/src/game/register_patches.cpp
    ${RS64_ROOT}/src/game/static_standins.cpp
)

set(S30_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/s30_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/s30_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/s30_stubs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/s30_musyx.cpp
)

# The cfw's N64 platform library: display, renderer (with the Factor 5 microcode) and gamepad
# gfx*.c: the renderer and every microcode it dispatches to (gfx.c links them all)
file(GLOB N64LIB_GFX_SOURCES CONFIGURE_DEPENDS ${N64LIB}/src/gfx*.c)
add_library(n64lib STATIC
    ${N64LIB}/src/display.c
    ${N64LIB_GFX_SOURCES}
    ${N64LIB}/src/input.c
    ${N64LIB}/src/app.c)
target_compile_definitions(n64lib PRIVATE _LANGUAGE_C F3DEX_GBI_PL)
target_compile_options(n64lib PRIVATE -O2 -Wno-format)
target_include_directories(n64lib PUBLIC ${N64LIB}/include/s30 PRIVATE ${N64LIB}/include ${CMAKE_SYSROOT}/usr/include/libdrm)
target_link_libraries(n64lib PUBLIC GLESv2 EGL gbm drm m)

# mupen64plus-rsp-hle's MusyX microcode in C (GPL-2.0-or-later, s30/musyx_hle/LICENSES).
add_library(musyx_hle STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/musyx_hle/musyx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/musyx_hle/audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/musyx_hle/memory.c)
target_include_directories(musyx_hle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/musyx_hle)
target_compile_options(musyx_hle PRIVATE -O2)

add_executable(roguesquadron ${S30_SOURCES} ${RS64_HOST_SOURCES} ${RSP_UCODE_SOURCES} $<TARGET_OBJECTS:PatchesLib>)
target_compile_definitions(roguesquadron PRIVATE RS64_VERSION_STRING="s30" RS64_S30 SSE2NEON_SUPPRESS_WARNINGS)
target_compile_options(roguesquadron PRIVATE -fms-extensions -Wno-deprecated-literal-operator)
target_include_directories(roguesquadron PRIVATE
    ${RS64_ROOT}/src
    ${RS64_ROOT}/include
    ${RS64_ROOT}/RecompiledFuncs
    ${RS64_GEN}
    ${RS64_RECOMP_INCLUDES}
    ${RS64_ROOT}/lib/rt64/src
    ${RS64_ROOT}/lib/rt64/src/contrib
    ${RS64_ROOT}/lib/rt64/src/contrib/hlslpp/include
    ${RS64_ROOT}/lib/N64ModernRuntime/thirdparty/sse2neon
    ${N64LIB}/include/s30
    ${CMAKE_SYSROOT}/usr/include/SDL2
)
# The host code redefines some RecompiledFuncs/runtime symbols; the first definition wins.
target_link_options(roguesquadron PRIVATE -Wl,--allow-multiple-definition -Wl,--error-limit=0)
target_link_libraries(roguesquadron PRIVATE RecompiledFuncs librecomp ultramodern musyx_hle n64lib SDL2 pthread dl m)
