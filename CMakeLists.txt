cmake_minimum_required(VERSION 3.21..3.31)

project(dw1 C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)

set(DW1_VERSION "us" CACHE STRING "Game release to build (only us is supported for now)")
set_property(CACHE DW1_VERSION PROPERTY STRINGS us)

# PSY-Z: PSY-Q replacement (libgs, libgpu, libgte, libcd, libsnd, libspu, ...)
# plus the SDL3 platform layer. It vendors SDL3 under external/psyz/external/SDL.
# 32-bit build on Debian/Ubuntu multiarch: SDL finds the i386 audio libraries
# (libpulse-dev:i386) only through the i386 pkg-config path, else it builds with
# ALSA alone, which has no PipeWire plugin on i386 and opens no device. Set here
# so automatic re-configures keep it (tools/configure_linux32.sh sets it too).
if(CMAKE_C_FLAGS MATCHES "-m32" AND EXISTS /usr/lib/i386-linux-gnu/pkgconfig)
    set(ENV{PKG_CONFIG_PATH} "/usr/lib/i386-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()
add_subdirectory(external/psyz/psyz psyz EXCLUDE_FROM_ALL)
include(cmake/psyz_extra.cmake)

include(cmake/sources_${DW1_VERSION}.cmake)

# --- text headers ------------------------------------------------------------
# The decomp keeps the game's text in text/<dir>/<name>.yaml and turns it into
# text/<dir>/<name>.h per release with its own generator.
set(DW1_DECOMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/dw_decomp)
set(DW1_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB_RECURSE DW1_TEXT_YAML CONFIGURE_DEPENDS ${DW1_DECOMP_DIR}/text/*/*.yaml)
set(DW1_TEXT_HEADERS)
foreach(yaml ${DW1_TEXT_YAML})
    file(RELATIVE_PATH rel ${DW1_DECOMP_DIR}/text ${yaml})
    string(REGEX REPLACE "\\.yaml$" ".h" rel_h ${rel})
    set(out ${DW1_GEN_DIR}/text/${rel_h})
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${Python3_EXECUTABLE} ${DW1_DECOMP_DIR}/tools/gen_text.py ${DW1_VERSION} ${yaml} ${out}
        DEPENDS ${yaml} ${DW1_DECOMP_DIR}/tools/gen_text.py
        COMMENT "Generating text/${rel_h}"
        VERBATIM)
    list(APPEND DW1_TEXT_HEADERS ${out})
endforeach()
add_custom_target(dw1_text DEPENDS ${DW1_TEXT_HEADERS})

# --- game code ---------------------------------------------------------------
# MAIN and SHOP are resident (part of SLUS_010.32); the other units are the
# *_REL.BIN overlays the PS1 loads one at a time over the same memory. Every
# unit is an object library so the linker script below can group each
# overlay's writable data and reset it when the game "loads" the overlay.
string(TOUPPER ${DW1_VERSION} DW1_VERSION_UPPER)
set(DW1_RESIDENT_UNITS MAIN SHOP)
set(DW1_OVERLAY_UNITS ${DW1_UNITS})
list(REMOVE_ITEM DW1_OVERLAY_UNITS ${DW1_RESIDENT_UNITS})
# PS1 boot code (register setup, bss clear, gp): replaced by host/main.c
list(REMOVE_ITEM DW1_MAIN_SRC game/src/main/_psstart.c)

function(dw1_game_unit target)
    add_library(${target} OBJECT ${ARGN})
    add_dependencies(${target} dw1_text)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/port/include   # native overrides, searched first
        ${CMAKE_CURRENT_SOURCE_DIR}/game/include
        ${DW1_GEN_DIR}
        $<TARGET_PROPERTY:psyz,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(${target} PRIVATE
        __psyz
        LANGUAGE_C
        VERSION_${DW1_VERSION_UPPER}
        DW1_NATIVE
        main=dw1_game_main)
    target_compile_options(${target} PRIVATE
        -fsigned-char            # MIPS char is signed, ARM's is not
        -fno-strict-aliasing
        -fwrapv
        -fcommon
        -finput-charset=UTF-8
        -fexec-charset=CP932     # the game's strings are Shift-JIS
        -include ${CMAKE_CURRENT_SOURCE_DIR}/port/include/dw1_native.h
        -w)                      # upstream code is warning-heavy; port fixes are reviewed by hand
endfunction()

# port/game: native-only code built like the game units (it uses their headers)
list(APPEND DW1_MAIN_SRC port/game/debug_console.c)
dw1_game_unit(dw1_resident ${DW1_MAIN_SRC} ${DW1_SHOP_SRC})
set(DW1_GAME_OBJECTS $<TARGET_OBJECTS:dw1_resident>)
set(DW1_OVERLAY_LD "SECTIONS\n{\n")
foreach(unit ${DW1_OVERLAY_UNITS})
    string(TOLOWER ${unit} ovl)
    dw1_game_unit(dw1ovl_${ovl} ${DW1_${unit}_SRC})
    list(APPEND DW1_GAME_OBJECTS $<TARGET_OBJECTS:dw1ovl_${ovl}>)
    string(APPEND DW1_OVERLAY_LD
        "    .dw1ovl_${ovl} : ALIGN(16) {\n"
        "        dw1_ovl_${ovl}_start = .;\n"
        "        *dw1ovl_${ovl}.dir/*(.data .data.* .bss .bss.* COMMON)\n"
        "        . = ALIGN(16);\n"
        "        dw1_ovl_${ovl}_end = .;\n"
        "    }\n")
endforeach()
string(APPEND DW1_OVERLAY_LD "}\nINSERT AFTER .data;\n")
file(WRITE ${DW1_GEN_DIR}/overlays.ld "${DW1_OVERLAY_LD}")

# --- port layer -------------------------------------------------------------
# Native pieces that replace PS1-only code: libgs (PSY-Z's is incomplete for the
# PSY-Q 4.1 API the game uses), GTE extras, stubs. Linked before psyz so these
# definitions win.
file(GLOB_RECURSE DW1_PORT_SRC CONFIGURE_DEPENDS port/src/*.c)
add_library(dw1_port STATIC ${DW1_PORT_SRC})
target_include_directories(dw1_port PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/port/include)
target_link_libraries(dw1_port PUBLIC psyz)
target_compile_options(dw1_port PRIVATE -fsigned-char -Wall -Wno-unused-function)

# --- executable --------------------------------------------------------------
add_executable(dw1 host/main.c ${DW1_GAME_OBJECTS})
target_link_libraries(dw1 PRIVATE dw1_port psyz_extra psyz)
target_link_options(dw1 PRIVATE -Wl,-T,${DW1_GEN_DIR}/overlays.ld
    -Wl,--wrap=PadRead          # port/src/input_script.c: DW1_RECORD / DW1_REPLAY
    -Wl,--wrap=GsSwapDispBuff,--wrap=VSync  # port/src/frame_dump.c: DW1_DUMP*
    -Wl,--wrap=CdRead           # port/src/read_log.c: DW1_READLOG
    # port/src/movie_stream.c and movie_extra.c: STR streaming and the MDEC
    -Wl,--wrap=CdControl,--wrap=CdRead2,--wrap=StSetRing,--wrap=StUnSetRing
    -Wl,--wrap=StSetStream,--wrap=StFreeRing,--wrap=StGetBackloc,--wrap=StGetNext
    -Wl,--wrap=DecDCTReset,--wrap=DecDCTin,--wrap=DecDCTout,--wrap=DecDCToutCallback)
set_property(TARGET dw1 APPEND PROPERTY LINK_DEPENDS ${DW1_GEN_DIR}/overlays.ld)
psyz_title(dw1 "Digimon World")

# --- tests -------------------------------------------------------------------
# Unit tests for port code that can run without the game's disc.
enable_testing()
add_executable(memcard_test tests/memcard_test.c port/src/memcard.c)
target_include_directories(memcard_test PRIVATE port/include
    $<TARGET_PROPERTY:psyz,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(memcard_test PRIVATE __psyz)
add_test(NAME memcard COMMAND memcard_test ${CMAKE_CURRENT_BINARY_DIR})

add_executable(libgs_test tests/libgs_test.c)
target_link_libraries(libgs_test PRIVATE dw1_port psyz)
target_compile_definitions(libgs_test PRIVATE __psyz)
add_test(NAME libgs COMMAND libgs_test)
set_tests_properties(libgs PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen")
