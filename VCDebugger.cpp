// mupen64plus-video-videocore/VCDebugger.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#define STB_IMAGE_IMPLEMENTATION
#include "VCConfig.h"
#include "VCDebugger.h"
#include "VCGL.h"
#include "VCGeometry.h"
#include "VCRenderer.h"
#include "stb_image.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_HEIGHT                256
#define ATLAS_WIDTH                 128
#define ATLAS_WIDTH_IN_CELLS        10
#define CELL_HEIGHT                 12
#define CELL_WIDTH                  12
#define GLYPHS_PER_FONT             100

#define DEBUG_COUNTERS              7
#define TAB_STOP                    24
#define WINDOW_WIDTH                82

#define INITIAL_VERTICES_CAPACITY   32

#ifdef HAVE_OPENGLES2
#define HORIZONTAL_MARGIN           12
#define SCALE                       4
#define VERTICAL_MARGIN             6
#else
#define HORIZONTAL_MARGIN           2
#define SCALE                       2
#define VERTICAL_MARGIN             2
#endif

const static uint8_t gFontData[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00,
  0x01, 0x03, 0x00, 0x00, 0x00, 0x41, 0x37, 0xec, 0xe4, 0x00, 0x00, 0x00,
  0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b,
  0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49,
  0x4d, 0x45, 0x07, 0xe0, 0x02, 0x1c, 0x16, 0x0f, 0x29, 0x67, 0x1e, 0x51,
  0x04, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xa5, 0xd9, 0x9f, 0xdd, 0x00, 0x00, 0x00, 0x01, 0x74,
  0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01,
  0x62, 0x4b, 0x47, 0x44, 0x01, 0xff, 0x02, 0x2d, 0xde, 0x00, 0x00, 0x03,
  0x32, 0x49, 0x44, 0x41, 0x54, 0x58, 0xc3, 0xd5, 0x96, 0x2f, 0x92, 0x1b,
  0x39, 0x14, 0x87, 0x3f, 0xab, 0x64, 0x59, 0x72, 0x14, 0x3b, 0x70, 0xa0,
  0x8e, 0x30, 0x70, 0xa0, 0x40, 0x40, 0xa0, 0xc1, 0x1e, 0xa0, 0x8f, 0x10,
  0xb0, 0x07, 0x78, 0xd0, 0x50, 0x15, 0x18, 0xd4, 0x30, 0x15, 0x94, 0x23,
  0xf4, 0x11, 0xf6, 0x08, 0x82, 0x0b, 0x73, 0x84, 0x00, 0xb5, 0xdc, 0x52,
  0xef, 0x64, 0xfe, 0x95, 0x67, 0xa7, 0xf2, 0xc8, 0x54, 0x7d, 0xf5, 0x46,
  0xad, 0xb6, 0x7e, 0x7a, 0xfd, 0xc1, 0x3d, 0x35, 0x42, 0x04, 0x2c, 0xd1,
  0x02, 0x58, 0x46, 0x18, 0x74, 0x08, 0x16, 0xd1, 0x05, 0xb0, 0x45, 0xf6,
  0xf1, 0xc4, 0x02, 0x8e, 0x22, 0x5a, 0x02, 0x6d, 0x47, 0xdc, 0xf0, 0x57,
  0xd7, 0x11, 0x54, 0x3c, 0x57, 0x00, 0x6c, 0x09, 0x7a, 0x3c, 0x37, 0x1d,
  0x30, 0xd9, 0xf8, 0x77, 0xd3, 0x51, 0xf6, 0xc1, 0xbc, 0x8f, 0xe7, 0xd7,
  0x58, 0xfe, 0xa8, 0xd5, 0x72, 0x28, 0x06, 0xcd, 0xe4, 0x81, 0x51, 0xcd,
  0x20, 0x79, 0x82, 0x5a, 0x9a, 0x35, 0x49, 0xcf, 0xe0, 0x28, 0x17, 0x10,
  0xfd, 0xdc, 0x71, 0x9c, 0x81, 0x34, 0xff, 0x62, 0x17, 0x00, 0x02, 0x62,
  0x19, 0x34, 0xd9, 0x5f, 0x00, 0x2f, 0x7c, 0x8b, 0xdf, 0x94, 0x1c, 0x18,
  0x0e, 0x0c, 0x66, 0x01, 0x16, 0x51, 0x24, 0x77, 0xf9, 0x35, 0xc7, 0x06,
  0xdc, 0x1e, 0x05, 0x46, 0x4f, 0xd6, 0x0c, 0x3b, 0xc4, 0x32, 0x01, 0xfc,
  0x54, 0x24, 0x4d, 0xda, 0x00, 0x84, 0xa3, 0x40, 0x50, 0x24, 0x3b, 0x83,
  0x1b, 0x80, 0xe0, 0x19, 0x2c, 0x83, 0x41, 0xfe, 0xb3, 0xfb, 0x6b, 0xbe,
  0x8e, 0xe7, 0xce, 0x90, 0x0d, 0xf9, 0x40, 0x36, 0x80, 0x28, 0x3e, 0x3a,
  0x92, 0x25, 0xd9, 0xf2, 0xe4, 0x41, 0xf1, 0xa5, 0x03, 0x37, 0x9a, 0xef,
  0x47, 0xb2, 0x25, 0x1d, 0xc8, 0xef, 0x80, 0xa0, 0xf9, 0x7a, 0xe9, 0x70,
  0xc0, 0x44, 0xb3, 0x86, 0x2b, 0x87, 0x78, 0xe7, 0xe6, 0xa7, 0x88, 0xe1,
  0x95, 0x2a, 0x59, 0x26, 0x87, 0x58, 0x49, 0xa6, 0xbc, 0x4b, 0xb2, 0xc4,
  0x02, 0x5c, 0x39, 0x8c, 0x64, 0x89, 0x7b, 0xe4, 0xbd, 0xfc, 0x33, 0x83,
  0x9f, 0x96, 0xe8, 0x91, 0xbd, 0xfc, 0x70, 0xe4, 0xa6, 0xc3, 0x4a, 0x72,
  0x65, 0xeb, 0xed, 0x1a, 0xb2, 0x2f, 0x40, 0x1c, 0xd9, 0x4a, 0x32, 0xc8,
  0xf6, 0x2a, 0x27, 0x70, 0xcf, 0xf1, 0x66, 0x43, 0x76, 0x9c, 0xad, 0x9c,
  0xad, 0x64, 0x7b, 0xd9, 0x47, 0x01, 0xa1, 0x03, 0xa7, 0x6d, 0xb9, 0xa6,
  0x59, 0x13, 0x1d, 0xa7, 0xbd, 0x04, 0x55, 0x40, 0x52, 0x44, 0xc7, 0xe9,
  0xbd, 0x9c, 0x3a, 0x10, 0xac, 0x9c, 0x55, 0x39, 0xca, 0xe4, 0x89, 0x66,
  0x06, 0xf9, 0xf1, 0x98, 0xf9, 0x27, 0xed, 0x5f, 0x74, 0xbd, 0xed, 0x0b,
  0x18, 0xcb, 0xc5, 0x84, 0x0f, 0x33, 0x28, 0x23, 0xa1, 0x82, 0x58, 0xe6,
  0x8e, 0x21, 0x1b, 0x3e, 0x9b, 0x05, 0x6c, 0x48, 0xb6, 0x1c, 0xd4, 0x0c,
  0x76, 0x24, 0x4b, 0x3a, 0x96, 0xd0, 0x01, 0xd4, 0x98, 0x5d, 0xc0, 0xee,
  0xb2, 0x46, 0xd0, 0x75, 0x84, 0x3c, 0xbc, 0xff, 0x67, 0xd5, 0x50, 0x6e,
  0xbb, 0xa5, 0x2e, 0x2c, 0x75, 0xc2, 0x54, 0x30, 0x19, 0xb2, 0x45, 0x1c,
  0x62, 0xa6, 0xc1, 0x00, 0xe2, 0x48, 0x16, 0xd9, 0x23, 0x2e, 0x24, 0xb7,
  0x00, 0xbf, 0x06, 0x4d, 0xc7, 0x6e, 0x8e, 0x99, 0xb8, 0x90, 0x4c, 0xf9,
  0x25, 0xe6, 0x51, 0x56, 0xeb, 0x6a, 0xb7, 0x48, 0x1e, 0x00, 0xd9, 0x30,
  0x18, 0x26, 0xc7, 0xd9, 0x85, 0xb3, 0x03, 0x92, 0x43, 0x2c, 0xe2, 0x38,
  0xbb, 0x70, 0xaa, 0x40, 0x23, 0x8e, 0x93, 0x0b, 0xa1, 0x02, 0x35, 0x83,
  0x93, 0x03, 0xf2, 0x0e, 0xf1, 0x0c, 0x86, 0x60, 0xa6, 0xf3, 0x0e, 0x90,
  0x4d, 0xf3, 0xb0, 0x0d, 0x20, 0x1b, 0x79, 0xfc, 0xbd, 0xfa, 0x48, 0xa1,
  0xd6, 0x9f, 0x26, 0x3d, 0x47, 0xe4, 0xb6, 0x07, 0x9e, 0x08, 0x6c, 0xa4,
  0x5c, 0x0f, 0x0d, 0x1e, 0x99, 0x3f, 0x1c, 0xc1, 0x12, 0x15, 0xd1, 0x93,
  0x81, 0x5d, 0xc9, 0x58, 0xd4, 0x7c, 0x5b, 0x3a, 0x44, 0x13, 0x35, 0x1f,
  0xca, 0x1a, 0x8a, 0x66, 0x8d, 0xdb, 0xfa, 0xf1, 0x29, 0xfb, 0xf0, 0x94,
  0xc9, 0xb9, 0xda, 0xa9, 0x7a, 0xda, 0x05, 0xfa, 0xc3, 0xaa, 0x35, 0x03,
  0xcb, 0xb8, 0x65, 0xf0, 0x31, 0xda, 0x4e, 0x48, 0xbc, 0x8c, 0x6b, 0x21,
  0xf9, 0xd4, 0xd8, 0xc5, 0x51, 0x82, 0x1a, 0x7e, 0x74, 0x1d, 0x93, 0x1e,
  0xfe, 0xbd, 0xa2, 0x7f, 0x5c, 0xc5, 0x50, 0x74, 0x15, 0x92, 0x6a, 0x28,
  0x87, 0xc5, 0x3f, 0x8a, 0x90, 0xe8, 0x45, 0x59, 0x44, 0xb5, 0x3a, 0xf0,
  0xaa, 0xfe, 0xf1, 0x60, 0xd9, 0xb5, 0xa1, 0x14, 0xff, 0x68, 0x84, 0xa4,
  0xf8, 0x47, 0x23, 0x24, 0x6f, 0xea, 0x1f, 0x70, 0xb7, 0x9e, 0x3e, 0x1f,
  0xfb, 0x89, 0xeb, 0xf9, 0xd2, 0x19, 0xca, 0x8d, 0xe2, 0x7b, 0x27, 0x24,
  0x41, 0xf3, 0xb5, 0x13, 0x92, 0x37, 0xf2, 0x8f, 0x17, 0x1a, 0xca, 0x33,
  0x84, 0xe4, 0x55, 0xfc, 0xe3, 0x85, 0xca, 0xf2, 0x7b, 0x43, 0x39, 0x6d,
  0x09, 0x8f, 0x0a, 0xc9, 0x13, 0xfc, 0xe3, 0xba, 0xd3, 0xb2, 0xf9, 0xc6,
  0x88, 0x27, 0x76, 0x5f, 0x94, 0x62, 0x28, 0xd4, 0x98, 0x5d, 0x74, 0x63,
  0xb1, 0x8b, 0x19, 0xd4, 0x98, 0xdd, 0xd3, 0x71, 0x11, 0x92, 0xba, 0x46,
  0xf0, 0x6b, 0xff, 0xf8, 0x5f, 0x2a, 0x9b, 0x2e, 0x66, 0x20, 0xb6, 0x8b,
  0x19, 0xe4, 0x77, 0x5d, 0xcc, 0xaa, 0x7f, 0x5c, 0x62, 0x06, 0x62, 0xba,
  0x98, 0xbd, 0x59, 0x65, 0xd3, 0xc5, 0x0c, 0x92, 0xeb, 0x62, 0x06, 0xd9,
  0x75, 0x31, 0x03, 0xd9, 0x77, 0x31, 0x03, 0xd9, 0x76, 0x31, 0x7b, 0xed,
  0xd1, 0xdb, 0x1a, 0x8a, 0xae, 0x19, 0xee, 0xfc, 0xe3, 0x73, 0xf5, 0x0f,
  0x3d, 0xfb, 0x87, 0x54, 0xff, 0xb0, 0xb3, 0x7f, 0xe4, 0xb5, 0x7f, 0x48,
  0xe3, 0x1f, 0xb6, 0x5d, 0x03, 0x45, 0x64, 0xe5, 0x1f, 0xfc, 0x09, 0xfe,
  0xb1, 0xf9, 0x05, 0x6c, 0x6c, 0x2b, 0xe6, 0x42, 0x86, 0x0a, 0x2d, 0x00,
  0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

const static uint16_t gFontDataLen = 959;

const static uint8_t gFontWidths[] = {
  0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 3, 2, 4, 6, 4, 8, 7, 2, 3, 3, 4, 6, 2, 4, 2, 4,
  5, 3, 4, 4, 5, 4, 5, 4, 5, 5, 2, 2, 5, 5, 5, 4, 8, 5, 5, 4, 5, 4, 4, 5,
  5, 2, 3, 5, 4, 6, 5, 5, 5, 5, 5, 4, 4, 5, 6, 6, 6, 6, 4, 3, 4, 3, 4, 6,
  5, 5, 5, 4, 5, 5, 3, 5, 5, 2, 2, 5, 2, 8, 5, 5, 5, 5, 4, 4, 4, 5, 6, 8,
  6, 5, 4, 4, 4, 4, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 2, 2, 4, 4, 5, 5,10, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5,
  5, 8, 5, 6, 5, 4, 8, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, 0, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  3, 2, 4, 6, 4, 8, 6, 2, 3, 3, 4, 4, 2, 4, 2, 4, 5, 3, 4, 4, 5, 4, 5, 4,
  5, 5, 2, 2, 5, 5, 5, 4, 8, 5, 5, 4, 5, 4, 4, 5, 5, 2, 3, 5, 4, 6, 5, 5,
  5, 5, 5, 4, 4, 5, 6, 6, 6, 6, 4, 3, 4, 3, 4, 6, 5, 5, 5, 4, 5, 4, 4, 5,
  5, 2, 3, 5, 4, 6, 5, 5, 5, 5, 5, 4, 4, 5, 6, 6, 6, 6, 4, 4, 4, 4, 6, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 2, 4, 4, 5, 5,10,
  5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5, 8, 5, 6, 5, 4, 8, 5,
  4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
  5, 5, 5, 5, 5, 5, 5, 5
};

const static uint16_t gFontWidthsLen = 512;

static void VCDebugger_CompileAndLinkShader(VCDebugger *debugger) {
    VCRenderer_CompileShader(&debugger->program.vertexShader, GL_VERTEX_SHADER, "debug.vs.glsl");
    VCRenderer_CompileShader(&debugger->program.fragmentShader,
                             GL_FRAGMENT_SHADER,
                             "debug.fs.glsl");
    VCRenderer_CreateProgram(&debugger->program.program,
                             debugger->program.vertexShader,
                             debugger->program.fragmentShader);
    GL(glBindAttribLocation(debugger->program.program, 0, "aPosition"));
    GL(glBindAttribLocation(debugger->program.program, 1, "aColor"));
    GL(glBindAttribLocation(debugger->program.program, 2, "aTextureUv"));
    GL(glLinkProgram(debugger->program.program));
    GL(glUseProgram(debugger->program.program));
}

static float VCDebugger_ToNormalizedDeviceX(int32_t x) {
    VCConfig *config = VCConfig_SharedConfig();
    return (float)x / (float)config->displayWidth * 2.0 - 1.0;
}

static float VCDebugger_ToNormalizedDeviceY(int32_t y) {
    VCConfig *config = VCConfig_SharedConfig();
    return 1.0 - (float)y / (float)config->displayHeight * 2.0;
}

static void VCDebugger_ResetVertices(VCDebugger *debugger) {
    VCConfig *config = VCConfig_SharedConfig();

    assert(debugger->verticesCapacity >= 6);
    debugger->verticesLength = 0;

    VCDebugVertex tl, tr, br, bl;
    tl.position.y = tr.position.y =
        VCDebugger_ToNormalizedDeviceY(config->displayHeight - SCALE * VERTICAL_MARGIN * 2 -
                                       CELL_HEIGHT * SCALE * DEBUG_COUNTERS);
    bl.position.y = br.position.y = VCDebugger_ToNormalizedDeviceY(config->displayHeight);
    tl.position.x = bl.position.x = VCDebugger_ToNormalizedDeviceX(0.0);
    tr.position.x = br.position.x = VCDebugger_ToNormalizedDeviceX((HORIZONTAL_MARGIN + WINDOW_WIDTH) * SCALE);

    tl.textureUV.x = bl.textureUV.x = (float)(ATLAS_WIDTH - 1) / (float)ATLAS_WIDTH;
    tr.textureUV.x = br.textureUV.x = 1.0;
    tl.textureUV.y = tr.textureUV.y = (float)(ATLAS_HEIGHT - 1) / (float)ATLAS_HEIGHT;
    bl.textureUV.y = br.textureUV.y = 1.0;

    tl.color.r = tr.color.r = br.color.r = bl.color.r = 0.0;
    tl.color.g = tr.color.g = br.color.g = bl.color.g = 0.0;
    tl.color.b = tr.color.b = br.color.b = bl.color.b = 0.0;
    tl.color.a = tr.color.a = br.color.a = bl.color.a = 0.75;

    debugger->vertices[debugger->verticesLength++] = tl;
    debugger->vertices[debugger->verticesLength++] = tr;
    debugger->vertices[debugger->verticesLength++] = bl;
    debugger->vertices[debugger->verticesLength++] = tr;
    debugger->vertices[debugger->verticesLength++] = br;
    debugger->vertices[debugger->verticesLength++] = bl;
}

static void VCDebugger_InitStat(VCDebugStat *stat) {
    stat->sum = 0;
    for (uint32_t i = 0; i < sizeof(stat->samples) / sizeof(stat->samples[0]); i++)
        stat->samples[i] = 0;
}

void VCDebugger_Init(VCDebugger *debugger, VCRenderer *renderer) {
    VCDebugger_CompileAndLinkShader(debugger);
    GL(glGenBuffers(1, &debugger->vbo));

    debugger->verticesCapacity = INITIAL_VERTICES_CAPACITY;
    debugger->vertices = (VCDebugVertex *)
        malloc(sizeof(debugger->vertices[0]) * debugger->verticesCapacity);

    debugger->stats.sampleCount = 0;
    VCDebugger_InitStat(&debugger->stats.trianglesDrawn);
    VCDebugger_InitStat(&debugger->stats.batches);
    VCDebugger_InitStat(&debugger->stats.texturesUploaded);
    VCDebugger_InitStat(&debugger->stats.programsCreated);
    VCDebugger_InitStat(&debugger->stats.prepareTime);
    VCDebugger_InitStat(&debugger->stats.drawTime);
    VCDebugger_InitStat(&debugger->stats.viRate);

    VCDebugger_ResetVertices(debugger);

    int atlasWidth, atlasHeight, atlasBPP;
    uint8_t *pixels = stbi_load_from_memory(gFontData,
                                            gFontDataLen,
                                            &atlasWidth,
                                            &atlasHeight,
                                            &atlasBPP,
                                            4);
    assert(atlasWidth == ATLAS_WIDTH && atlasHeight == ATLAS_HEIGHT);

    GL(glGenTextures(1, &debugger->fontTexture));
    GL(glBindTexture(GL_TEXTURE_2D, debugger->fontTexture));
    GL(glTexImage2D(GL_TEXTURE_2D,
                    0,
                    GL_RGBA,
                    ATLAS_WIDTH,
                    ATLAS_HEIGHT,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    pixels));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    stbi_image_free(pixels);
}

static void VCDebugger_SetVBOState(VCDebugger *debugger) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, debugger->vbo));
    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VCDebugVertex), (const GLvoid *)0));
    GL(glVertexAttribPointer(1,
                             4,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(VCDebugVertex),
                             (const GLvoid *)offsetof(VCDebugVertex, color)));
    GL(glVertexAttribPointer(2,
                             2,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(VCDebugVertex),
                             (const GLvoid *)offsetof(VCDebugVertex, textureUV)));

    for (int i = 0; i < 3; i++)
        GL(glEnableVertexAttribArray(i));
}

