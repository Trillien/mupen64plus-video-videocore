// mupen64plus-video-videocore/VCRenderer.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <SDL2/SDL.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "VCCombiner.h"
#include "VCConfig.h"
#include "VCGL.h"
#include "VCGeometry.h"
#include "VCRenderer.h"
#include "VCShaderCompiler.h"
#include "VCUtils.h"
#include "gSP.h"
#include "stb_image_write.h"

#define INITIAL_BATCHES_CAPACITY 8
#define INITIAL_N64_VERTEX_STORAGE_CAPACITY 1024

// FIXME: This is pretty ugly.
static VCRenderer SharedRenderer;

static void VCRenderer_Init(VCRenderer *renderer, SDL_Window *window, SDL_GLContext context);
static void VCRenderer_Draw(VCRenderer *renderer, VCBatch *batches, size_t batchesLength);
static void VCRenderer_Present(VCRenderer *renderer);

static char *VCRenderer_Slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    struct stat st;
    if (fstat(fileno(f), &st) < 0) {
        perror("Failed to stat shader");
        abort();
    }
    char *buffer = (char *)malloc(st.st_size + 1);
    if (!buffer)
        abort();
    if (fread(buffer, st.st_size, 1, f) < 1) {
        perror("Failed to read shader");
        abort();
    }
    buffer[st.st_size] = '\0';
    fclose(f);
    return buffer;
}

void VCRenderer_CompileShaderFromCString(GLuint *shader, GLint shaderType, const char *source) {
    *shader = glCreateShader(shaderType);
    GL(glShaderSource(*shader, 1, &source, NULL));
    GL(glCompileShader(*shader));
#ifdef VCDEBUG
    GLint compileStatus = 0;
    GL(glGetShaderiv(*shader, GL_COMPILE_STATUS, &compileStatus));
    if (compileStatus != GL_TRUE) {
        fprintf(stderr, "Failed to compile shader!\n");
        GLint infoLogLength = 0;
        GL(glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &infoLogLength));
        char *infoLog = (char *)malloc(infoLogLength + 1);
        GL(glGetShaderInfoLog(*shader, (GLsizei)infoLogLength, NULL, infoLog));
        infoLog[infoLogLength] = '\0';
        fprintf(stderr, "%s\n", infoLog);
        abort();
    }
#endif
}

// Result is null-terminated. Caller is responsible for freeing it. Exits app on failure.
static char *VCRenderer_SlurpShaderSource(const char *filename) {
    char *path = (char *)malloc(PATH_MAX + 1);
    char *dataHome = getenv("XDG_DATA_HOME");
    if (dataHome == NULL || dataHome[0] == '\0') {
        const char *home = getenv("HOME");
        if (home == NULL)
            home = ".";
        snprintf(path, PATH_MAX, "%s/.local/share/mupen64plus/videocore/%s", home, filename);
    } else {
        snprintf(path, PATH_MAX, "%s/mupen64plus/videocore/%s", dataHome, filename);
    }

    char *source = VCRenderer_Slurp(path);
    if (source != NULL)
        return source;

    const char *dataDirsLocation = getenv("XDG_DATA_DIRS");
    if (dataDirsLocation == NULL || dataDirsLocation[0] == '\0')
        dataDirsLocation = "/usr/local/share/:/usr/share/";
    char *dataDirs = strdup(dataDirsLocation);
    char *dataDir;
    while ((dataDir = xstrsep(&dataDirs, ":")) != NULL) {
        snprintf(path, PATH_MAX, "%s/mupen64plus/videocore/%s", dataDir, filename);
        source = VCRenderer_Slurp(path);
        if (source != NULL)
            return source;
    }

    snprintf(path, PATH_MAX, "./%s", filename);
    if ((source = VCRenderer_Slurp(path)) != NULL)
        return source;

    fprintf(stderr,
            "video error: couldn't find shader `%s`: try installing it to "
            "`~/.local/share/mupen64plus/videocore/%s` or "
            "`/usr/local/share/mupen64plus/videocore/%s`\n",
            filename,
            filename,
            filename);
    exit(1);
}

void VCRenderer_CompileShader(GLuint *shader, GLint shaderType, const char *filename) {
    char *source = VCRenderer_SlurpShaderSource(filename);
    VCRenderer_CompileShaderFromCString(shader, shaderType, source);
}

void VCRenderer_CreateProgram(GLuint *program, GLuint vertexShader, GLuint fragmentShader) {
    *program = glCreateProgram();
    GL(glAttachShader(*program, vertexShader));
    GL(glAttachShader(*program, fragmentShader));
}

void VCRenderer_DestroyProgram(VCProgram *program) {
    GL(glDeleteProgram(program->program));
    GL(glDeleteShader(program->vertexShader));
    GL(glDeleteShader(program->fragmentShader));
    program->program = 0;
    program->vertexShader = 0;
    program->fragmentShader = 0;
}

static void VCRenderer_CompileAndLinkShaders(VCRenderer *renderer) {
    VCRenderer_CompileShader(&renderer->blitProgram.vertexShader,
                             GL_VERTEX_SHADER,
                             "blit.vs.glsl");
    VCRenderer_CompileShader(&renderer->blitProgram.fragmentShader,
                             GL_FRAGMENT_SHADER,
                             "blit.fs.glsl");
    VCRenderer_CreateProgram(&renderer->blitProgram.program,
                             renderer->blitProgram.vertexShader,
                             renderer->blitProgram.fragmentShader);

    GL(glBindAttribLocation(renderer->blitProgram.program, 0, "aPosition"));
    GL(glBindAttribLocation(renderer->blitProgram.program, 1, "aTextureUv"));
    GL(glLinkProgram(renderer->blitProgram.program));
    GL(glUseProgram(renderer->blitProgram.program));

    renderer->shaderPreamble = VCRenderer_SlurpShaderSource("n64.inc.fs.glsl");
}

