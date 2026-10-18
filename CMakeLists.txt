cmake_minimum_required(VERSION 3.21..3.31)

project(dw1 C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)

set(DW1_VERSION "us" CACHE STRING "Game release to build (only us is supported for now)")
set_property(CACHE DW1_VERSION PROPERTY STRINGS us)

# PSY-Z: PSY-Q replacement (libgs, libgpu, libgte, libcd, libsnd, libspu, ...)
# plus the SDL3 platform layer. It vendors SDL3 under external/psyz/external/SDL.
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
set(DW1_GAME_SRC)
foreach(unit ${DW1_UNITS})
    list(APPEND DW1_GAME_SRC ${DW1_${unit}_SRC})
endforeach()
# PS1 boot code (register setup, bss clear, gp): replaced by host/main.c
list(REMOVE_ITEM DW1_GAME_SRC game/src/main/_psstart.c)

string(TOUPPER ${DW1_VERSION} DW1_VERSION_UPPER)

add_library(dw1_game STATIC ${DW1_GAME_SRC})
add_dependencies(dw1_game dw1_text)
target_include_directories(dw1_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/port/include   # native overrides, searched first
    ${CMAKE_CURRENT_SOURCE_DIR}/game/include
    ${DW1_GEN_DIR})
target_compile_definitions(dw1_game PRIVATE
    LANGUAGE_C
    VERSION_${DW1_VERSION_UPPER}
    DW1_NATIVE
    main=dw1_game_main)
target_compile_options(dw1_game PRIVATE
    -fsigned-char            # MIPS char is signed, ARM's is not
    -fno-strict-aliasing
    -fwrapv
    -fcommon
    -finput-charset=UTF-8
    -fexec-charset=CP932     # the game's strings are Shift-JIS
    -include ${CMAKE_CURRENT_SOURCE_DIR}/port/include/dw1_native.h
    -w)                      # upstream code is warning-heavy; port fixes are reviewed by hand
target_link_libraries(dw1_game PUBLIC dw1_port psyz_extra psyz)

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
add_executable(dw1 host/main.c)
target_link_libraries(dw1 PRIVATE dw1_game dw1_port psyz_extra psyz)
psyz_title(dw1 "Digimon World")
