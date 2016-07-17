// mupen64plus-video-videocore/VCGeometry.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VC_GEOMETRY_H
#define VC_GEOMETRY_H

#include <stdint.h>

struct VCPoint2f {
    float x;
    float y;
};

struct VCPoint2s {
    int16_t x;
    int16_t y;
};

struct VCPoint2i {
    int32_t x;
    int32_t y;
};

struct VCPoint2us {
    uint16_t x;
    uint16_t y;
};

struct VCPoint2u {
    uint32_t x;
    uint32_t y;
};

struct VCPoint3f {
    float x;
    float y;
    float z;
};

struct VCPoint4f {
    float x;
    float y;
    float z;
    float w;
};

struct VCSize2f {
    float width;
    float height;
};

struct VCSize2i {
    int32_t width;
    int32_t height;
};

struct VCSize2s {
    int16_t width;
    int16_t height;
};

struct VCSize2us {
    uint16_t width;
    uint16_t height;
};

struct VCSize2u {
    uint32_t width;
    uint32_t height;
};

struct VCRectf {
    VCPoint2f origin;
    VCSize2f size;
};

struct VCRects {
    VCPoint2s origin;
    VCSize2s size;
};

struct VCRecti {
    VCPoint2i origin;
    VCSize2i size;
};

struct VCRectus {
    VCPoint2us origin;
    VCSize2us size;
};

struct VCRectu {
    VCPoint2u origin;
    VCSize2u size;
};

struct VCColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct VCColorf {
    float r;
    float g;
    float b;
    float a;
};

VCColor VCColor_ColorFToColor(VCColorf colorf);
VCPoint3f VCPoint3f_Cross(const VCPoint3f *a, const VCPoint3f *b);
VCPoint3f VCPoint3f_Sub(const VCPoint3f *a, const VCPoint3f *b);
VCPoint3f VCPoint4f_To3f(const VCPoint4f *a);
VCPoint3f VCPoint3f_ScalarDiv(const VCPoint3f *a, float b);

#endif