static void VCRenderer_CreateVBOs(VCRenderer *renderer) {
    GL(glGenBuffers(1, &renderer->n64VBO));
    GL(glGenBuffers(1, &renderer->quadVBO));
}

static void VCRenderer_SetVBOStateForN64Program(VCRenderer *renderer) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, renderer->n64VBO));
    GL(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VCN64Vertex), (const GLvoid *)0));
    GL(glVertexAttribPointer(1,
                             2,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, textureUV)));
    GL(glVertexAttribPointer(2,
                             4,
                             GL_SHORT,
                             GL_FALSE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, texture0)));
    GL(glVertexAttribPointer(3,
                             4,
                             GL_SHORT,
                             GL_FALSE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, texture1)));
    GL(glVertexAttribPointer(4,
                             4,
                             GL_UNSIGNED_BYTE,
                             GL_TRUE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, shade)));
    GL(glVertexAttribPointer(5,
                             4,
                             GL_UNSIGNED_BYTE,
                             GL_TRUE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, primitive)));
    GL(glVertexAttribPointer(6,
                             4,
                             GL_UNSIGNED_BYTE,
                             GL_TRUE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, environment)));
    GL(glVertexAttribPointer(7,
                             3,
                             GL_UNSIGNED_BYTE,
                             GL_FALSE,
                             sizeof(VCN64Vertex),
                             (const GLvoid *)offsetof(VCN64Vertex, subprogram)));
    for (int i = 0; i < 8; i++)
        GL(glEnableVertexAttribArray(i));
}

static void VCRenderer_SetVBOStateForBlitProgram(VCRenderer *renderer) {
    GL(glUseProgram(renderer->blitProgram.program));
    GL(glBindBuffer(GL_ARRAY_BUFFER, renderer->quadVBO));
    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VCBlitVertex), (const GLvoid *)0));
    GL(glVertexAttribPointer(1,
                             2,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(VCBlitVertex),
                             (const GLvoid *)offsetof(VCBlitVertex, textureUV)));

    for (int i = 0; i < 2; i++)
        GL(glEnableVertexAttribArray(i));
}

static void VCRenderer_CreateFBO(VCRenderer *renderer) {
    GL(glGenFramebuffers(1, &renderer->fbo));
    GL(glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo));

    GL(glGenTextures(1, &renderer->fboTexture));
    GL(glBindTexture(GL_TEXTURE_2D, renderer->fboTexture));
    GL(glTexImage2D(GL_TEXTURE_2D,
                    0,
                    GL_RGB,
                    VC_N64_WIDTH,
                    VC_N64_HEIGHT,
                    0,
                    GL_RGB,
                    GL_UNSIGNED_BYTE,
                    NULL));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    GL(glGenRenderbuffers(1, &renderer->depthRenderbuffer));
    GL(glBindRenderbuffer(GL_RENDERBUFFER, renderer->depthRenderbuffer));
    GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, VC_N64_WIDTH, VC_N64_HEIGHT));
    GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                 GL_DEPTH_ATTACHMENT,
                                 GL_RENDERBUFFER,
                                 renderer->depthRenderbuffer));

    GL(glFramebufferTexture2D(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              renderer->fboTexture,
                              0));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer incomplete!\n");
        abort();
    }
}

static void VCRenderer_UploadBlitVertices(VCRenderer *renderer) {
    VCRenderer_SetVBOStateForBlitProgram(renderer);
    VCBlitVertex vertices[4] = {
        { { -1.0, -1.0 }, { 0.0, 0.0 } },
        { {  1.0, -1.0 }, { 1.0, 0.0 } },
        { { -1.0,  1.0 }, { 0.0, 1.0 } },
        { {  1.0,  1.0 }, { 1.0, 1.0 } }
    };
    GL(glBindBuffer(GL_ARRAY_BUFFER, renderer->quadVBO));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(VCBlitVertex) * 4, vertices, GL_STATIC_DRAW));
}

static void VCRenderer_AddNewBatch(VCRenderer *renderer, VCBlendFlags *blendFlags) {
    if (renderer->batchesLength >= renderer->batchesCapacity) {
        renderer->batchesCapacity *= 2;
        renderer->batches =
            (VCBatch *)realloc(renderer->batches,
                               sizeof(renderer->batches[0]) * renderer->batchesCapacity);
    }
    VCBatch *batch = &renderer->batches[renderer->batchesLength];
    renderer->batchesLength++;

    batch->vertices = (VCN64Vertex *)malloc(sizeof(VCN64Vertex) *
                                            INITIAL_N64_VERTEX_STORAGE_CAPACITY);
    batch->verticesLength = 0;
    batch->verticesCapacity = INITIAL_N64_VERTEX_STORAGE_CAPACITY;
    batch->blendFlags = *blendFlags;
    batch->program.table = VCShaderCompiler_CreateSubprogramSignatureTable();
    batch->programIDPresent = false;
}

static bool VCRenderer_CheckBlendFlagEquality(bool test, const char *name) {
#if 0
    if (!test)
        fprintf(stderr, "batch break: %s\n", name);
#endif
    return test;
}

