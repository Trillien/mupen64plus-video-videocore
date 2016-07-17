// mupen64plus-video-videocore/VCGeometry.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include <math.h>
#include <stdint.h>
#include "VCGeometry.h"

VCColor VCColor_ColorFToColor(VCColorf colorf) {
    VCColor color = {
        (uint8_t)round(fmin(colorf.r, 1.0) * 255.0),
        (uint8_t)round(fmin(colorf.g, 1.0) * 255.0),
        (uint8_t)round(fmin(colorf.b, 1.0) * 255.0),
        (uint8_t)round(fmin(colorf.a, 1.0) * 255.0),
    };
    return color;
}

VCPoint3f VCPoint3f_Cross(const VCPoint3f *a, const VCPoint3f *b) {
    VCPoint3f c = {
        a->y * b->z - a->z * b->y,
        a->z * b->x - a->x * b->z,
        a->x * b->y - a->y * b->x
    };
    return c;
}

VCPoint3f VCPoint3f_Sub(const VCPoint3f *a, const VCPoint3f *b) {
    VCPoint3f c = {
        a->x - b->x,
        a->y - b->y,
        a->z - b->z
    };
    return c;
}

VCPoint3f VCPoint3f_ScalarDiv(const VCPoint3f *a, float b) {
    VCPoint3f c = {
        a->x / b,
        a->y / b,
        a->z / b
    };
    return c;
} 

VCPoint3f VCPoint4f_To3f(const VCPoint4f *a) {
    VCPoint3f b = {
        a->x,
        a->y,
        a->z
    };
    return b;
} 

