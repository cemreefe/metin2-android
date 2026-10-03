#!/bin/bash
set -e
NDK=${ANDROID_NDK:-$HOME/Android/Sdk/ndk/25.2.9519653}
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64
ABI=${ABI:-arm64-v8a}
case $ABI in arm64-v8a) TRIPLE=aarch64-linux-android;; x86_64) TRIPLE=x86_64-linux-android;; esac; CXX="$TC/bin/${TRIPLE}21-clang++"; CC="$TC/bin/${TRIPLE}21-clang"
SRC="$(cd "$(dirname "$0")/../../.." && pwd)/Extern/include/cryptopp"
OUT="$(cd "$(dirname "$0")/../../.." && pwd)/Extern/lib/android/$ABI"
B=${EXTERN_SRC:-$HOME/extern-src}/build-$ABI/cryptopp; rm -rf $B; mkdir -p $B
cd "$SRC"
EXCL="bench1 bench2 bench3 datatest dlltest fipsalgt test validat0 validat1 validat2 validat3 validat4 validat5 validat6 validat7 validat8 validat9 validat10 regtest1 regtest2 regtest3 regtest4 pch ppc_power7 ppc_power8 ppc_power9 ppc_simd chacha_avx donna_sse cpu"; [ $ABI = arm64-v8a ] && EXCL="$EXCL sse_simd"
FILES=""
for f in *.cpp; do n=${f%.cpp}; skip=0; for e in $EXCL; do [ "$n" = "$e" ] && skip=1; done; [ $skip = 0 ] && FILES="$FILES $f"; done
printf '%s\n' $FILES | xargs -P 16 -I{} sh -c "\"$CXX\" -fPIC -O2 -std=c++11 -DNDEBUG -DCRYPTOPP_DISABLE_ASM -I$NDK/sources/android/cpufeatures -I. -c {} -o $B/{}.o 2>>$B/err.log || echo FAIL {}"
"$CXX" -fPIC -O2 -std=c++11 -DNDEBUG -DCRYPTOPP_DISABLE_ASM -I$NDK/sources/android/cpufeatures -I. -c cpu.cpp -o $B/cpu.cpp.o 2>>$B/err.log || echo FAIL cpu.cpp
"$CC" -fPIC -O2 -c $NDK/sources/android/cpufeatures/cpu-features.c -o $B/cpu-features.o
rm -f "$OUT/libcryptopp.a"; $TC/bin/llvm-ar rcs "$OUT/libcryptopp.a" $B/*.o
echo CRYPTOPP OK; grep -c error: $B/err.log || true