static bool VCRenderer_CanAddToCurrentBatch(VCRenderer *renderer, VCBlendFlags *blendFlags) {
    if (renderer->batchesLength == 0)
        return false;
    VCBatch *batch = &renderer->batches[renderer->batchesLength - 1];
    return VCRenderer_CheckBlendFlagEquality(batch->blendFlags.zTest == blendFlags->zTest,
                                             "zTest") &&
        VCRenderer_CheckBlendFlagEquality(batch->blendFlags.zUpdate == blendFlags->zUpdate,
                                          "zUpdate") &&
        VCRenderer_CheckBlendFlagEquality(batch->blendFlags.globalBlendMode ==
                                          blendFlags->globalBlendMode,
                                          "globalBlendMode") &&
        VCRenderer_CheckBlendFlagEquality(batch->blendFlags.viewport.origin.x ==
                                          blendFlags->viewport.origin.x,
                                          "viewport.origin.x") &&
        VCRenderer_CheckBlendFlagEquality(batch->blendFlags.viewport.origin.y ==
                                          blendFlags->viewport.origin.y,
                                          "viewport.origin.y") &&
        VCRenderer_CheckBlendFlagEquality(batch->blendFlags.viewport.size.width ==
                                          blendFlags->viewport.size.width,
                                          "viewport.size.width") &&
        VCRenderer_CheckBlendFlagEquality(batch->blendFlags.viewport.size.height ==
                                          blendFlags->viewport.size.height,
                                          "viewport.size.height");
}

static uint8_t VCRenderer_GetCurrentSourceBlendMode(uint8_t triangleMode) {
    if (triangleMode == VC_TRIANGLE_MODE_TEXTURE_RECTANGLE)
        return VC_SRC_BLEND_MODE_ALPHA;

    if (gDP.otherMode.cycleType == G_CYC_FILL)
        return VC_SRC_BLEND_MODE_ALPHA;

    if (gDP.otherMode.forceBlender &&
        gDP.otherMode.cycleType != G_CYC_COPY &&
        !gDP.otherMode.alphaCvgSel) {
        switch (gDP.otherMode.l >> 16) {
            case 0x0448: // Add
            case 0x055A:
                return VC_SRC_BLEND_MODE_ALPHA;
            case 0x0C08: // 1080 Sky
            case 0x0F0A: // Used LOTS of places
                return VC_SRC_BLEND_MODE_ONE;
            case 0xC810: // Blends fog
            case 0xC811: // Blends fog
            case 0x0C18: // Standard interpolated blend
            case 0x0C19: // Used for antialiasing
            case 0x0050: // Standard interpolated blend
            case 0x0055: // Used for antialiasing
                return VC_SRC_BLEND_MODE_ALPHA;
            case 0x0FA5: // Seems to be doing just blend color - maybe combiner can be used for this?
            case 0x5055: // Used in Paper Mario intro, I'm not sure if this is right...
                return VC_SRC_BLEND_MODE_ZERO;
            default:
                return VC_SRC_BLEND_MODE_ALPHA;
        }
    }

    return VC_SRC_BLEND_MODE_ONE;
}

void VCRenderer_AddVertex(VCRenderer *renderer,
                          VCN64Vertex *vertex,
                          VCBlendFlags *blendFlags,
                          uint8_t triangleMode,
                          float alphaThreshold) {
    if (!VCRenderer_CanAddToCurrentBatch(renderer, blendFlags))
        VCRenderer_AddNewBatch(renderer, blendFlags);

    VCBatch *batch = &renderer->batches[renderer->batchesLength - 1];

    if (renderer->currentSubprogramID == VC_INVALID_SUBPROGRAM_ID ||
            triangleMode != renderer->triangleModeForCachedSubprogramID) {
        VCColor envColor = { gDP.envColor.r, gDP.envColor.g, gDP.envColor.b, gDP.envColor.a };
        VCColor primColor = { gDP.primColor.r, gDP.primColor.g, gDP.primColor.b, gDP.primColor.a };
        VCShaderSubprogramContext subprogramContext =
            VCShaderCompiler_CreateSubprogramContext(primColor,
                                                     envColor,
                                                     gDP.otherMode.cycleType == G_CYC_2CYCLE,
                                                     triangleMode);
        VCShaderSubprogramSignature subprogramSignature =
            VCShaderCompiler_GetOrCreateSubprogramSignatureForCurrentCombiner(
                    renderer->shaderSubprogramLibrary,
                    &subprogramContext);
        assert(!batch->programIDPresent);

        renderer->currentSubprogramID =
            VCShaderCompiler_GetOrCreateSubprogramID(&batch->program.table, &subprogramSignature);
        renderer->triangleModeForCachedSubprogramID = triangleMode;
    }
    vertex->subprogram = renderer->currentSubprogramID;

    vertex->alphaThreshold = (uint8_t)roundf(alphaThreshold * 255.0);
    vertex->sourceBlendMode = VCRenderer_GetCurrentSourceBlendMode(triangleMode);

    if (batch->verticesLength >= batch->verticesCapacity) {
        batch->verticesCapacity *= 2;
        batch->vertices =
            (VCN64Vertex *)realloc(batch->vertices,
                                   sizeof(batch->vertices[0]) * batch->verticesCapacity);
        if (batch->vertices == NULL)
            abort();
    }

    batch->vertices[batch->verticesLength] = *vertex;
    batch->verticesLength++;
}

static void VCRenderer_SetUniforms(VCRenderer *renderer) {
    GL(glUseProgram(renderer->blitProgram.program));
    GLuint uTexture = glGetUniformLocation(renderer->blitProgram.program, "uTexture");
    GL(glUniform1i(uTexture, 0));
}

VCRenderer *VCRenderer_SharedRenderer() {
    return &SharedRenderer;
}