static uint8_t VCDebugger_GlyphWidth(char glyph, bool useSmallFont) {
    return gFontWidths[(uint32_t)glyph + (useSmallFont ? 256 : 0)];
}

static void VCDebugger_AddText(VCDebugger *debugger,
                               VCPoint2i *position,
                               char *string,
                               size_t stringLength,
                               VCColorf *color,
                               bool useSmallFont,
                               bool rightJustify) {
    while (debugger->verticesCapacity < debugger->verticesLength + stringLength * 6) {
        debugger->verticesCapacity *= 2;
        debugger->vertices = (VCDebugVertex *)
            realloc(debugger->vertices,
                    debugger->verticesCapacity * sizeof(debugger->vertices[0]));
    }

    if (rightJustify) {
        for (uint32_t glyphIndex = 0; glyphIndex < stringLength; glyphIndex++)
            position->x -= VCDebugger_GlyphWidth(string[glyphIndex], useSmallFont) * SCALE;
    }

    for (uint32_t glyphIndex = 0; glyphIndex < stringLength; glyphIndex++) {
        uint8_t width = VCDebugger_GlyphWidth(string[glyphIndex], useSmallFont);
        uint32_t glyph = string[glyphIndex] - 32;
        if (glyph > GLYPHS_PER_FONT)
            glyph = 0;
        uint32_t glyphRow = glyph / ATLAS_WIDTH_IN_CELLS;
        uint32_t glyphColumn = glyph % ATLAS_WIDTH_IN_CELLS;
        uint32_t fontAtlasOffset = useSmallFont ? 128 : 0;

        VCDebugVertex tl, tr, br, bl;
        tl.position.y = tr.position.y = VCDebugger_ToNormalizedDeviceY(position->y);
        bl.position.y = br.position.y =
            VCDebugger_ToNormalizedDeviceY(position->y + CELL_HEIGHT * SCALE);
        tl.position.x = bl.position.x = VCDebugger_ToNormalizedDeviceX(position->x);
        tr.position.x = br.position.x =
            VCDebugger_ToNormalizedDeviceX(position->x + CELL_WIDTH * SCALE);

        tl.textureUV.x = bl.textureUV.x = (float)(glyphColumn * CELL_WIDTH) / (float)ATLAS_WIDTH;
        tr.textureUV.x = br.textureUV.x = (float)((glyphColumn + 1) * CELL_WIDTH) /
            (float)ATLAS_WIDTH;
        tl.textureUV.y = tr.textureUV.y = (float)(glyphRow * CELL_HEIGHT + fontAtlasOffset) /
            (float)ATLAS_HEIGHT;
        bl.textureUV.y = br.textureUV.y = (float)((glyphRow + 1) * CELL_HEIGHT + fontAtlasOffset) /
            (float)ATLAS_HEIGHT;

        tl.color = tr.color = br.color = bl.color = *color;

        debugger->vertices[debugger->verticesLength++] = tl;
        debugger->vertices[debugger->verticesLength++] = tr;
        debugger->vertices[debugger->verticesLength++] = bl;
        debugger->vertices[debugger->verticesLength++] = tr;
        debugger->vertices[debugger->verticesLength++] = br;
        debugger->vertices[debugger->verticesLength++] = bl;

        position->x += width * SCALE;
    }
}

