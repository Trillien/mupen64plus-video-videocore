// mupen64plus-video-videocore/VCUtils.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include "VCUtils.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t VCUtils_NextPowerOfTwo(size_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

VCString VCString_Create() {
    VCString string = { NULL, 0, 0 };
    return string;
}

void VCString_Destroy(VCString *string) {
    free(string->ptr);
    string->ptr = NULL;
    string->len = 0;
    string->cap = 0;
}

VCString VCString_Duplicate(VCString *string) {
    VCString newString;
    newString.len = string->len;
    newString.cap = newString.len + 1;
    newString.ptr = (char *)malloc(newString.cap);
    memcpy(newString.ptr, string->ptr, newString.cap);
    return newString;
}

void VCString_Reserve(VCString *string, size_t amount) {
    if (string->cap >= amount)
        return;
    string->cap = amount;
    string->ptr = (char *)realloc(string->ptr, amount);
}

void VCString_ReserveAtLeast(VCString *string, size_t amount) {
    VCString_Reserve(string, VCUtils_NextPowerOfTwo(amount));
}

void VCString_AppendString(VCString *string, const VCString *other) {
    VCString_AppendBytes(string, other->ptr, other->len);
}

void VCString_AppendBytes(VCString *string, const void *bytes, size_t size) {
    VCString_ReserveAtLeast(string, string->len + size + 1);
    memcpy(&string->ptr[string->len], bytes, size);
    string->len += size;
    string->ptr[string->len] = '\0';
    assert(string->len < string->cap);
}

void VCString_AppendCString(VCString *string, const char *cString) {
    size_t cStringLength = strlen(cString);
    VCString_ReserveAtLeast(string, string->len + cStringLength + 1);
    strcpy(&string->ptr[string->len], cString);
    string->len += cStringLength;
    assert(string->len < string->cap);
}

size_t VCString_AppendFormat(VCString *string, const char *fmt, ...) {
    va_list ap0, ap1;
    va_start(ap0, fmt);
    va_copy(ap1, ap0);
    size_t addedLength = vsnprintf(NULL, 0, fmt, ap0);
    va_end(ap0);
    VCString_ReserveAtLeast(string, string->len + addedLength + 1);
    string->len += vsnprintf(&string->ptr[string->len], string->cap - string->len, fmt, ap1);
    assert(string->len < string->cap);
    va_end(ap1);
    return addedLength;
}

char *xstrsep(char **stringp, const char* delim) {
    char *start = *stringp;
    char *p = (start != NULL) ? strpbrk(start, delim) : NULL;
    if (p == NULL) {
        *stringp = NULL;
    } else {
        *p = '\0';
        *stringp = p + 1;
    }
    return start;
}