static void VCRenderer_CompileShaderProgram(VCRenderer *renderer,
                                            VCShaderProgram *shaderProgram,
                                            uint32_t shaderProgramID) {
    if (renderer->shaderProgramsCapacity < shaderProgramID + 1) {
        renderer->shaderPrograms = (VCCompiledShaderProgram *)
            realloc(renderer->shaderPrograms,
                    sizeof(VCCompiledShaderProgram) * (shaderProgramID + 1));
        renderer->shaderProgramsCapacity = shaderProgramID + 1;
    }

    for (size_t i = renderer->shaderProgramsLength; i < shaderProgramID + 1; i++) {
        renderer->shaderPrograms[i].program.vertexShader = 0;
        renderer->shaderPrograms[i].program.fragmentShader = 0;
        renderer->shaderPrograms[i].program.program = 0;
    }

    VCCompiledShaderProgram *program = &renderer->shaderPrograms[shaderProgramID];
    renderer->shaderProgramsLength = shaderProgramID + 1;

    VCRenderer_CompileShader(&program->program.vertexShader, GL_VERTEX_SHADER, "n64.vs.glsl");

    VCString fragmentShaderSource = VCString_Create();
    VCString_AppendCString(&fragmentShaderSource, renderer->shaderPreamble);
    VCShaderCompiler_GenerateGLSLFragmentShaderForProgram(&fragmentShaderSource, shaderProgram);
    //printf("New program:\n%s\n// end\n", fragmentShaderSource.ptr);
    VCRenderer_CompileShaderFromCString(&program->program.fragmentShader,
                                        GL_FRAGMENT_SHADER,
                                        fragmentShaderSource.ptr);
    VCString_Destroy(&fragmentShaderSource);

    VCRenderer_CreateProgram(&program->program.program,
                             program->program.vertexShader,
                             program->program.fragmentShader);
    GL(glBindAttribLocation(program->program.program, 0, "aPosition"));
    GL(glBindAttribLocation(program->program.program, 1, "aTextureUv"));
    GL(glBindAttribLocation(program->program.program, 2, "aTexture0Bounds"));
    GL(glBindAttribLocation(program->program.program, 3, "aTexture1Bounds"));
    GL(glBindAttribLocation(program->program.program, 4, "aShade"));
    GL(glBindAttribLocation(program->program.program, 5, "aPrimitive"));
    GL(glBindAttribLocation(program->program.program, 6, "aEnvironment"));
    GL(glBindAttribLocation(program->program.program, 7, "aControl"));
    GL(glLinkProgram(program->program.program));
    GL(glUseProgram(program->program.program));

    GLint uTexture = glGetUniformLocation(program->program.program, "uTexture");
    GL(glUniform1i(uTexture, 0));

    VCDebugger_IncrementSample(renderer->debugger, &renderer->debugger->stats.programsCreated);
}

static void VCRenderer_DestroyShaderProgram(VCRenderer *renderer, uint32_t shaderProgramID) {
    assert(shaderProgramID < renderer->shaderProgramsLength);
    assert(shaderProgramID < renderer->shaderProgramsCapacity);
    VCCompiledShaderProgram *program = &renderer->shaderPrograms[shaderProgramID];
    VCRenderer_DestroyProgram(&program->program);
}

