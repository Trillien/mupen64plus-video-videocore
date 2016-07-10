// mupen64plus-video-videocore/VCGeometry.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include <math.h>
#include <stdint.h>
#include "VCGeometry.h"

VCColor VCColor_ColorFToColor(VCColorf colorf) {
    VCColor color = {
        (uint8_t)round(colorf.r * 255.0),
        (uint8_t)round(colorf.g * 255.0),
        (uint8_t)round(colorf.b * 255.0),
        (uint8_t)round(colorf.a * 255.0),
    };
    return color;
}

