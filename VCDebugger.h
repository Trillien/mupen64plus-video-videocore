// mupen64plus-video-videocore/VCDebugger.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VCDEBUGGER_H
#define VCDEBUGGER_H

#include "VCGL.h"
#include "VCGeometry.h"
#include <stdlib.h>

#define VC_SAMPLES_IN_WINDOW    (5*30)

struct VCRenderer;

struct VCDebugStat {
    uint32_t sum;
    uint32_t samples[VC_SAMPLES_IN_WINDOW];
};

struct VCDebugStats {
    VCDebugStat trianglesDrawn;
    VCDebugStat batches;
    VCDebugStat texturesUploaded;
    VCDebugStat programsCreated;
    VCDebugStat prepareTime;
    VCDebugStat drawTime;
    VCDebugStat viRate;
    uint32_t sampleCount;
};

struct VCDebugVertex {
    VCPoint2f position;
    VCColorf color;
    VCPoint2f textureUV;
};

struct VCDebugger {
    VCProgram program;
    VCDebugStats stats;
    GLuint fontTexture;
    GLuint vbo;
    VCDebugVertex *vertices;
    size_t verticesLength;
    size_t verticesCapacity;
};

void VCDebugger_Init(VCDebugger *debugger, VCRenderer *renderer);
void VCDebugger_DrawDebugOverlay(VCDebugger *debugger, const VCSize2u *windowSize);
void VCDebugger_NewFrame(VCDebugger *debugger);
void VCDebugger_AddSample(VCDebugger *debugger, VCDebugStat *stat, uint32_t newSample);
void VCDebugger_IncrementSample(VCDebugger *debugger, VCDebugStat *stat);

#endif

