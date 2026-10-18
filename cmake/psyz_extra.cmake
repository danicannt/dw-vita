# PSY-Q units that PSY-Z has decompiled (external/psyz/decomp/src) but does not
# build into libpsyz. DW1 needs them; they are added here as they show up as
# undefined symbols at link time (see doc/PORTING.md).
set(PSYZ_DECOMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/psyz/decomp/src)

set(PSYZ_EXTRA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/port/src/psyz_extra.c
)

add_library(psyz_extra STATIC ${PSYZ_EXTRA_SRC})
target_link_libraries(psyz_extra PUBLIC psyz)
target_compile_options(psyz_extra PRIVATE -w)