static void VCDebugger_DrawVertices(VCDebugger *debugger) {
    GL(glUseProgram(debugger->program.program));
    VCDebugger_SetVBOState(debugger);
    GL(glBindBuffer(GL_ARRAY_BUFFER, debugger->vbo));
    GL(glBufferData(GL_ARRAY_BUFFER,
                    sizeof(VCDebugVertex) * debugger->verticesLength,
                    debugger->vertices,
                    GL_DYNAMIC_DRAW));
    GL(glBindTexture(GL_TEXTURE_2D, debugger->fontTexture));
    GL(glDrawArrays(GL_TRIANGLES, 0, debugger->verticesLength));

    VCDebugger_ResetVertices(debugger);
}

static void VCDebugger_DrawDebugStat(VCDebugger *debugger,
                                     const char *statName,
                                     uint32_t statValue,
                                     int32_t warningThreshold,
                                     int32_t errorThreshold,
                                     VCPoint2i *position) {
    uint32_t oldXPosition = position->x;
    char string[256];
    position->y -= CELL_HEIGHT * SCALE;
    snprintf(string, sizeof(string), "%d ", (int)statValue);

    VCColorf textColor;
    if ((errorThreshold > 0 && (int32_t)statValue >= errorThreshold) ||
            (errorThreshold < 0 && (int32_t) statValue <= -errorThreshold)) {
        textColor.r = textColor.a = 1.0;
        textColor.g = textColor.b = 0.0;
    } else if ((warningThreshold > 0 && (int32_t)statValue >= warningThreshold) ||
            (warningThreshold < 0 && (int32_t) statValue <= -warningThreshold)) {
        textColor.r = textColor.g = textColor.a = 1.0;
        textColor.b = 0.0;
    } else {
        textColor.r = textColor.g = textColor.b = textColor.a = 1.0;
    }

    VCDebugger_AddText(debugger, position, string, strlen(string), &textColor, false, true);
    position->x = oldXPosition;
    snprintf(string, sizeof(string), "%s", statName);
    VCColorf labelColor = { 0.5, 0.5, 0.5, 1.0 };
    VCDebugger_AddText(debugger, position, string, strlen(string), &labelColor, true, false);
    position->x = oldXPosition;
}

