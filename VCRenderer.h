// mupen64plus-video-videocore/VCRenderer.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VCRENDERER_H
#define VCRENDERER_H

#define VC_N64_WIDTH 320
#define VC_N64_HEIGHT 240

#define VC_RENDER_COMMAND_NONE                      0
#define VC_RENDER_COMMAND_UPLOAD_TEXTURE            1
#define VC_RENDER_COMMAND_DRAW_BATCHES              2
#define VC_RENDER_COMMAND_COMPILE_SHADER_PROGRAM    3
#define VC_RENDER_COMMAND_DESTROY_SHADER_PROGRAM    4

#define VC_TRIANGLE_MODE_NORMAL             0
#define VC_TRIANGLE_MODE_TEXTURE_RECTANGLE  1
#define VC_TRIANGLE_MODE_RECT_FILL          2

#define VC_GLOBAL_BLEND_MODE_NORMAL     0
#define VC_GLOBAL_BLEND_MODE_ADD        1

#define VC_SRC_BLEND_MODE_ONE           0
#define VC_SRC_BLEND_MODE_ZERO          1
#define VC_SRC_BLEND_MODE_ALPHA         2

#define VC_INVALID_SUBPROGRAM_ID        ((uint32_t)~0)

#include <SDL2/SDL.h>
#include <stdint.h>
#include "VCAtlas.h"
#include "VCDebugger.h"
#include "VCGeometry.h"
#include "VCShaderCompiler.h"

struct Combiner;
struct SPVertex;
struct VCShaderProgram;
struct VCShaderProgramDescriptorLibrary;

struct VCBlendFlags {
    bool zTest;
    bool zUpdate;
    uint8_t globalBlendMode;
    VCRectf viewport;
};

union VCN64VertexTextureRef {
    VCCachedTexture *cachedTexture;
    VCRects textureBounds;
};

struct VCN64Vertex {
    VCPoint4f position;
    VCPoint2f textureUV;
    VCN64VertexTextureRef texture0;
    VCN64VertexTextureRef texture1;
    VCColor shade;
    VCColor primitive;
    VCColor environment;
    uint8_t subprogram;
    uint8_t alphaThreshold;
    uint8_t sourceBlendMode;
};

struct VCBlitVertex {
    VCPoint2f position;
    VCPoint2f textureUV;
};

struct VCBatch {
    VCN64Vertex *vertices;
    size_t verticesLength;
    size_t verticesCapacity;
    VCBlendFlags blendFlags;
    union {
        VCShaderSubprogramSignatureTable table;
        uint32_t id;
    } program;
    bool programIDPresent;
};

struct VCRenderCommand {
    uint8_t command;
    VCRectus uv;
    uint8_t *pixels;
    uint32_t elapsedTime;
    uint32_t shaderProgramID;
    VCShaderProgram *shaderProgram;
    VCBatch *batches;
    size_t batchesLength;
};

struct VCCompiledShaderProgram {
    VCProgram program;
};

struct VCRenderer {
    SDL_Window *window;
    SDL_GLContext context;

    bool ready;
    SDL_mutex *readyMutex;
    SDL_cond *readyCond;

    VCSize2u windowSize;

    VCRenderCommand *commands;
    size_t commandsLength;
    size_t commandsCapacity;

    // For RSP thread use only.
    uint32_t commandsSubmitted;

    uint32_t commandsQueued;
    SDL_mutex *commandsQueuedMutex;
    SDL_cond *commandsQueuedCond;
    uint32_t commandsDequeued;
    SDL_mutex *commandsDequeuedMutex;
    SDL_cond *commandsDequeuedCond;

    // For use by RSP thread only. The render thread may not touch these!
    VCBatch *batches;
    size_t batchesLength;
    size_t batchesCapacity;

    // For RSP thread only.
    VCAtlas atlas;
    VCShaderSubprogramLibrary *shaderSubprogramLibrary;
    VCShaderProgramDescriptorLibrary *shaderProgramDescriptorLibrary;
    VCDebugger *debugger;

    // For RSP thread only.
    uint32_t currentEpoch;

    // For RSP thread only.
    uint32_t currentSubprogramID;
    uint8_t triangleModeForCachedSubprogramID;

    VCProgram blitProgram;
    GLuint quadVBO;

    VCCompiledShaderProgram *shaderPrograms;
    size_t shaderProgramsLength;
    size_t shaderProgramsCapacity;
    char *shaderPreamble;
    GLuint n64VBO;

    GLuint fbo;
    GLuint fboTexture;
    GLuint depthRenderbuffer;
};

VCRenderer *VCRenderer_SharedRenderer();
void VCRenderer_Start(VCRenderer *renderer);
void VCRenderer_CreateProgram(GLuint *program, GLuint vertexShader, GLuint fragmentShader);
void VCRenderer_CompileShader(GLuint *shader, GLint shaderType, const char *path);
void VCRenderer_AddVertex(VCRenderer *renderer,
                          VCN64Vertex *vertex,
                          VCBlendFlags *blendFlags,
                          uint8_t triangleMode,
                          float alphaThreshold);
void VCRenderer_EnqueueCommand(VCRenderer *renderer, VCRenderCommand *command);
void VCRenderer_SubmitCommands(VCRenderer *renderer);
void VCRenderer_InitTriangleVertices(VCRenderer *renderer,
                                     VCN64Vertex *n64Vertices,
                                     SPVertex *spVertices,
                                     uint32_t *indices,
                                     uint32_t indexCount,
                                     uint8_t mode);
void VCRenderer_CreateNewShaderProgramsIfNecessary(VCRenderer *renderer);
uint8_t VCRenderer_GetCurrentGlobalBlendMode(uint8_t triangleMode);
void VCRenderer_BeginNewFrame(VCRenderer *renderer);
void VCRenderer_EndFrame(VCRenderer *renderer);
void VCRenderer_PopulateTextureBoundsInBatches(VCRenderer *renderer);
void VCRenderer_SendBatchesToRenderThread(VCRenderer *renderer, uint32_t elapsedTime);
bool VCRenderer_ShouldCull(SPVertex *va,
                           SPVertex *vb,
                           SPVertex *vc,
                           bool cullFront,
                           bool cullBack);
void VCRenderer_AllocateTexturesAndEnqueueTextureUploadCommands(VCRenderer *renderer);
void VCRenderer_InvalidateCachedSubprogramID(VCRenderer *renderer);

#endif

