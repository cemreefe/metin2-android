#!/usr/bin/env bash
# Rebuild the wasm32 prebuilt extern libs for the Emscripten port.
#
# Produces:
#   r10dev.net OPENGL-ITJA/Extern/lib/web/libpython2.7.a   (CPython 2.7.18, ucs4, thread+signal builtin)
#   r10dev.net OPENGL-ITJA/Extern/lib/web/libcryptopp.a    (Crypto++ 8.2, vendored in Extern/include/cryptopp)
#   r10dev.net OPENGL-ITJA/Extern/lib/web/liblzo2.a        (LZO 2.10)
#   r10dev.net OPENGL-ITJA/Extern/include/Python2-web/pyconfig.h
#
# Requires: emsdk installed and activated on PATH (emcc/em++/emar/emconfigure),
#           curl, make, a C toolchain, and ~4 GB of disk.
#
#   EMSDK=/path/to/emsdk ./tools/web-deps/build.sh        # full rebuild
#
# Sources are downloaded to $EXTERN_SRC (default ~/extern-src) once, so the
# script is re-runnable. Built and verified with emcc 6.0.11.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXTERN_DIR="$REPO_ROOT/r10dev.net OPENGL-ITJA/Extern"
OUT="$EXTERN_DIR/lib/web"
PYCFG_DIR="$EXTERN_DIR/include/Python2-web"
SRC="${EXTERN_SRC:-$HOME/extern-src}"
B="$SRC/build-web-deps"
PYVER=2.7.18
LZOVER=2.10

if ! command -v emcc >/dev/null; then
    if [ -n "${EMSDK:-}" ] && [ -f "$EMSDK/emsdk_env.sh" ]; then
        # shellcheck disable=SC1091
        . "$EMSDK/emsdk_env.sh" >/dev/null
    else
        echo "emcc not on PATH; install emsdk or pass EMSDK=/path/to/emsdk" >&2
        exit 1
    fi
fi
AR="$(dirname "$(command -v emcc)")/../bin/llvm-ar"
[ -x "$AR" ] || AR=emar
NM="$(dirname "$(command -v emcc)")/../bin/llvm-nm"
[ -x "$NM" ] || NM=llvm-nm

mkdir -p "$SRC" "$B" "$OUT" "$PYCFG_DIR"
cd "$SRC"
[ -f Python-$PYVER.tgz ]      || curl -fLO "https://www.python.org/ftp/python/$PYVER/Python-$PYVER.tgz"
[ -f lzo-$LZOVER.tar.gz ]     || curl -fLO "https://www.oberhumer.com/opensource/lzo/download/lzo-$LZOVER.tar.gz"
# config.sub/config.guess new enough to know wasm32 (CPython 2.7 and LZO ship
# autotools files that reject the wasm32 host triple).
if [ ! -f config.sub ]; then
    curl -fL -o config.sub   https://raw.githubusercontent.com/gcc-mirror/gcc/master/config.sub
    curl -fL -o config.guess https://raw.githubusercontent.com/gcc-mirror/gcc/master/config.guess
fi

# ---------------------------------------------------------------------------
# Host python2.7: py2.7's configure refuses a cross build without an identical
# interpreter for PYTHON_FOR_BUILD.
# ---------------------------------------------------------------------------
if ! command -v python2.7 >/dev/null && [ ! -x "$B/host-python/Python-$PYVER/python" ]; then
    rm -rf "$B/host-python/Python-$PYVER"
    mkdir -p "$B/host-python"
    (cd "$B/host-python" && tar xf "$SRC/Python-$PYVER.tgz" && cd "Python-$PYVER" \
        && ./configure --disable-ipv6 --without-doc-strings --without-pymalloc >/dev/null \
        && make -j"$(nproc)" python >/dev/null)
fi
PYTHON_FOR_BUILD="${PYTHON_FOR_BUILD:-$(command -v python2.7 || echo "$B/host-python/Python-$PYVER/python")}"

# ---------------------------------------------------------------------------
# LZO
# ---------------------------------------------------------------------------
rm -rf "$B/lzo-$LZOVER"
(cd "$B" && tar xf "$SRC/lzo-$LZOVER.tar.gz" \
    && cp "$SRC/config.sub" "$SRC/config.guess" "lzo-$LZOVER/autoconf/" \
    && cd "lzo-$LZOVER" \
    && emconfigure ./configure --host=wasm32-unknown-emscripten \
        --disable-shared --enable-static CFLAGS="-O2 -matomics -mbulk-memory" >/dev/null \
    && emmake make -j"$(nproc)" >/dev/null \
    && cp src/.libs/liblzo2.a "$OUT/")
echo "lzo OK"

