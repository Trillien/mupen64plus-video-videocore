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

