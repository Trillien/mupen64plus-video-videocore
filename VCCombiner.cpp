// mupen64plus-video-videocore/VCCombiner.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include "Combiner.h"
#include "GBI.h"
#include "VCCombiner.h"

#if 0
#define VC_CCMUX_SHADE_TIMES_ENVIRONMENT    32
#define VC_CCMUX_SHADE_TIMES_PRIMITIVE      33
#endif

static const uint8_t saRGBMapping[] = {
	VC_COMBINER_CCMUX_COMBINED,    VC_COMBINER_CCMUX_TEXEL0,
    VC_COMBINER_CCMUX_TEXEL1,		    VC_COMBINER_CCMUX_PRIMITIVE, 
	VC_COMBINER_CCMUX_SHADE,			VC_COMBINER_CCMUX_ENVIRONMENT,
    VC_COMBINER_CCMUX_NOISE,			VC_COMBINER_CCMUX_1,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0
};

static const uint8_t sbRGBMapping[] = {
	VC_COMBINER_CCMUX_COMBINED,		VC_COMBINER_CCMUX_TEXEL0,
    VC_COMBINER_CCMUX_TEXEL1,		    VC_COMBINER_CCMUX_PRIMITIVE, 
	VC_COMBINER_CCMUX_SHADE,			VC_COMBINER_CCMUX_ENVIRONMENT,
    VC_COMBINER_CCMUX_CENTER,		    VC_COMBINER_CCMUX_K4,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0
};

static const uint8_t mRGBMapping[] = {
	VC_COMBINER_CCMUX_COMBINED,		VC_COMBINER_CCMUX_TEXEL0,
    VC_COMBINER_CCMUX_TEXEL1,	        VC_COMBINER_CCMUX_PRIMITIVE, 
	VC_COMBINER_CCMUX_SHADE,			VC_COMBINER_CCMUX_ENVIRONMENT,
    VC_COMBINER_CCMUX_SCALE,		    VC_COMBINER_CCMUX_COMBINED_ALPHA,
	VC_COMBINER_CCMUX_TEXEL0_ALPHA,	VC_COMBINER_CCMUX_TEXEL1_ALPHA,
    VC_COMBINER_CCMUX_PRIMITIVE_ALPHA, VC_COMBINER_CCMUX_SHADE_ALPHA,
	VC_COMBINER_CCMUX_ENV_ALPHA,		VC_COMBINER_CCMUX_LOD_FRACTION,
    VC_COMBINER_CCMUX_PRIM_LOD_FRAC,	VC_COMBINER_CCMUX_K5,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
	VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0,
    VC_COMBINER_CCMUX_0,				VC_COMBINER_CCMUX_0
};

static const uint8_t aRGBMapping[] = {
	VC_COMBINER_CCMUX_COMBINED,		VC_COMBINER_CCMUX_TEXEL0,
    VC_COMBINER_CCMUX_TEXEL1,		    VC_COMBINER_CCMUX_PRIMITIVE, 
	VC_COMBINER_CCMUX_SHADE,			VC_COMBINER_CCMUX_ENVIRONMENT,
    VC_COMBINER_CCMUX_1,				VC_COMBINER_CCMUX_0,
};

static uint8_t saAMapping[] = {
	VC_COMBINER_ACMUX_COMBINED,		VC_COMBINER_ACMUX_TEXEL0,
    VC_COMBINER_ACMUX_TEXEL1,			VC_COMBINER_ACMUX_PRIMITIVE, 
	VC_COMBINER_ACMUX_SHADE,			VC_COMBINER_ACMUX_ENVIRONMENT,
    VC_COMBINER_ACMUX_1,				VC_COMBINER_ACMUX_0,
};

static uint8_t sbAMapping[] = {
	VC_COMBINER_ACMUX_COMBINED,		VC_COMBINER_ACMUX_TEXEL0,
    VC_COMBINER_ACMUX_TEXEL1,			VC_COMBINER_ACMUX_PRIMITIVE, 
	VC_COMBINER_ACMUX_SHADE,			VC_COMBINER_ACMUX_ENVIRONMENT,
    VC_COMBINER_ACMUX_1,				VC_COMBINER_ACMUX_0,
};

static uint8_t mAMapping[] = {
	VC_COMBINER_ACMUX_LOD_FRACTION,	VC_COMBINER_ACMUX_TEXEL0,
    VC_COMBINER_ACMUX_TEXEL1,			VC_COMBINER_ACMUX_PRIMITIVE, 
	VC_COMBINER_ACMUX_SHADE,			VC_COMBINER_ACMUX_ENVIRONMENT,
    VC_COMBINER_ACMUX_PRIM_LOD_FRAC,	VC_COMBINER_ACMUX_0,
};

