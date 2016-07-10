// mupen64plus-video-videocore/VCCombiner.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VCCOMBINER_H
#define VCCOMBINER_H

#include <stdint.h>

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