# ---------------------------------------------------------------------------
# Crypto++ (sources vendored in Extern/include/cryptopp)
# ---------------------------------------------------------------------------
CPP_B="$B/cryptopp"; rm -rf "$CPP_B"; mkdir -p "$CPP_B"
(cd "$EXTERN_DIR/include/cryptopp" && {
    EXCL="bench1 bench2 bench3 datatest dlltest fipsalgt test \
validat0 validat1 validat2 validat3 validat4 validat5 validat6 validat7 \
validat8 validat9 validat10 regtest1 regtest2 regtest3 regtest4 pch \
ppc_power7 ppc_power8 ppc_power9 ppc_simd chacha_avx donna_sse cpu"
    FILES=""
    for f in *.cpp; do
        n="${f%.cpp}"; skip=0
        for e in $EXCL; do [ "$n" = "$e" ] && skip=1; done
        [ "$skip" = 0 ] && FILES="$FILES $f"
    done
    printf '%s\n' $FILES cpu.cpp | xargs -P "$(nproc)" -I{} sh -c \
        "em++ -O2 -std=c++11 -matomics -mbulk-memory -DNDEBUG -DCRYPTOPP_DISABLE_ASM -I. -c {} -o '$CPP_B/{}.o' \
         2>>'$CPP_B/err.log' || echo FAIL {}"
})
"$AR" rcs "$OUT/libcryptopp.a" "$CPP_B"/*.o
echo "cryptopp OK"

# ---------------------------------------------------------------------------
# CPython 2.7 (the interesting one)
# ---------------------------------------------------------------------------
PY_B="$B/Python-$PYVER"; rm -rf "$PY_B"
(cd "$B" && tar xf "$SRC/Python-$PYVER.tgz")
cd "$PY_B"
cp "$SRC/config.sub" "$SRC/config.guess" .

# Cache vars for checks configure cannot run while cross compiling.
cat > config.site <<'CS'
ac_cv_file__dev_ptmx=no
ac_cv_file__dev_ptc=no
ac_cv_little_endian_double=yes
ac_cv_func_gethostbyname_r=no
ac_cv_header_langinfo_h=no
ac_cv_c_bigendian=no
ac_cv_func_dlopen=no
ac_cv_func_fork=no
ac_cv_func_forkpty=no
ac_cv_func_vfork=no
CS

# py2.7's configure only supports linux/cygwin cross hosts; wasm32-linux-gnu
# canonicalizes fine and yields MACHDEP=linux2 (sys.platform == 'linux2',
# same as the Android build). emconfigure supplies emcc/emar.
CONFIG_SITE="$PY_B/config.site" PYTHON_FOR_BUILD="$PYTHON_FOR_BUILD" \
    emconfigure ./configure \
    --host=wasm32-unknown-linux-gnu \
    --build="$(gcc -dumpmachine 2>/dev/null || echo x86_64-pc-linux-gnu)" \
    --with-threads --disable-ipv6 --enable-unicode=ucs4 \
    --without-doc-strings --without-pymalloc >/dev/null

# Builtin modules matching the shipped Android lib (config.o inittab).
# posix/errno/pwd/_sre/_codecs/_weakref/zipimport/_symtable/xxsubtype come
# from the default Modules/Setup; thread+signal from Setup.config; marshal,
# imp, gc, _ast, _warnings are fixed entries in Modules/config.c.in.
cat > Modules/Setup.local <<'EOF'
*static*
array arraymodule.c
cmath cmathmodule.c _math.c
math mathmodule.c _math.c
_struct _struct.c
time timemodule.c
operator operator.c
_weakref _weakref.c
_random _randommodule.c
_collections _collectionsmodule.c
_heapq _heapqmodule.c
itertools itertoolsmodule.c
strop stropmodule.c
_functools _functoolsmodule.c
datetime datetimemodule.c
_bisect _bisectmodule.c
unicodedata unicodedata.c
_locale _localemodule.c
fcntl fcntlmodule.c
select selectmodule.c
_socket socketmodule.c timemodule.c
_md5 md5module.c md5.c
_sha shamodule.c
_sha256 sha256module.c
_sha512 sha512module.c
binascii binascii.c
cStringIO cStringIO.c
cPickle cPickle.c
EOF

# The tarball ships generated sources; keep their timestamps so pgen is never
# needed (pgen only runs on the build host anyway).
touch Include/graminit.h Python/graminit.c Include/Python-ast.h Python/Python-ast.c

emmake make -j"$(nproc)" libpython2.7.a \
    CFLAGS="-fno-strict-aliasing -O2 -matomics -mbulk-memory -DNDEBUG -O3 -Wall -Wstrict-prototypes" \
    >/dev/null
cp libpython2.7.a "$OUT/"
cp pyconfig.h "$PYCFG_DIR/pyconfig.h"
echo "python OK"

# ---------------------------------------------------------------------------
# Acceptance test: boot the interpreter under node.
# ---------------------------------------------------------------------------
cat > "$B/embed_test.c" <<'EOF'
#include "Python.h"
int main(void) {
    Py_NoSiteFlag = 1;
    Py_Initialize();
    PyRun_SimpleString("import sys\nimport marshal\nprint 'py27-wasm-ok', 40+2\n");
    Py_Finalize();
    return 0;
}
EOF
emcc -O2 -I"$PY_B" -I"$PY_B/Include" "$B/embed_test.c" "$OUT/libpython2.7.a" \
    -o "$B/embed_test.js" -sNODERAWFS
RESULT="$(node "$B/embed_test.js" | grep 'py27-wasm-ok')"
[ "$RESULT" = "py27-wasm-ok 42" ] || { echo "embed test FAILED: $RESULT" >&2; exit 1; }
echo "embed test: $RESULT"

for sym in 'Py_Initialize:libpython2.7.a' 'AgreeE:libcryptopp.a' 'lzo1x_decompress:liblzo2.a'; do
    name="${sym%%:*}"; lib="${sym##*:}"
    # buffer nm output first: grep -q exits early and SIGPIPEs nm under pipefail
    "$NM" "$OUT/$lib" > "$B/nm.out" 2>/dev/null || true
    grep -q " T .*$name" "$B/nm.out" || { echo "missing T symbol $name in $lib" >&2; exit 1; }
done
echo "ALL OK -> $OUT"
