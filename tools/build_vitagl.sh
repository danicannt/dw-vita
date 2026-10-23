#!/bin/sh
# Build vitaGL from source with the options CTR High Octane and Lighthouse use,
# instead of the generic vdpm package:
#   NO_DEBUG=1           no GL error checking on every call
#   HAVE_SHADER_CACHE=1  compiled shaders are kept in ux0:data/shader_cache/
#                        DGWD00001, so later launches skip the compiler
# The library lands in build/vitagl/libvitaGL.a; configure the Vita build with
#   -DPSYZ_VITAGL_LIB=$PWD/build/vitagl/libvitaGL.a
# The headers stay the vdpm ones (same commit as the pinned one below).
set -e
COMMIT=${VITAGL_COMMIT:-7635e3c}  # 2026-10-16, the version tested on the Vita
SRC=${VITAGL_SRC:-build/vitagl-src}
OUT=build/vitagl
: "${VITASDK:=/usr/local/vitasdk}"
export VITASDK PATH="$VITASDK/bin:$PATH"
if [ ! -d "$SRC/.git" ]; then
    git clone https://github.com/Rinnegatamante/vitaGL.git "$SRC"
fi
git -C "$SRC" fetch -q origin || true
git -C "$SRC" checkout -q "$COMMIT"
make -C "$SRC" clean > /dev/null
make -C "$SRC" -j"$(nproc)" NO_DEBUG=1 HAVE_SHADER_CACHE=1
mkdir -p "$OUT"
cp "$SRC/libvitaGL.a" "$OUT/libvitaGL.a"
echo "built $OUT/libvitaGL.a (vitaGL $COMMIT)"
