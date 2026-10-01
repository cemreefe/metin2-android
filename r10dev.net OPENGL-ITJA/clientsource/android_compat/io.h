#pragma once
// Minimal <io.h> shim for Android — file attribute constants + _findfirst API.

#include "windows.h"

#ifndef _A_NORMAL
#define _A_NORMAL   0x00
#define _A_RDONLY   0x01
#define _A_HIDDEN   0x02
#define _A_SYSTEM   0x04
#define _A_SUBDIR   0x10
#define _A_ARCH     0x20
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _finddata_t {
    unsigned    attrib;
    time_t      time_create;
    time_t      time_access;
    time_t      time_write;
    long        size;      // _fsize_t
    char        name[260];
} _finddata_t;

typedef struct _finddata64_t {
    unsigned    attrib;
    time_t      time_create;
    time_t      time_access;
    time_t      time_write;
    int64_t     size;
    char        name[260];
} _finddata64_t;

intptr_t _findfirst(const char* filespec, _finddata_t* fileinfo);
int      _findnext(intptr_t handle, _finddata_t* fileinfo);
int      _findclose(intptr_t handle);
intptr_t _findfirst64(const char* filespec, _finddata64_t* fileinfo);
int      _findnext64(intptr_t handle, _finddata64_t* fileinfo);

#ifdef __cplusplus
}
#endif
