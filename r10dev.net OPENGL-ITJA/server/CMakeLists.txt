# Builds m2dev-server-src (db, game, qc) against the SQLite-backed MySQL shim
# instead of MariaDB Connector/C. Used for both host tools and Android.
cmake_minimum_required(VERSION 3.18)
project(m2-embedded-server C CXX)

set(M2_SERVER_SRC "" CACHE PATH "Patched m2dev-server-src tree")
if(NOT EXISTS "${M2_SERVER_SRC}/src/CMakeLists.txt")
	message(FATAL_ERROR "M2_SERVER_SRC must point at a (patched) m2dev-server-src checkout")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options("$<$<CXX_COMPILER_ID:GNU>:-finput-charset=UTF-8>")
add_compile_definitions(OS_LINUX NOMINMAX WIN32_LEAN_AND_MEAN)
if(ANDROID)
	add_compile_definitions(OS_ANDROID)
	include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/compat/android)
	add_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/compat/android/m2_android_compat.h)
else()
	include(CheckSymbolExists)
	check_symbol_exists(strlcpy string.h M2_HAVE_STRLCPY)
	if(NOT M2_HAVE_STRLCPY)
		add_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/compat/bsd_string.h)
	endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(GIT_DESCRIBE_VERSION "0cc595b-android")

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/sqlite-mysql/include ${CMAKE_CURRENT_SOURCE_DIR}/sqlite-mysql/sqlite3)
include_directories(${M2_SERVER_SRC}/src ${M2_SERVER_SRC}/include)

# Stands in for the vendored mariadbclient target the server links against.
add_library(mariadbclient STATIC
	sqlite-mysql/mysql_sqlite.cpp
	sqlite-mysql/sqlite3/sqlite3.c)
target_compile_definitions(mariadbclient PRIVATE
	SQLITE_THREADSAFE=1 SQLITE_DEFAULT_WAL_SYNCHRONOUS=1 SQLITE_OMIT_LOAD_EXTENSION
	SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DQS=3 _GNU_SOURCE)
if(NOT ANDROID)
	find_package(Threads REQUIRED)
	target_link_libraries(mariadbclient PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(ANDROID)
	# bionic has pthread in libc; the server CMake links "pthread" by name.
	add_library(pthread INTERFACE)
	target_link_libraries(mariadbclient PUBLIC log)
endif()

add_subdirectory(${M2_SERVER_SRC}/vendor/spdlog-1.15.3 vendor/spdlog)
add_subdirectory(${M2_SERVER_SRC}/vendor/libsodium vendor/libsodium)
add_subdirectory(${M2_SERVER_SRC}/src src)

if(ANDROID)
	# Packaged as lib<name>.so so the APK installer extracts them into
	# nativeLibraryDir, the only exec-allowed location on API 29+.
	# Lua 5.0 keeps string bytes after the TString header; FORTIFY's
	# object-size check in strchr() aborts on that layout.
	target_compile_options(liblua PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
	foreach(t db game)
		target_link_options(${t} PRIVATE -pie)
		set_target_properties(${t} PROPERTIES OUTPUT_NAME "m2${t}" SUFFIX ".so" PREFIX "lib")
	endforeach()
endif()
