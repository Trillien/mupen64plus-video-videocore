// mupen64plus-video-videocore/VCUtils.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VCUTILS_H
#define VCUTILS_H

#include <stddef.h>

struct VCString {
    char *ptr;
    size_t len;
    size_t cap;
};

inline float VCUtils_MinF(float a, float b) {
    return (a < b) ? a : b;
}

size_t VCUtils_NextPowerOfTwo(size_t n);
VCString VCString_Create();
void VCString_Destroy(VCString *string);
VCString VCString_Duplicate(VCString *string);
void VCString_Reserve(VCString *string, size_t amount);
void VCString_AppendString(VCString *string, const VCString *other);
void VCString_AppendBytes(VCString *string, const void *bytes, size_t size);
void VCString_AppendCString(VCString *string, const char *cString);
size_t VCString_AppendFormat(VCString *string, const char *fmt, ...);

char *xstrsep(char **stringp, const char* delim);

#endif

