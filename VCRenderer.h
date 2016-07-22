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

#define VC_BLEND_MODE_DISABLED                                  0
#define VC_BLEND_MODE_SRC_ONE_DEST_ONE                          1
#define VC_BLEND_MODE_SRC_ONE_DEST_ZERO                         2
#define VC_BLEND_MODE_SRC_SRC_ALPHA_DEST_ONE_MINUS_SRC_ALPHA    3
#define VC_BLEND_MODE_SRC_ZERO_DEST_ONE                         4

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
    uint8_t blendMode;
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

    bool commandsQueued;
    SDL_mutex *commandsMutex;
    SDL_cond *commandsCond;

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
                          uint8_t mode,
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
uint8_t VCRenderer_GetCurrentBlendMode(uint8_t triangleMode);
void VCRenderer_BeginNewFrame(VCRenderer *renderer);
void VCRenderer_EndFrame(VCRenderer *renderer);
void VCRenderer_PopulateTextureBoundsInBatches(VCRenderer *renderer);
void VCRenderer_SendBatchesToRenderThread(VCRenderer *renderer, uint32_t elapsedTime);
bool VCRenderer_ShouldCull(VCN64Vertex *triangleVertices, bool cullFront, bool cullBack);
void VCRenderer_AllocateTexturesAndEnqueueTextureUploadCommands(VCRenderer *renderer);

#endif