static uint32_t VCDebugger_MovingAverageOfStat(VCDebugger *debugger, VCDebugStat *stat) {
    uint32_t sampleCount;
    if (debugger->stats.sampleCount > VC_SAMPLES_IN_WINDOW)
        sampleCount = VC_SAMPLES_IN_WINDOW;
    else
        sampleCount = debugger->stats.sampleCount;
    return (sampleCount == 0) ? 0 : (stat->sum / sampleCount);
}

static uint32_t VCDebugger_MovingAverageOfVIPerSecond(VCDebugger *debugger) {
    uint32_t latestIndex = debugger->stats.sampleCount % VC_SAMPLES_IN_WINDOW;
    uint32_t earliestIndex;
    if (debugger->stats.sampleCount >= VC_SAMPLES_IN_WINDOW)
        earliestIndex = (latestIndex + 1) % VC_SAMPLES_IN_WINDOW;
    else
        earliestIndex = 0;
    uint32_t elapsed = debugger->stats.viRate.samples[latestIndex] -
        debugger->stats.viRate.samples[earliestIndex];
    if (elapsed == 0)
        return 30;  // Lies...

    uint32_t samples;
    if (debugger->stats.sampleCount > VC_SAMPLES_IN_WINDOW)
        samples = VC_SAMPLES_IN_WINDOW;
    else
        samples = debugger->stats.sampleCount;
    return samples * 1000 / elapsed;
}