#ifdef VC_TEXTURE_SPEW
static void VCRenderer_DumpAtlas(VCRenderer *renderer) {
    static int outputIndex = 0;
    char path[256];
    snprintf(path, 256, "texture%03d.png", (int)outputIndex);
    char *pixels = (char *)malloc(4 * VC_ATLAS_TEXTURE_SIZE * VC_ATLAS_TEXTURE_SIZE);
    VCAtlas_Bind(&renderer->atlas);
    GL(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
    char *tmp = (char *)malloc(4 * VC_ATLAS_TEXTURE_SIZE);
    for (unsigned i = 0; i < VC_ATLAS_TEXTURE_SIZE / 2; i++) {
        memcpy(tmp, &pixels[4 * i * VC_ATLAS_TEXTURE_SIZE], VC_ATLAS_TEXTURE_SIZE * 4);
        memcpy(&pixels[4 * i * VC_ATLAS_TEXTURE_SIZE],
               &pixels[4 * (VC_ATLAS_TEXTURE_SIZE - i - 1) * VC_ATLAS_TEXTURE_SIZE],
               VC_ATLAS_TEXTURE_SIZE * 4);
        memcpy(&pixels[4 * (VC_ATLAS_TEXTURE_SIZE - i - 1) * VC_ATLAS_TEXTURE_SIZE],
               tmp,
               VC_ATLAS_TEXTURE_SIZE * 4);
    }
    free(tmp);
    stbi_write_png(path,
                   VC_ATLAS_TEXTURE_SIZE,
                   VC_ATLAS_TEXTURE_SIZE,
                   4,
                   pixels,
                   VC_ATLAS_TEXTURE_SIZE * 4);
    free(pixels);
    fprintf(stderr, "wrote %s\n", path);
    outputIndex++;
}
#endif

static int VCRenderer_ThreadMain(void *userData) {
    VCRenderer *renderer = (VCRenderer *)userData;

    int flags = SDL_WINDOW_OPENGL;
#ifdef HAVE_OPENGLES2
    flags |= SDL_WINDOW_FULLSCREEN;
#endif

    SDL_Window *screen = SDL_CreateWindow("mupen64plus-video-videocore - Mupen64Plus",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          renderer->windowSize.width,
                                          renderer->windowSize.height,
                                          flags);
    SDL_GLContext context = SDL_GL_CreateContext(screen);

    SDL_ShowCursor(SDL_DISABLE);

#if !defined(HAVE_OPENGLES2) && !defined(__APPLE__)
    int glewError = glewInit();
    if (glewError != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW: %d!\n", (int)glewError);
        abort();
    }
#endif

    VCRenderer_Init(renderer, screen, context);

    uint32_t commandsProcessed = 0;
    while (1) {
        SDL_LockMutex(renderer->commandsQueuedMutex);
        while (commandsProcessed == renderer->commandsQueued) {
#if 0
            fprintf(stderr,
                    "render thread waiting, commandsQueued=%d\n",
                    (int)renderer->commandsQueued);
#endif
            SDL_CondWait(renderer->commandsQueuedCond, renderer->commandsQueuedMutex);
        }
        SDL_UnlockMutex(renderer->commandsQueuedMutex);

        SDL_LockMutex(renderer->commandsDequeuedMutex);
        VCRenderCommand *commands = renderer->commands;
        size_t commandsLength = renderer->commandsLength;
        renderer->commands = NULL;
        renderer->commandsLength = renderer->commandsCapacity = 0;
        renderer->commandsDequeued++;
        SDL_CondSignal(renderer->commandsDequeuedCond);
        SDL_UnlockMutex(renderer->commandsDequeuedMutex);

#ifdef VC_TEXTURE_SPEW
        bool hadUploads = false;
#endif

        for (uint32_t commandIndex = 0; commandIndex < commandsLength; commandIndex++) {
            VCRenderCommand *command = &commands[commandIndex];
            switch (command->command) {
            case VC_RENDER_COMMAND_UPLOAD_TEXTURE:
#ifdef VC_TEXTURE_SPEW
                hadUploads = true;
#endif
                VCAtlas_ProcessUploadCommand(&renderer->atlas, command);
                break;
            case VC_RENDER_COMMAND_DRAW_BATCHES:
                VCDebugger_AddSample(renderer->debugger,
                                     &renderer->debugger->stats.prepareTime,
                                     command->elapsedTime);
                VCRenderer_Draw(renderer, command->batches, command->batchesLength);
                VCRenderer_Present(renderer);
                break;
            case VC_RENDER_COMMAND_COMPILE_SHADER_PROGRAM:
                VCRenderer_CompileShaderProgram(renderer,
                                                command->shaderProgram,
                                                command->shaderProgramID);
                break;
            case VC_RENDER_COMMAND_DESTROY_SHADER_PROGRAM:
                VCRenderer_DestroyShaderProgram(renderer, command->shaderProgramID);
                break;
            }
        }

#ifdef VC_TEXTURE_SPEW
        if (hadUploads)
            VCRenderer_DumpAtlas(renderer);
#endif

        free(commands);

        commandsProcessed++;
    }
    return 0;
}

void VCRenderer_EnqueueCommand(VCRenderer *renderer, VCRenderCommand *command) {
    SDL_LockMutex(renderer->commandsQueuedMutex);
    SDL_LockMutex(renderer->commandsDequeuedMutex);
    if (renderer->commandsLength >= renderer->commandsCapacity) {
        renderer->commandsCapacity =
            renderer->commandsCapacity == 0 ? 1 : 2 * renderer->commandsCapacity;
        renderer->commands =
            (VCRenderCommand *)realloc(renderer->commands,
                                       sizeof(VCRenderCommand) * renderer->commandsCapacity);
    }
    renderer->commands[renderer->commandsLength] = *command;
    renderer->commandsLength++;
    SDL_UnlockMutex(renderer->commandsDequeuedMutex);
    SDL_UnlockMutex(renderer->commandsQueuedMutex);
}

void VCRenderer_SubmitCommands(VCRenderer *renderer) {
    SDL_LockMutex(renderer->commandsQueuedMutex);
    renderer->commandsQueued++;
    SDL_CondSignal(renderer->commandsQueuedCond);
    SDL_UnlockMutex(renderer->commandsQueuedMutex);

    SDL_LockMutex(renderer->commandsDequeuedMutex);
    while (renderer->commandsSubmitted == renderer->commandsDequeued) {
#if 0
        fprintf(stderr,
                "RSP thread waiting, commandsDequeued=%d\n",
                (int)renderer->commandsDequeued);
#endif
        SDL_CondWait(renderer->commandsDequeuedCond, renderer->commandsDequeuedMutex);
    }
    SDL_UnlockMutex(renderer->commandsDequeuedMutex);
    renderer->commandsSubmitted++;
}

static void VCRenderer_Init(VCRenderer *renderer, SDL_Window *window, SDL_GLContext context) {
    renderer->window = window;
    renderer->context = context;

    renderer->batches = NULL;
    renderer->batchesLength = 0;
    renderer->batchesCapacity = 0;

    VCRenderer_CompileAndLinkShaders(renderer);
    VCRenderer_CreateVBOs(renderer);
    VCRenderer_CreateFBO(renderer);
    VCRenderer_UploadBlitVertices(renderer);
    VCAtlas_Create(&renderer->atlas);
    VCRenderer_SetUniforms(renderer);

    renderer->shaderSubprogramLibrary = VCShaderCompiler_CreateSubprogramLibrary();
    renderer->shaderProgramDescriptorLibrary =
        VCShaderCompiler_CreateShaderProgramDescriptorLibrary();

    renderer->debugger = (VCDebugger *)malloc(sizeof(VCDebugger));
    if (renderer->debugger == NULL)
        abort();
    VCDebugger_Init(renderer->debugger, renderer);

    renderer->commands = NULL;
    renderer->commandsLength = 0;
    renderer->commandsCapacity = 0;
    renderer->commandsSubmitted = 0;

    renderer->commandsQueued = 0;
    renderer->commandsQueuedMutex = SDL_CreateMutex();
    renderer->commandsQueuedCond = SDL_CreateCond();
    renderer->commandsDequeued = 0;
    renderer->commandsDequeuedMutex = SDL_CreateMutex();
    renderer->commandsDequeuedCond = SDL_CreateCond();

    SDL_LockMutex(renderer->readyMutex);
    renderer->ready = true;
    SDL_CondSignal(renderer->readyCond);
    SDL_UnlockMutex(renderer->readyMutex);
}

void VCRenderer_Start(VCRenderer *renderer) {
    // These have to be set first to avoid races. Ugly...
    VCConfig *config = VCConfig_SharedConfig();
    renderer->windowSize.width = config->displayWidth;
    renderer->windowSize.height = config->displayHeight;
    renderer->currentEpoch = 0;
    renderer->currentSubprogramID = VC_INVALID_SUBPROGRAM_ID;
    renderer->triangleModeForCachedSubprogramID = 0;
    renderer->ready = false;
    renderer->readyMutex = SDL_CreateMutex();
    renderer->readyCond = SDL_CreateCond();

    assert(SDL_CreateThread(VCRenderer_ThreadMain, "VCRenderer", (void *)renderer));

    SDL_LockMutex(renderer->readyMutex);
    while (!renderer->ready)
        SDL_CondWait(renderer->readyCond, renderer->readyMutex);
    SDL_UnlockMutex(renderer->readyMutex);
}

static void VCRenderer_DestroyBatches(VCBatch *batches, size_t batchesLength) {
    for (size_t batchIndex = 0; batchIndex < batchesLength; batchIndex++)
        free(batches[batchIndex].vertices);
    free(batches);
}

static void VCRenderer_Draw(VCRenderer *renderer, VCBatch *batches, size_t batchesLength) {
    uint32_t beforeDrawTimestamp = SDL_GetTicks();

    SDL_GL_MakeCurrent(renderer->window, renderer->context);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo);
    /*GL(glClearColor((float)(rand() % 32) / 256.0,
                    (float)(rand() % 32) / 256.0,
                    (float)(rand() % 32) / 256.0,
                    1.0));*/
    VCRenderer_SetVBOStateForN64Program(renderer);

    GL(glDepthMask(GL_TRUE));
    GL(glDisable(GL_BLEND));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    GL(glDisable(GL_SCISSOR_TEST));
    GL(glEnable(GL_DEPTH_TEST));

    GL(glActiveTexture(GL_TEXTURE0));
    VCAtlas_Bind(&renderer->atlas);

    uint32_t totalVertexCount = 0;
    for (uint32_t batchIndex = 0; batchIndex < batchesLength; batchIndex++) {
        VCBatch *batch = &batches[batchIndex];
        assert(batch->programIDPresent);
        assert(batch->program.id < renderer->shaderProgramsLength);
        VCCompiledShaderProgram *program = &renderer->shaderPrograms[batch->program.id];
        GL(glUseProgram(program->program.program));

        GL(glDepthMask(batch->blendFlags.zUpdate ? GL_TRUE : GL_FALSE));
        GL(glDepthFunc(batch->blendFlags.zTest ? GL_LEQUAL : GL_ALWAYS));

        GL(glEnable(GL_BLEND));
        switch (batch->blendFlags.globalBlendMode) {
        case VC_GLOBAL_BLEND_MODE_NORMAL:
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case VC_GLOBAL_BLEND_MODE_ADD:
            GL(glBlendFunc(GL_ONE, GL_ONE));
            break;
        default:
            assert(0 && "Unknown global blend mode!");
        }

        GL(glViewport(batch->blendFlags.viewport.origin.x,
                      VC_N64_HEIGHT - (batch->blendFlags.viewport.origin.y +
                                       batch->blendFlags.viewport.size.height),
                      batch->blendFlags.viewport.size.width,
                      batch->blendFlags.viewport.size.height));

        GL(glBindBuffer(GL_ARRAY_BUFFER, renderer->n64VBO));
        GL(glBufferData(GL_ARRAY_BUFFER,
                        sizeof(VCN64Vertex) * batch->verticesLength,
                        batch->vertices,
                        GL_DYNAMIC_DRAW));
        GL(glDrawArrays(GL_TRIANGLES, 0, batch->verticesLength));
        totalVertexCount += batch->verticesLength;
    }

    uint32_t now = SDL_GetTicks();
    uint32_t elapsedDrawTime = now - beforeDrawTimestamp;
    VCDebugger_AddSample(renderer->debugger,
                         &renderer->debugger->stats.trianglesDrawn,
                         totalVertexCount / 3);
    VCDebugger_AddSample(renderer->debugger, &renderer->debugger->stats.batches, batchesLength);
    VCDebugger_AddSample(renderer->debugger, &renderer->debugger->stats.drawTime, elapsedDrawTime);
    VCDebugger_AddSample(renderer->debugger, &renderer->debugger->stats.viRate, now);

    // Calculate aspect ratio.
    GLint viewportWidth = renderer->windowSize.height * 4 / 3;
    GLint viewportX = (renderer->windowSize.width - viewportWidth) / 2;

    // Blit to the screen.
    GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL(glViewport(viewportX, 0, viewportWidth, renderer->windowSize.height));
    GL(glUseProgram(renderer->blitProgram.program));
    VCRenderer_SetVBOStateForBlitProgram(renderer);
    GL(glDisable(GL_DEPTH_TEST));
    GL(glDisable(GL_CULL_FACE));
    GL(glBindTexture(GL_TEXTURE_2D, renderer->fboTexture));
    GL(glBindBuffer(GL_ARRAY_BUFFER, renderer->quadVBO));
    GL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

    // Draw the debug overlay, if requested.
    if (VCConfig_SharedConfig()->debugDisplay)
        VCDebugger_DrawDebugOverlay(renderer->debugger, &renderer->windowSize);

    VCDebugger_NewFrame(renderer->debugger);
    VCRenderer_DestroyBatches(batches, batchesLength);
}

