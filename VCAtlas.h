// mupen64plus-video-videocore/VCAtlas.h
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#ifndef VC_ATLAS_H
#define VC_ATLAS_H

#include <stdlib.h>
#include "VCGL.h"
#include "VCGeometry.h"
#include "uthash.h"
#include "xxhash.h"

#define VC_ATLAS_TEXTURE_SIZE   1024
#define VC_ATLAS_TILE_COUNT     8

struct VCN64Vertex;
struct VCRenderCommand;
struct VCRenderer;
struct gDPTile;

struct VCTextureInfo {
    VCRectus uv;
    uint8_t *pixels;
    bool repeatX;
    bool repeatY;
    bool mirrorX;
    bool mirrorY;
    bool uvValid;
    bool needsUpload;
};

struct VCCachedTexture {
    VCTextureInfo info;
    XXH32_hash_t tmemHash;
    uint32_t lastUsedEpoch;
    UT_hash_handle hh;
};

struct VCAtlas {
    GLuint texture;
    VCCachedTexture *cachedTileTextures[VC_ATLAS_TILE_COUNT];
    VCCachedTexture *cachedTextures;
    VCRectus *freeList;
    size_t freeListSize;
    size_t freeListCapacity;
    size_t textureBytesUsed;
    XXH32_state_t *hashState;
};

inline void VCAtlas_FillTextureBounds(VCRects *textureBounds, VCTextureInfo *textureInfo) {
    textureBounds->origin.x = (int16_t)textureInfo->uv.origin.x;
    textureBounds->origin.y = (int16_t)textureInfo->uv.origin.y;
    textureBounds->size.width = (int16_t)textureInfo->uv.size.width;
    textureBounds->size.height = (int16_t)textureInfo->uv.size.height;
    if (textureInfo->repeatX)
        textureBounds->size.width = -textureBounds->size.width;
    if (textureInfo->repeatY)
        textureBounds->size.height = -textureBounds->size.height;
}

void VCAtlas_Create(VCAtlas *atlas);
bool VCAtlas_Allocate(VCAtlas *atlas, VCRectus *result, VCSize2us *size);
void VCAtlas_Bind(VCAtlas *atlas);
GLint VCAtlas_GetGLTexture(VCAtlas *atlas);
void VCAtlas_FillTextureBounds(VCRects *textureBounds, VCTextureInfo *textureInfo);
void VCAtlas_AllocateTexturesInAtlas(VCAtlas *atlas,
                                     const VCRenderer *renderer,
                                     bool clearAndRetryOnOverflow);
void VCAtlas_EnqueueCommandsToUploadTextures(VCAtlas *atlas, VCRenderer *renderer);
void VCAtlas_ProcessUploadCommand(VCAtlas *atlas, VCRenderCommand *command);
VCCachedTexture *VCAtlas_GetOrUploadTexture(VCAtlas *atlas, VCRenderer *renderer, gDPTile *tile);
void VCAtlas_InvalidateCache(VCAtlas *atlas);
void VCAtlas_Trim(VCAtlas *atlas, uint32_t currentEpoch);

#endif

