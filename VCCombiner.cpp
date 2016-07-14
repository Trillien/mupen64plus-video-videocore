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

#if 0
static void VCCombiner_ZeroCombineStage(VCColorf *encodedCombine) {
    encodedCombine->r = encodedCombine->g = encodedCombine->b = encodedCombine->a = 0.0;
}

static void VCCombiner_FillCombineStage(VCColorf *encodedCombine,
                                        VCColorf *shade,
                                        uint8_t rgbSource,
                                        uint8_t aSource) {
    switch (rgbSource) {
    case G_CCMUX_COMBINED:
    case G_CCMUX_COMBINED_ALPHA:
        //printf("FIXME: Support G_CCMUX_COMBINED/G_CCMUX_COMBINED_ALPHA for RGB!\n");
        encodedCombine->r = encodedCombine->g = encodedCombine->b = 0.0;
        break;
    case G_CCMUX_TEXEL0:
        encodedCombine->r = encodedCombine->b = -1.0;
        encodedCombine->g = 0.0;
        break;
    case G_CCMUX_TEXEL0_ALPHA:
        encodedCombine->r = encodedCombine->g = encodedCombine->b = -1.0;
        break;
    case G_CCMUX_TEXEL1:
        encodedCombine->r = -1.0;
        encodedCombine->g = encodedCombine->b = 0.0;
        break;
    case G_CCMUX_TEXEL1_ALPHA:
        encodedCombine->r = encodedCombine->g = -1.0;
        encodedCombine->b = 0.0;
        return;
    case G_CCMUX_SHADE:
        encodedCombine->r = shade->r;
        encodedCombine->g = shade->g;
        encodedCombine->b = shade->b;
        break;
    case G_CCMUX_PRIMITIVE:
        encodedCombine->r = gDP.primColor.r;
        encodedCombine->g = gDP.primColor.g;
        encodedCombine->b = gDP.primColor.b;
        break;
    case G_CCMUX_ENVIRONMENT:
        encodedCombine->r = gDP.envColor.r;
        encodedCombine->g = gDP.envColor.g;
        encodedCombine->b = gDP.envColor.b;
        break;
    case G_CCMUX_PRIMITIVE_ALPHA:
        encodedCombine->r = encodedCombine->g = encodedCombine->b = gDP.primColor.a;
        break;
    case G_CCMUX_ENV_ALPHA:
        encodedCombine->r = encodedCombine->g = encodedCombine->b = gDP.envColor.a;
        break;
    case G_CCMUX_0:
    case G_CCMUX_K5:
        encodedCombine->r = encodedCombine->g = encodedCombine->b = 0.0;
        break;
    case G_CCMUX_1:
        encodedCombine->r = encodedCombine->g = encodedCombine->b = 1.0;
        break;
    case VC_CCMUX_SHADE_TIMES_ENVIRONMENT:
        encodedCombine->r = gDP.envColor.r * shade->r;
        encodedCombine->g = gDP.envColor.g * shade->g;
        encodedCombine->b = gDP.envColor.b * shade->b;
        break;
    case VC_CCMUX_SHADE_TIMES_PRIMITIVE:
        encodedCombine->r = gDP.primColor.r * shade->r;
        encodedCombine->g = gDP.primColor.g * shade->g;
        encodedCombine->b = gDP.primColor.b * shade->b;
        break;
    default:
        //printf("FIXME: Support %d for RGB!\n", (int)rgbSource);
        encodedCombine->r = encodedCombine->g = encodedCombine->b = 0.0;
        break;
    }

    switch (aSource) {
    case G_ACMUX_COMBINED:
        //printf("FIXME: Support G_ACMUX_COMBINED/G_ACMUX_COMBINED_ALPHA for A!\n");
        encodedCombine->a = 0.0;
        break;
    case G_ACMUX_TEXEL0:
        encodedCombine->a = -200.0;
        break;
    case G_ACMUX_TEXEL1:
        encodedCombine->a = -1.0;
        break;
    case G_ACMUX_SHADE:
        encodedCombine->a = shade->a;
        break;
    case G_ACMUX_PRIMITIVE:
        encodedCombine->a = gDP.primColor.a;
        break;
    case G_ACMUX_ENVIRONMENT:
        encodedCombine->a = gDP.envColor.a;
        break;
    case G_ACMUX_0:
        encodedCombine->a = 0.0;
        break;
    case G_ACMUX_1:
        encodedCombine->a = 1.0;
        break;
    default:
        printf("FIXME: Support %d for A!\n", (int)rgbSource);
        encodedCombine->a = 0.0;
        break;
    }
}

