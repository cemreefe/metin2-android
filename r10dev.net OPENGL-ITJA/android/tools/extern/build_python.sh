#!/bin/bash
set -e
NDK=${ANDROID_NDK:-$HOME/Android/Sdk/ndk/25.2.9519653}
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64
ABI=${ABI:-arm64-v8a}; case $ABI in arm64-v8a) TRIPLE=aarch64-linux-android;; x86_64) TRIPLE=x86_64-linux-android;; esac
export CC="$TC/bin/${TRIPLE}21-clang" CXX="$TC/bin/${TRIPLE}21-clang++" AR=$TC/bin/llvm-ar RANLIB=$TC/bin/llvm-ranlib READELF=$TC/bin/llvm-readelf
export CFLAGS="-fPIC -O2"
SRC=${EXTERN_SRC:-$HOME/extern-src}; OUT="$(cd "$(dirname "$0")/../../.." && pwd)/Extern/lib/android/$ABI"
B=$SRC/build-$ABI; mkdir -p $B; cd $B
rm -rf Python-2.7.18 && tar xf $SRC/Python-2.7.18.tgz && cd Python-2.7.18
cat > config.site <<CS
ac_cv_file__dev_ptmx=no
ac_cv_file__dev_ptc=no
ac_cv_little_endian_double=yes
ac_cv_func_gethostbyname_r=no
ac_cv_header_langinfo_h=no
CS
CONFIG_SITE=config.site ./configure --host=$TRIPLE --build=x86_64-linux-gnu --disable-ipv6 --enable-unicode=ucs4 --without-doc-strings --without-pymalloc >/tmp/pyconf.log 2>&1
cp ${EXTERN_SRC:-$HOME/extern-src}/Setup.local Modules/Setup.local
touch Include/graminit.h Python/graminit.c Include/Python-ast.h Python/Python-ast.c
make -j8 libpython2.7.a >/tmp/pymake.log 2>&1
cp libpython2.7.a "$OUT/"
cp pyconfig.h $B/pyconfig.generated.h
echo PY OK