static void VCRenderer_Present(VCRenderer *renderer) {
    SDL_GL_SwapWindow(renderer->window);

    SDL_Event event;
    SDL_PollEvent(&event);
}

void VCRenderer_InitTriangleVertices(VCRenderer *renderer,
                                     VCN64Vertex *n64Vertices,
                                     SPVertex *spVertices,
                                     uint32_t *indices,
                                     uint32_t indexCount,
                                     uint8_t mode) {
    for (uint8_t triangleIndex = 0; triangleIndex < indexCount; triangleIndex++) {
        uint32_t vertexIndex = indices[triangleIndex];
        VCN64Vertex *n64Vertex = &n64Vertices[triangleIndex];
        SPVertex *spVertex = &spVertices[vertexIndex];

        n64Vertex->position.x = spVertex->x;
        n64Vertex->position.y = spVertex->y;
        n64Vertex->position.z = spVertex->z;
        n64Vertex->position.w = spVertex->w;

        if (gDP.otherMode.depthMode == ZMODE_DEC)
            n64Vertex->position.z -= 0.5;

        n64Vertex->textureUV.x = spVertex->s;
        n64Vertex->textureUV.y = spVertex->t;

        if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
            /*if ((gSP.textureTile[0]->cms & G_TX_MIRROR) != 0)
                n64Vertex->textureUV.x = gSP.textureTile[0]->lrs - n64Vertex->textureUV.x;
            if ((gSP.textureTile[0]->cmt & G_TX_MIRROR) != 0)
                n64Vertex->textureUV.y = gSP.textureTile[0]->lrt - n64Vertex->textureUV.y;*/

            // Texture scale is ignored for texture rectangle.
            if (mode != VC_TRIANGLE_MODE_TEXTURE_RECTANGLE) {
                n64Vertex->textureUV.x *= gSP.texture.scales;
                n64Vertex->textureUV.y *= gSP.texture.scalet;
            }

            n64Vertex->textureUV.x -= gSP.textureTile[0]->uls;
            n64Vertex->textureUV.y -= gSP.textureTile[0]->ult;

            if (gSP.textureTile[0]->shifts > 0 && gSP.textureTile[0]->shifts < 11)
                n64Vertex->textureUV.x /= (float)(1 << gSP.textureTile[0]->shifts);
            else if (gSP.textureTile[0]->shifts > 10)
                n64Vertex->textureUV.x *= (float)(1 << (16 - gSP.textureTile[0]->shifts));

            if (gSP.textureTile[0]->shiftt > 0 && gSP.textureTile[0]->shiftt < 11)
                n64Vertex->textureUV.y /= (float)(1 << gSP.textureTile[0]->shiftt);
            else if (gSP.textureTile[0]->shiftt > 10)
                n64Vertex->textureUV.y *= (float)(1 << (16 - gSP.textureTile[0]->shiftt));
        }

        n64Vertex->texture0.cachedTexture = VCAtlas_GetOrUploadTexture(&renderer->atlas,
                                                                       renderer,
                                                                       gSP.textureTile[0]);
        n64Vertex->texture1.cachedTexture = VCAtlas_GetOrUploadTexture(&renderer->atlas,
                                                                       renderer,
                                                                       gSP.textureTile[1]);

        VCColorf shadeColor = { spVertex->r, spVertex->g, spVertex->b, spVertex->a };
        n64Vertex->shade = VCColor_ColorFToColor(shadeColor);

        VCColor primColor = { gDP.primColor.r, gDP.primColor.g, gDP.primColor.b, gDP.primColor.a };
#if 0
        if ((primColor.r != 0.0 && primColor.r != 1.0) ||
                (primColor.g != 0.0 && primColor.g != 1.0) ||
                (primColor.b != 0.0 && primColor.b != 1.0) ||
                (primColor.a != 0.0 && primColor.a != 1.0))
            fprintf(stderr, "primColor=%f,%f,%f,%f\n", primColor.r, primColor.g, primColor.b, primColor.a);
#endif
        n64Vertex->primitive = primColor;

        VCColor envColor = { gDP.envColor.r, gDP.envColor.g, gDP.envColor.b, gDP.envColor.a };
#if 0
        if ((envColor.r != 0.0 && envColor.r != 1.0) ||
                (envColor.g != 0.0 && envColor.g != 1.0) ||
                (envColor.b != 0.0 && envColor.b != 1.0) ||
                (envColor.a != 0.0 && envColor.a != 1.0))
            fprintf(stderr, "envColor=%f,%f,%f,%f\n", envColor.r, envColor.g, envColor.b, envColor.a);
#endif
        n64Vertex->environment = envColor;
#if 0
        switch (mode) {
        case VC_TRIANGLE_MODE_NORMAL:
            VCCombiner_FillCombiner(&n64Vertex->combiner, &shadeColor);
            break;
        case VC_TRIANGLE_MODE_TEXTURE_RECTANGLE:
            VCCombiner_FillCombinerForTextureBlit(&n64Vertex->combiner);
            break;
        case VC_TRIANGLE_MODE_RECT_FILL:
            VCCombiner_FillCombinerForRectFill(&n64Vertex->combiner, &shadeColor);
            break;
        }
#endif
    }
}