static void VCCombiner_SimplifyRGBCombiner(VCUnpackedCombiner *unpacked) {
    VCUnpackedCombiner cycle1;
    VCCombiner_UnpackCurrentRGBCombiner(unpacked, &cycle1);

    if (unpacked->saRGB == cycle1.saRGB &&
            unpacked->sbRGB == cycle1.sbRGB &&
            unpacked->mRGB == cycle1.mRGB &&
            unpacked->aRGB == cycle1.aRGB) {
        return;
    }

    if (cycle1.saRGB == G_CCMUX_0 &&
            cycle1.sbRGB == G_CCMUX_0 &&
            cycle1.mRGB == G_CCMUX_0 &&
            cycle1.aRGB == G_CCMUX_COMBINED) {
        return;
    }

    // Zelda:

   //Cycle 0: (TEXEL0-0)*SHADE+0; Cycle 1: (COMBINED-0)*PRIMITIVE+0 
    // Associativity: ((sa0-sb0)*m0)*m1+a1 == (sa0-sb0)*(m0*m1)+a1
    if (unpacked->mRGB == G_CCMUX_SHADE &&
            unpacked->aRGB == G_CCMUX_0 &&
            cycle1.sbRGB == G_CCMUX_0 &&
            (cycle1.saRGB == G_CCMUX_COMBINED && cycle1.mRGB == G_CCMUX_PRIMITIVE) ||
            (cycle1.saRGB == G_CCMUX_PRIMITIVE && cycle1.mRGB == G_CCMUX_COMBINED)) {
        unpacked->saRGB = gDP.combine.saRGB0;
        unpacked->sbRGB = gDP.combine.sbRGB0;
        unpacked->mRGB = VC_CCMUX_SHADE_TIMES_PRIMITIVE;
        unpacked->aRGB = gDP.combine.aRGB1;
        return;
    }

    // Associativity: ((sa0-sb0)*m0)*m1+a1 == (sa0-sb0)*(m0*m1)+a1
    if (unpacked->mRGB == G_CCMUX_SHADE &&
            unpacked->aRGB == G_CCMUX_0 &&
            cycle1.sbRGB == G_CCMUX_0 &&
            (cycle1.saRGB == G_CCMUX_COMBINED && cycle1.mRGB == G_CCMUX_ENVIRONMENT) ||
            (cycle1.saRGB == G_CCMUX_ENVIRONMENT && cycle1.mRGB == G_CCMUX_COMBINED)) {
        unpacked->mRGB = VC_CCMUX_SHADE_TIMES_ENVIRONMENT;
        unpacked->aRGB = cycle1.aRGB;
        return;
    }

    // Distributive property: ((sa0-sb0)*m0+a0)*m1 == (sa0-sb0)*m0*m1+a0*m1
    // Associativity: ((sa0-sb0)*m0*m1+a0*m1) == (sa0*m1-sb0*m1)*m0+a0*m1
    if (unpacked->saRGB == G_CCMUX_PRIMITIVE &&
            unpacked->sbRGB == G_CCMUX_ENVIRONMENT &&
            unpacked->aRGB == G_CCMUX_ENVIRONMENT &&
            cycle1.saRGB == G_CCMUX_COMBINED &&
            cycle1.sbRGB == G_CCMUX_0 &&
            cycle1.mRGB == G_CCMUX_SHADE &&
            cycle1.aRGB == G_CCMUX_0) {
        unpacked->saRGB = VC_CCMUX_SHADE_TIMES_PRIMITIVE;
        unpacked->sbRGB = VC_CCMUX_SHADE_TIMES_ENVIRONMENT;
        unpacked->aRGB = VC_CCMUX_SHADE_TIMES_ENVIRONMENT;
        return;
    }

    if (unpacked->saRGB == G_CCMUX_TEXEL1 &&
            unpacked->sbRGB == G_CCMUX_TEXEL0 &&
            unpacked->mRGB == G_CCMUX_ENV_ALPHA &&
            unpacked->aRGB == G_CCMUX_TEXEL0 &&
            cycle1.saRGB == G_CCMUX_COMBINED &&
            cycle1.sbRGB == G_CCMUX_0 &&
            cycle1.mRGB == G_CCMUX_SHADE &&
            cycle1.aRGB == G_CCMUX_0) {
        // FIXME(tachi)
        return;
    }

#if 0
    printf("Cycle 0: (%s-%s)*%s+%s; Cycle 1: (%s-%s)*%s+%s\n",
           saRGBText[gDP.combine.saRGB0],
           sbRGBText[gDP.combine.sbRGB0],
           mRGBText[gDP.combine.mRGB0],
           aRGBText[gDP.combine.aRGB0],
           saRGBText[gDP.combine.saRGB1],
           sbRGBText[gDP.combine.sbRGB1],
           mRGBText[gDP.combine.mRGB1],
           aRGBText[gDP.combine.aRGB1]);
#endif
}