void VCDebugger_DrawDebugOverlay(VCDebugger *debugger, const VCSize2u *windowSize) {
    GL(glViewport(0, 0, windowSize->width, windowSize->height));

    VCConfig *config = VCConfig_SharedConfig();
    VCPoint2i position = {
        (HORIZONTAL_MARGIN + TAB_STOP) * SCALE,
        config->displayHeight - VERTICAL_MARGIN * SCALE
    };
    VCDebugger_DrawDebugStat(debugger,
                             "VI/s",
                             VCDebugger_MovingAverageOfVIPerSecond(debugger),
                             -27,
                             -24,
                             &position);
    VCDebugger_DrawDebugStat(debugger,
                             "ms render",
                             VCDebugger_MovingAverageOfStat(debugger, &debugger->stats.drawTime),
                             12,
                             16,
                             &position);
    VCDebugger_DrawDebugStat(debugger,
                             "ms RSP",
                             VCDebugger_MovingAverageOfStat(debugger,
                                                            &debugger->stats.prepareTime),
                             12,
                             16,
                             &position);
    VCDebugger_DrawDebugStat(debugger,
                             "new textures",
                             VCDebugger_MovingAverageOfStat(debugger,
                                                            &debugger->stats.texturesUploaded),
                             7,
                             15,
                             &position);
    VCDebugger_DrawDebugStat(debugger,
                             "new shaders",
                             VCDebugger_MovingAverageOfStat(debugger,
                                                            &debugger->stats.programsCreated),
                             3,
                             5,
                             &position);
    VCDebugger_DrawDebugStat(debugger,
                             "draw calls",
                             VCDebugger_MovingAverageOfStat(debugger,
                                                            &debugger->stats.batches),
                             10,
                             20,
                             &position);
    VCDebugger_DrawDebugStat(debugger,
                             "triangles",
                             VCDebugger_MovingAverageOfStat(debugger,
                                                            &debugger->stats.trianglesDrawn),
                             3000,
                             4000,
                             &position);
    VCDebugger_DrawVertices(debugger);
}

