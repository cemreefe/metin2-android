# Emscripten build settings for M2_TARGET=web. Included from
# clientsource/CMakeLists.txt; see web/README.md for the full pipeline.
#
# Pthreads: the engine spawns worker threads (file loader, area loader,
# miniaudio) through the win32 _beginthreadex shim, which maps to real
# pthread_create calls — so we build with -sUSE_PTHREADS. That means the
# hosting page must be served with COOP/COEP headers (serve.py does this)
# to unlock SharedArrayBuffer.
# The codebase predates C++17 (std::unary_function in EterBase/Stl.h,
# 'register' in the Python2 headers) — keep the NDK-era language level.
# zlib/png/jpeg come from emscripten ports rather than the Extern prebuilts.
# EMULATE_FUNCTION_POINTER_CASTS: the codebase calls function pointers through
# wrong-type casts (win32-era idiom) which otherwise traps as
# "null function or function signature mismatch".
add_compile_options(-sUSE_PTHREADS=1 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1
    -sEMULATE_FUNCTION_POINTER_CASTS=1
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)

# main() runs on a pthread worker (PROXY_TO_PTHREAD): under that mode every
# libc call is proxied to the main thread anyway, so lazy files (sync XHR)
# are impossible on the main thread — shell.html prefetches all data into
# MEMFS before main() instead. Emscripten proxies GL/input to the UI thread.
add_link_options(
    -sPROXY_TO_PTHREAD=1
    -sUSE_PTHREADS=1
    -sPTHREAD_POOL_SIZE=8
    -sOFFSCREENCANVAS_SUPPORT=1
    -sOFFSCREEN_FRAMEBUFFER=1
    -sEMULATE_FUNCTION_POINTER_CASTS=1
    -sUSE_WEBGL2=1
    -sUSE_ZLIB=1
    -sUSE_LIBPNG=1
    -sUSE_LIBJPEG=1
    -sFORCE_FILESYSTEM=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=268435456
    -sEXIT_RUNTIME=1
    -sEXPORTED_RUNTIME_METHODS=HEAP8,HEAP32,HEAPU8
    -sSTACK_SIZE=4194304
    -lwebsocket.js
    --shell-file=${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html
)
