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

VCString VCString_Create();
void VCString_Destroy(VCString *string);
void VCString_Reserve(VCString *string, size_t amount);
void VCString_AppendCString(VCString *string, const char *cString);
size_t VCString_AppendFormat(VCString *string, const char *fmt, ...);

char *xstrsep(char **stringp, const char* delim);

#endif