static void VCCombiner_SimplifyACombiner(VCUnpackedCombiner *unpacked) {
    VCUnpackedCombiner cycle1;
    VCCombiner_UnpackCurrentACombiner(unpacked, &cycle1);
}

void VCCombiner_FillCombiner(VCCombiner *vcCombiner, VCColorf *shade) {
    VCUnpackedCombiner unpacked;
    VCCombiner_SimplifyRGBCombiner(&unpacked);
    VCCombiner_SimplifyACombiner(&unpacked);

#if 0
    if (gDP.combine.saRGB0 != gDP.combine.saRGB1 ||
            gDP.combine.sbRGB0 != gDP.combine.sbRGB1 ||
            gDP.combine.mRGB0 != gDP.combine.mRGB1 ||
            gDP.combine.aRGB0 != gDP.combine.aRGB1) {
        printf("Cycle 0: (%s-%s)*%s+%s; Cycle 1: (%s-%s)*%s+%s\n",
               saRGBText[gDP.combine.saRGB0],
               sbRGBText[gDP.combine.sbRGB0],
               mRGBText[gDP.combine.mRGB0],
               aRGBText[gDP.combine.aRGB0],
               saRGBText[gDP.combine.saRGB1],
               sbRGBText[gDP.combine.sbRGB1],
               mRGBText[gDP.combine.mRGB1],
               aRGBText[gDP.combine.aRGB1]);
#endif
    VCCombiner_FillCombineStage(&vcCombiner->combineA, shade, unpacked.saRGB, unpacked.saA);
    VCCombiner_FillCombineStage(&vcCombiner->combineB, shade, unpacked.sbRGB, unpacked.sbA);
    VCCombiner_FillCombineStage(&vcCombiner->combineC, shade, unpacked.mRGB, unpacked.mA);
    VCCombiner_FillCombineStage(&vcCombiner->combineD, shade, unpacked.aRGB, unpacked.aA);
}

void VCCombiner_FillCombinerForTextureBlit(VCCombiner *vcCombiner) {
    VCCombiner_ZeroCombineStage(&vcCombiner->combineA);
    VCCombiner_ZeroCombineStage(&vcCombiner->combineB);
    VCCombiner_ZeroCombineStage(&vcCombiner->combineC);
    VCCombiner_FillCombineStage(&vcCombiner->combineD,
                                NULL,
                                VC_COMBINER_CCMUX_TEXEL0,
                                VC_COMBINER_ACMUX_TEXEL0);
}

void VCCombiner_FillCombinerForRectFill(VCCombiner *vcCombiner, VCColorf *fillColor) {
    VCCombiner_ZeroCombineStage(&vcCombiner->combineA);
    VCCombiner_ZeroCombineStage(&vcCombiner->combineB);
    VCCombiner_ZeroCombineStage(&vcCombiner->combineC);
    VCCombiner_FillCombineStage(&vcCombiner->combineD,
                                fillColor,
                                VC_COMBINER_CCMUX_SHADE,
                                VC_COMBINER_ACMUX_SHADE);
}
#endif