static uint8_t aAMapping[] = {
	VC_COMBINER_ACMUX_COMBINED,		VC_COMBINER_ACMUX_TEXEL0,
    VC_COMBINER_ACMUX_TEXEL1,			VC_COMBINER_ACMUX_PRIMITIVE, 
	VC_COMBINER_ACMUX_SHADE,			VC_COMBINER_ACMUX_ENVIRONMENT,
    VC_COMBINER_ACMUX_1,				VC_COMBINER_ACMUX_0,
};

static const char *saRGBText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"NOISE",			"1",
	"0",				"0",				"0",				"0",
	"0",				"0",				"0",				"0"
};

static const char *sbRGBText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"CENTER",			"K4",
	"0",				"0",				"0",				"0",
	"0",				"0",				"0",				"0"
};

static const char *mRGBText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"SCALE",			"COMBINED_ALPHA",
	"TEXEL0_ALPHA",		"TEXEL1_ALPHA",		"PRIMITIVE_ALPHA",	"SHADE_ALPHA",
	"ENV_ALPHA",		"LOD_FRACTION",		"PRIM_LOD_FRAC",	"K5",
	"0",				"0",				"0",				"0",
	"0",				"0",				"0",				"0",
	"0",				"0",				"0",				"0",
	"0",				"0",				"0",				"0"
};

static const char *aRGBText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"1",				"0",
};

static const char *saAText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"1",				"0",
};

static const char *sbAText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"1",				"0",
};

static const char *mAText[] =
{
	"LOD_FRACTION",		"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"PRIM_LOD_FRAC",	"0",
};

static const char *aAText[] =
{
	"COMBINED",			"TEXEL0",			"TEXEL1",			"PRIMITIVE", 
	"SHADE",			"ENVIRONMENT",		"1",				"0",
};

void VCCombiner_UnpackCurrentRGBCombiner(VCUnpackedCombiner *cycle0, VCUnpackedCombiner *cycle1) {
    switch (gDP.otherMode.cycleType) {
    case G_CYC_COPY:
        cycle0->saRGB = cycle1->saRGB = VC_COMBINER_CCMUX_0;
        cycle0->sbRGB = cycle1->sbRGB = VC_COMBINER_CCMUX_0;
        cycle0->mRGB = cycle1->mRGB = VC_COMBINER_CCMUX_0;
        cycle0->aRGB = cycle1->aRGB = VC_COMBINER_CCMUX_TEXEL0;
        return;
    case G_CYC_FILL:
        cycle0->saRGB = cycle1->saRGB = VC_COMBINER_CCMUX_0;
        cycle0->sbRGB = cycle1->sbRGB = VC_COMBINER_CCMUX_0;
        cycle0->mRGB = cycle1->mRGB = VC_COMBINER_CCMUX_0;
        cycle0->aRGB = cycle1->aRGB = VC_COMBINER_CCMUX_SHADE;
        return;
    default:
        cycle0->saRGB = saRGBMapping[gDP.combine.saRGB0];
        cycle0->sbRGB = sbRGBMapping[gDP.combine.sbRGB0];
        cycle0->mRGB = mRGBMapping[gDP.combine.mRGB0];
        cycle0->aRGB = aRGBMapping[gDP.combine.aRGB0];

        cycle1->saRGB = saRGBMapping[gDP.combine.saRGB1];
        cycle1->sbRGB = sbRGBMapping[gDP.combine.sbRGB1];
        cycle1->mRGB = mRGBMapping[gDP.combine.mRGB1];
        cycle1->aRGB = aRGBMapping[gDP.combine.aRGB1];
    }
}

void VCCombiner_UnpackCurrentACombiner(VCUnpackedCombiner *cycle0, VCUnpackedCombiner *cycle1) {
    switch (gDP.otherMode.cycleType) {
    case G_CYC_COPY:
        cycle0->saA = cycle1->saA = VC_COMBINER_ACMUX_0;
        cycle0->sbA = cycle1->sbA = VC_COMBINER_ACMUX_0;
        cycle0->mA = cycle1->mA = VC_COMBINER_ACMUX_0;
        cycle0->aA = cycle1->aA = VC_COMBINER_ACMUX_TEXEL0;
        return;
    case G_CYC_FILL:
        cycle0->saA = cycle1->saA = VC_COMBINER_ACMUX_0;
        cycle0->sbA = cycle1->sbA = VC_COMBINER_ACMUX_0;
        cycle0->mA = cycle1->mA = VC_COMBINER_ACMUX_0;
        cycle0->aA = cycle1->aA = VC_COMBINER_ACMUX_1;
        return;
    default:
        cycle0->saA = saAMapping[gDP.combine.saA0];
        cycle0->sbA = sbAMapping[gDP.combine.sbA0];
        cycle0->mA = mAMapping[gDP.combine.mA0];
        cycle0->aA = aAMapping[gDP.combine.aA0];

        cycle1->saA = saAMapping[gDP.combine.saA1];
        cycle1->sbA = sbAMapping[gDP.combine.sbA1];
        cycle1->mA = mAMapping[gDP.combine.mA1];
        cycle1->aA = aAMapping[gDP.combine.aA1];
    }
}

