#!/bin/bash
set -e
NDK=${ANDROID_NDK:-$HOME/Android/Sdk/ndk/25.2.9519653}
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64
API=21
ABI=${ABI:-arm64-v8a}
case $ABI in
  arm64-v8a) TRIPLE=aarch64-linux-android ;;
  armeabi-v7a) TRIPLE=armv7a-linux-androideabi ;;
  x86) TRIPLE=i686-linux-android ;;
  x86_64) TRIPLE=x86_64-linux-android ;;
esac
export CC="$TC/bin/${TRIPLE}${API}-clang" CXX="$TC/bin/${TRIPLE}${API}-clang++"
export AR=$TC/bin/llvm-ar RANLIB=$TC/bin/llvm-ranlib STRIP=$TC/bin/llvm-strip
export CFLAGS="-fPIC -O2" CXXFLAGS="-fPIC -O2"
HOSTT=$TRIPLE; [ $ABI = armeabi-v7a ] && HOSTT=arm-linux-androideabi
SRC=${EXTERN_SRC:-$HOME/extern-src}
OUT="$(cd "$(dirname "$0")/../../.." && pwd)/Extern/lib/android/$ABI"
B=$SRC/build-$ABI; mkdir -p "$B" "$OUT"
cd "$B"
# lzo
rm -rf lzo-2.10 && tar xf $SRC/lzo-2.10.tar.gz && cd lzo-2.10 && ./configure --host=$HOSTT --disable-shared --enable-static >/dev/null && make -j8 >/dev/null && cp src/.libs/liblzo2.a "$OUT/" && cd "$B" && echo "lzo OK"
# jpeg
rm -rf jpeg-9e && tar xf $SRC/jpegsrc.v9e.tar.gz && cd jpeg-9e && ./configure --host=$HOSTT --disable-shared --enable-static >/dev/null && make -j8 >/dev/null && cp .libs/libjpeg.a "$OUT/" && cd "$B" && echo "jpeg OK"
# png
rm -rf libpng-1.6.43 && tar xf $SRC/libpng-1.6.43.tar.gz && cd libpng-1.6.43 && ./configure --host=$HOSTT --disable-shared --enable-static >/dev/null && make -j8 >/dev/null && cp .libs/libpng16.a "$OUT/libpng.a" && cd "$B" && echo "png OK"