void VCRenderer_CreateNewShaderProgramsIfNecessary(VCRenderer *renderer) {
    for (size_t batchIndex = 0; batchIndex < renderer->batchesLength; batchIndex++) {
        VCBatch *batch = &renderer->batches[batchIndex];
        bool newlyCreated = false;
        VCShaderSubprogramSignatureList list =
            VCShaderCompiler_ConvertSubprogramSignatureTableToList(&batch->program.table);
        VCShaderCompiler_DestroySubprogramSignatureTable(&batch->program.table);
        uint32_t programID =
            VCShaderCompiler_GetOrCreateShaderProgramID(renderer->shaderProgramDescriptorLibrary,
                                                        &list,
                                                        &newlyCreated);
        VCShaderCompiler_DestroySubprogramSignatureList(&list);
        batch->program.id = programID;
        batch->programIDPresent = true;

        if (!newlyCreated)
            continue;

        VCShaderProgramDescriptor *shaderProgramDescriptor =
            VCShaderCompiler_GetShaderProgramDescriptorByID(
                    renderer->shaderProgramDescriptorLibrary,
                    programID);

        VCRenderCommand command;
        command.command = VC_RENDER_COMMAND_COMPILE_SHADER_PROGRAM;
        command.shaderProgramID = programID;
        command.shaderProgram =
            VCShaderCompiler_GetOrCreateProgram(renderer->shaderSubprogramLibrary,
                                                shaderProgramDescriptor);
        VCRenderer_EnqueueCommand(renderer, &command);

        uint16_t shaderProgramIDToDelete = 0;
        while (VCShaderCompiler_ExpireOldProgramIfNecessary(
                    renderer->shaderProgramDescriptorLibrary,
                    &shaderProgramIDToDelete)) {
            VCRenderCommand expireCommand;
            expireCommand.command = VC_RENDER_COMMAND_DESTROY_SHADER_PROGRAM;
            expireCommand.shaderProgramID = shaderProgramIDToDelete;
            VCRenderer_EnqueueCommand(renderer, &expireCommand);
        }
    }
}

