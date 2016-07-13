// mupen64plus-video-videocore/VCCombiner.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VCCOMBINER_H
#define VCCOMBINER_H

#include <stdint.h>

#define VC_COMBINER_CCMUX_COMBINED		0
#define VC_COMBINER_CCMUX_TEXEL0			1
#define VC_COMBINER_CCMUX_TEXEL1			2
#define VC_COMBINER_CCMUX_PRIMITIVE		3
#define VC_COMBINER_CCMUX_SHADE			4
#define VC_COMBINER_CCMUX_ENVIRONMENT		5
#define VC_COMBINER_CCMUX_CENTER			6
#define VC_COMBINER_CCMUX_SCALE                  7
#define VC_COMBINER_CCMUX_COMBINED_ALPHA 8
#define VC_COMBINER_CCMUX_TEXEL0_ALPHA   9
#define VC_COMBINER_CCMUX_TEXEL1_ALPHA   10
#define VC_COMBINER_CCMUX_PRIMITIVE_ALPHA        11
#define VC_COMBINER_CCMUX_SHADE_ALPHA            12
#define VC_COMBINER_CCMUX_ENV_ALPHA              13
#define VC_COMBINER_CCMUX_LOD_FRACTION   14
#define VC_COMBINER_CCMUX_PRIM_LOD_FRAC  15
#define VC_COMBINER_CCMUX_NOISE                  16
#define VC_COMBINER_CCMUX_K4                             17
#define VC_COMBINER_CCMUX_K5                             18
#define VC_COMBINER_CCMUX_1                              19
#define VC_COMBINER_CCMUX_0                              31

#define VC_COMBINER_ACMUX_COMBINED		0
#define VC_COMBINER_ACMUX_TEXEL0			1
#define VC_COMBINER_ACMUX_TEXEL1			2
#define VC_COMBINER_ACMUX_PRIMITIVE		3
#define VC_COMBINER_ACMUX_SHADE			4
#define VC_COMBINER_ACMUX_ENVIRONMENT		5
#define VC_COMBINER_ACMUX_LOD_FRACTION   6
#define VC_COMBINER_ACMUX_PRIM_LOD_FRAC  7
#define VC_COMBINER_ACMUX_1                              8
#define VC_COMBINER_ACMUX_0                              9

struct VCColorf;
struct VCCombiner;

struct VCUnpackedCombiner {
    uint8_t saRGB;
    uint8_t saA;
    uint8_t sbRGB;
    uint8_t sbA;
    uint8_t mRGB;
    uint8_t mA;
    uint8_t aRGB;
    uint8_t aA;
};

void VCCombiner_UnpackCurrentRGBCombiner(VCUnpackedCombiner *cycle0, VCUnpackedCombiner *cycle1);
void VCCombiner_UnpackCurrentACombiner(VCUnpackedCombiner *cycle0, VCUnpackedCombiner *cycle1);

#if 0
void VCCombiner_FillCombiner(VCCombiner *combiner, VCColorf *shade);
void VCCombiner_FillCombinerForTextureBlit(VCCombiner *vcCombiner);
void VCCombiner_FillCombinerForRectFill(VCCombiner *vcCombiner, VCColorf *fillColor);
#endif

#endif