static void VCDebugger_AdvanceSampleWindow(VCDebugger *debugger, VCDebugStat *stat) {
    stat->sum -= stat->samples[debugger->stats.sampleCount % VC_SAMPLES_IN_WINDOW];
    stat->samples[debugger->stats.sampleCount % VC_SAMPLES_IN_WINDOW] = 0;
}

void VCDebugger_NewFrame(VCDebugger *debugger) {
    debugger->stats.sampleCount++;
    if (debugger->stats.sampleCount > VC_SAMPLES_IN_WINDOW) {
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.trianglesDrawn);
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.batches);
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.texturesUploaded);
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.programsCreated);
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.prepareTime);
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.drawTime);
        VCDebugger_AdvanceSampleWindow(debugger, &debugger->stats.viRate);
    }
}

void VCDebugger_AddSample(VCDebugger *debugger, VCDebugStat *stat, uint32_t newSample) {
    stat->samples[debugger->stats.sampleCount % VC_SAMPLES_IN_WINDOW] = newSample;
    stat->sum += newSample;
}

void VCDebugger_IncrementSample(VCDebugger *debugger, VCDebugStat *stat) {
    stat->samples[debugger->stats.sampleCount % VC_SAMPLES_IN_WINDOW]++;
    stat->sum++;
}