uint8_t VCRenderer_GetCurrentGlobalBlendMode(uint8_t triangleMode) {
    if (triangleMode == VC_TRIANGLE_MODE_TEXTURE_RECTANGLE)
        return VC_GLOBAL_BLEND_MODE_NORMAL;

    if (gDP.otherMode.cycleType == G_CYC_FILL)
        return VC_GLOBAL_BLEND_MODE_NORMAL;

    if (gDP.otherMode.forceBlender &&
        gDP.otherMode.cycleType != G_CYC_COPY &&
        !gDP.otherMode.alphaCvgSel) {
        switch (gDP.otherMode.l >> 16) {
            case 0x0448: // Add
            case 0x055A:
                return VC_GLOBAL_BLEND_MODE_ADD;
            default:
                return VC_GLOBAL_BLEND_MODE_NORMAL;
        }
    }

    return VC_GLOBAL_BLEND_MODE_NORMAL;
}

void VCRenderer_BeginNewFrame(VCRenderer *renderer) {
    free(renderer->batches);

    renderer->batches = (VCBatch *)malloc(sizeof(VCBatch) * INITIAL_BATCHES_CAPACITY);
    if (renderer->batches == NULL)
        abort();
    renderer->batchesLength = 0;
    renderer->batchesCapacity = INITIAL_BATCHES_CAPACITY;
}

void VCRenderer_EndFrame(VCRenderer *renderer) {
    renderer->currentEpoch++;
}

void VCRenderer_PopulateTextureBoundsInBatches(VCRenderer *renderer) {
    for (uint32_t batchIndex = 0; batchIndex < renderer->batchesLength; batchIndex++) {
        VCBatch *batch = &renderer->batches[batchIndex];
        for (uint32_t vertexIndex = 0; vertexIndex < batch->verticesLength; vertexIndex++) {
            VCN64Vertex *vertex = &batch->vertices[vertexIndex];
            VCRects textureBounds = { 0 };

            VCAtlas_FillTextureBounds(&textureBounds, &vertex->texture0.cachedTexture->info);
            vertex->texture0.textureBounds = textureBounds;

            VCAtlas_FillTextureBounds(&textureBounds, &vertex->texture1.cachedTexture->info);
            vertex->texture1.textureBounds = textureBounds;
        }
    }
}

void VCRenderer_SendBatchesToRenderThread(VCRenderer *renderer, uint32_t elapsedTime) {
    VCRenderCommand command = { 0 };
    command.command = VC_RENDER_COMMAND_DRAW_BATCHES;
    command.elapsedTime = elapsedTime;
    command.batches = renderer->batches;
    command.batchesLength = renderer->batchesLength;
    renderer->batches = NULL;
    renderer->batchesLength = 0;
    renderer->batchesCapacity = 0;
    VCRenderer_EnqueueCommand(renderer, &command);
}

bool VCRenderer_ShouldCull(SPVertex *va,
                           SPVertex *vb,
                           SPVertex *vc,
                           bool cullFront,
                           bool cullBack) {
    VCPoint4f a = { va->x, va->y, va->z, va->w };
    VCPoint4f b = { vb->x, vb->y, vb->z, vb->w };
    VCPoint4f c = { vc->x, vc->y, vc->z, vc->w };
    VCPoint3f a3 = VCPoint4f_Dehomogenize(&a);
    VCPoint3f b3 = VCPoint4f_Dehomogenize(&b);
    VCPoint3f c3 = VCPoint4f_Dehomogenize(&c);
    VCPoint3f ba = VCPoint3f_Sub(&b3, &a3);
    VCPoint3f ca = VCPoint3f_Sub(&c3, &a3);
    VCPoint3f cross = VCPoint3f_Cross(&ba, &ca);
    VCPoint3f ap = VCPoint3f_Neg(&a3);
    float dot = VCPoint3f_Dot(&ap, &cross);
    return (dot < 0.0 && cullFront) || (dot > 0.0 && cullBack);
}

void VCRenderer_AllocateTexturesAndEnqueueTextureUploadCommands(VCRenderer *renderer) {
    VCAtlas_Trim(&renderer->atlas, renderer->currentEpoch);
    VCAtlas_AllocateTexturesInAtlas(&renderer->atlas, renderer, true);
    VCAtlas_EnqueueCommandsToUploadTextures(&renderer->atlas, renderer);
}

void VCRenderer_InvalidateCachedSubprogramID(VCRenderer *renderer) {
    renderer->currentSubprogramID = VC_INVALID_SUBPROGRAM_ID;
}

