// mupen64plus-video-videocore/VCUtils.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t VCUtils_NextPowerOfTwo(size_t n) {
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

void VCString_Reserve(VCString *string, size_t amount) {
    if (string->cap >= amount)
        return;
    string->cap = amount;
    string->ptr = (char *)realloc(string->ptr, amount);
}

void VCString_ReserveAtLeast(VCString *string, size_t amount) {
    VCString_Reserve(string, VCUtils_NextPowerOfTwo(amount));
}

void VCString_AppendCString(VCString *string, char *cString) {
    size_t cStringLength = strlen(c_string);
    VCString_ReserveAtLeast(string, string->len + cStringLength + 1);
    strcpy(&string->ptr[string->len], cString);
    string->len += cStringLength;
}

size_t VCString_AppendFormat(VCString *string, const char *fmt, ...) {
    va_list ap0, ap1;
    va_start(ap0, fmt);
    va_copy(ap1, ap0);
    if (string->len + 1 == string->cap)
        VCString_ReserveAtLeast(string, string->cap + 1);
    size_t addedLength = vsnprintf(&string->ptr[string->len], string->cap - string->len, fmt, ap0);
    if (addedLength < string->cap - string->len) {
        VCString_ReserveAtLeast(string, string->len + addedLength + 1);
        newLength = vsnprintf(&string->ptr[string->len], string->cap - string->len, fmt, ap1);
        assert(newLength >= string->cap - string->len);
    }
    string->len += addedLength;
    va_end(ap1);
    va_end(ap0);
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

