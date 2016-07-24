// mupen64plus-video-videocore/VCAtlas.cpp
//
// Copyright (c) 2016 The mupen64plus-video-videocore Authors

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "N64.h"
#include "Textures.h"
#include "VCAtlas.h"
#include "VCGL.h"
#include "VCGeometry.h"
#include "VCRenderer.h"
#include "convert.h"
#include "gDP.h"
#include "gSP.h"

#define BYTES_PER_PIXEL                     4
#define HASH_SEED                           0xdeadbeef
#define INITIAL_STORED_TEXTURES_CAPACITY    16
#define MAX_TEXTURE_BYTES_USED              (4 * 1024 * 1024)

#define S8  3
#define S16 1

static void VCAtlas_InitFreeList(VCAtlas *atlas) {
    atlas->freeList = (VCRectus *)malloc(sizeof(VCRectus));
    atlas->freeListSize = 1;
    atlas->freeListCapacity = 1;
    atlas->freeList[0].origin.x = atlas->freeList[0].origin.y = 0;
    atlas->freeList[0].size.width = atlas->freeList[0].size.height = VC_ATLAS_TEXTURE_SIZE;
}

void VCAtlas_Create(VCAtlas *atlas) {
    atlas->cachedTextures = NULL;
    for (uint32_t i = 0; i < VC_ATLAS_TILE_COUNT; i++)
        atlas->cachedTileTextures[i] = NULL;

    atlas->textureBytesUsed = 0;

    VCAtlas_InitFreeList(atlas);

    atlas->hashState = XXH32_createState();

    GL(glGenTextures(1, &atlas->texture));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, atlas->texture));
    char *pixels = (char *)malloc(VC_ATLAS_TEXTURE_SIZE * VC_ATLAS_TEXTURE_SIZE * BYTES_PER_PIXEL);
    for (int i = 0; i < VC_ATLAS_TEXTURE_SIZE * VC_ATLAS_TEXTURE_SIZE; i++) {
        pixels[i * BYTES_PER_PIXEL + 0] = 0xff;
        pixels[i * BYTES_PER_PIXEL + 1] = 0xff;
        pixels[i * BYTES_PER_PIXEL + 2] = 0xff;
        pixels[i * BYTES_PER_PIXEL + 3] = 0xff;
    }
    GL(glTexImage2D(GL_TEXTURE_2D,
                    0,
                    GL_RGBA,
                    VC_ATLAS_TEXTURE_SIZE,
                    VC_ATLAS_TEXTURE_SIZE,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    pixels));
    free(pixels);
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
}

bool VCAtlas_Allocate(VCAtlas *atlas, VCPoint2us *result, VCSize2us *size) {
    if (atlas->freeListSize == 0)
        return false;

    size_t bestIndex = 0;
    uint32_t best_area = UINT_MAX;
    for (size_t i = 0; i < atlas->freeListSize; i++) {
        VCRectus *candidate = &atlas->freeList[i];
        uint32_t candidate_area = (uint32_t)candidate->size.width *
            (uint32_t)candidate->size.height;
        if (candidate->size.width >= size->width &&
                candidate->size.height >= size->height &&
                candidate_area < best_area) {
            bestIndex = i;
            best_area = candidate_area;
        }
    }
    if (best_area == UINT_MAX)
        return false;

    VCRectus chosenRect = atlas->freeList[bestIndex];
    *result = chosenRect.origin;

    // Guillotine to right.
    atlas->freeList[bestIndex].origin.x = chosenRect.origin.x + size->width;
    atlas->freeList[bestIndex].origin.y = chosenRect.origin.y;
    atlas->freeList[bestIndex].size.width = chosenRect.size.width - size->width;
    atlas->freeList[bestIndex].size.height = size->height;

    // Guillotine to bottom.
    if (atlas->freeListSize + 1 > atlas->freeListCapacity) {
        atlas->freeListCapacity *= 2;
        atlas->freeList = (VCRectus *)realloc(atlas->freeList,
                                              sizeof(VCRectus) * atlas->freeListCapacity);
    }

    atlas->freeList[atlas->freeListSize].origin.x = chosenRect.origin.x;
    atlas->freeList[atlas->freeListSize].origin.y = chosenRect.origin.y + size->height;
    atlas->freeList[atlas->freeListSize].size.width = chosenRect.size.width;
    atlas->freeList[atlas->freeListSize].size.height = chosenRect.size.height - size->height;
    atlas->freeListSize++;
    return true;
}

static VCCachedTexture *VCAtlas_LookUpTextureByTMEMHash(VCAtlas *atlas, XXH32_hash_t tmemHash) {
    VCCachedTexture *cachedTexture = NULL;
    HASH_FIND_INT(atlas->cachedTextures, &tmemHash, cachedTexture);
    if (cachedTexture == NULL)
        return NULL;
    
    // Move the item to the end of the list to maintain LRU sorting.
    HASH_DEL(atlas->cachedTextures, cachedTexture);
    HASH_ADD_INT(atlas->cachedTextures, tmemHash, cachedTexture);
    return cachedTexture;
}

#if 0
static void VCAtlas_Clear(VCAtlas *atlas) {
    VCCachedTexture *currentCachedTexture, *temp_allocated_texture;
    HASH_ITER(hh, atlas->cachedTextures, currentCachedTexture, temp_allocated_texture) {
        HASH_DEL(atlas->cachedTextures, currentCachedTexture);
        free(currentCachedTexture);
    }

    free(atlas->freeList);
    VCAtlas_InitFreeList(atlas);
}
#endif

static void VCAtlas_ClearAtlasTexture(VCAtlas *atlas) {
    VCCachedTexture *cachedTexture = NULL, *tempCachedTexture = NULL;
    HASH_ITER(hh, atlas->cachedTextures, cachedTexture, tempCachedTexture) {
        cachedTexture->info.uvValid = false;
    }

    free(atlas->freeList);
    VCAtlas_InitFreeList(atlas);
}

void VCAtlas_AllocateTexturesInAtlas(VCAtlas *atlas,
                                     const VCRenderer *renderer,
                                     bool clearAndRetryOnOverflow) {
    VCCachedTexture *cachedTexture = NULL, *tempCachedTexture = NULL;
    HASH_ITER(hh, atlas->cachedTextures, cachedTexture, tempCachedTexture) {
        if (cachedTexture->info.uvValid)
            continue;
        if (cachedTexture->lastUsedEpoch != renderer->currentEpoch)
            continue;

        VCPoint2us originIncludingBorder = { 0, 0 };
        VCSize2us sizeIncludingBorder = {
            (uint16_t)(cachedTexture->info.uv.size.width + 2),
            (uint16_t)(cachedTexture->info.uv.size.height + 2),
        };
        if (VCAtlas_Allocate(atlas, &originIncludingBorder, &sizeIncludingBorder)) {
            cachedTexture->info.uv.origin.x = originIncludingBorder.x + 1;
            cachedTexture->info.uv.origin.y = originIncludingBorder.y + 1;
            cachedTexture->info.uvValid = true;
            cachedTexture->info.needsUpload = true;
            continue;
        }

        if (!clearAndRetryOnOverflow) {
            fprintf(stderr, "Couldn't allocate texture in atlas!\n");
            abort();
        }

        //fprintf(stderr, "clearing atlas texture!\n");
        VCAtlas_ClearAtlasTexture(atlas);
        VCAtlas_AllocateTexturesInAtlas(atlas, renderer, false);
        return;
    }
}

static VCRectus VCTextureInfo_UVIncludingBorder(VCTextureInfo *info)  {
    VCRectus uv;
    uv.origin.x = info->uv.origin.x - 1;
    uv.origin.y = info->uv.origin.y - 1;
    uv.size.width = info->uv.size.width + 2;
    uv.size.height = info->uv.size.height + 2;
    return uv;
}

void VCAtlas_EnqueueCommandsToUploadTextures(VCAtlas *atlas, VCRenderer *renderer) {
    VCCachedTexture *cachedTexture = NULL, *tempCachedTexture = NULL;
    HASH_ITER(hh, atlas->cachedTextures, cachedTexture, tempCachedTexture) {
        if (!cachedTexture->info.needsUpload)
            continue;

        VCRenderCommand command;
        command.command = VC_RENDER_COMMAND_UPLOAD_TEXTURE;
        command.uv = VCTextureInfo_UVIncludingBorder(&cachedTexture->info);
        command.pixels = cachedTexture->info.pixels;
        VCRenderer_EnqueueCommand(renderer, &command);

        cachedTexture->info.needsUpload = false;
    }
}

static VCCachedTexture *VCAtlas_CacheTexture(VCAtlas *atlas,
                                             VCSize2us *size,
                                             XXH32_hash_t tmemHash,
                                             uint8_t *pixels,
                                             uint32_t currentEpoch,
                                             bool repeatX,
                                             bool repeatY,
                                             bool mirrorX,
                                             bool mirrorY) {
    VCCachedTexture *cachedTexture = (VCCachedTexture *)malloc(sizeof(VCCachedTexture));
    cachedTexture->lastUsedEpoch = currentEpoch;

    VCSize2us mirrorFactors = { (uint16_t)(mirrorX ? 2 : 1), (uint16_t)(mirrorY ? 2 : 1) };
    VCSize2us sizeIncludingMirror = {
        (uint16_t)(size->width * mirrorFactors.width),
        (uint16_t)(size->height * mirrorFactors.height)
    };
    VCSize2us sizeIncludingBorder = {
        (uint16_t)(sizeIncludingMirror.width + 2),
        (uint16_t)(sizeIncludingMirror.height + 2)
    };

    cachedTexture->tmemHash = tmemHash;
    HASH_ADD_INT(atlas->cachedTextures, tmemHash, cachedTexture);
    
    // Add a border to prevent bleed.
    size_t dataSize = sizeIncludingBorder.width * sizeIncludingBorder.height * BYTES_PER_PIXEL;
    uint8_t *pixelsWithBorder = (uint8_t *)malloc(dataSize);
    atlas->textureBytesUsed += dataSize;

    uint32_t stride = size->width * BYTES_PER_PIXEL;
    uint32_t strideWithBorder = sizeIncludingBorder.width * BYTES_PER_PIXEL;
    for (uint32_t y = 0; y < size->height; y++) {
        memcpy(&pixelsWithBorder[(y + 1) * strideWithBorder + 4], &pixels[y * stride], stride);

        // Replicate a mirrored strip along X if necessary.
        if (mirrorX) {
            for (uint32_t x = 0; x < size->width; x++) {
                memcpy(&pixelsWithBorder[(y + 1) * strideWithBorder + (1 + size->width + x) * 4],
                       &pixelsWithBorder[(y + 1) * strideWithBorder + (size->width - x) * 4],
                       4);
            }
        }

        // The RDP is weird here: we never bilerp on the top or left sides of a texture, even if
        // repeating is on.
        memcpy(&pixelsWithBorder[(y + 1) * strideWithBorder],
               &pixels[y * stride],
               4);
        if (repeatX) {
            memcpy(&pixelsWithBorder[(y + 1) * strideWithBorder +
                                     (sizeIncludingMirror.width + 1) * 4],
                   &pixels[y * stride],
                   4);
        } else {
            memcpy(&pixelsWithBorder[(y + 1) * strideWithBorder +
                                     (sizeIncludingMirror.width + 1) * 4],
                   &pixels[y * stride + (sizeIncludingMirror.width - 1) * 4],
                   4);
        }
    }

    // Replicate Y if necessary.
    if (mirrorY) {
        for (uint32_t y = 0; y < size->height; y++) {
            memcpy(&pixelsWithBorder[(sizeIncludingMirror.height - 1 - y + 1) * strideWithBorder],
                   &pixelsWithBorder[(y + 1) * strideWithBorder],
                   strideWithBorder);
        }
    }

    // See above: replicate the strange bilerp quirk.
    memcpy(&pixelsWithBorder[4], &pixels[0], stride);
    if (repeatY) {
        memcpy(&pixelsWithBorder[(sizeIncludingMirror.height + 1) * strideWithBorder + 4],
               &pixels[0],
               stride);
    } else {
        memcpy(&pixelsWithBorder[(sizeIncludingMirror.height + 1) * strideWithBorder + 4],
               &pixels[(sizeIncludingMirror.height - 1) * stride],
               stride);
    }

    // Zero out corners.
    memset(&pixelsWithBorder[0], '\0', 4);
    memset(&pixelsWithBorder[strideWithBorder - 4], '\0', 4);
    memset(&pixelsWithBorder[(sizeIncludingMirror.height + 1) * strideWithBorder], '\0', 4);
    memset(&pixelsWithBorder[(sizeIncludingMirror.height + 2) * strideWithBorder - 4], '\0', 4);

    cachedTexture->info.uv.origin.x = 0;
    cachedTexture->info.uv.origin.y = 0;
    cachedTexture->info.uv.size.width = sizeIncludingMirror.width;
    cachedTexture->info.uv.size.height = sizeIncludingMirror.height;
    cachedTexture->info.repeatX = repeatX;
    cachedTexture->info.repeatY = repeatY;
    cachedTexture->info.mirrorX = mirrorX;
    cachedTexture->info.mirrorY = mirrorY;
    cachedTexture->info.pixels = pixelsWithBorder;
    cachedTexture->info.uvValid = false;
    cachedTexture->info.needsUpload = false;
    return cachedTexture;
}

void VCAtlas_Bind(VCAtlas *atlas) {
    GL(glBindTexture(GL_TEXTURE_2D, atlas->texture));
}

GLint VCAtlas_GetGLTexture(VCAtlas *atlas) {
    return atlas->texture;
}

void VCAtlas_ProcessUploadCommand(VCAtlas *atlas, VCRenderCommand *command) {
    GL(glBindTexture(GL_TEXTURE_2D, atlas->texture));
    GL(glTexSubImage2D(GL_TEXTURE_2D,
                       0,
                       command->uv.origin.x,
                       command->uv.origin.y,
                       command->uv.size.width,
                       command->uv.size.height,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       command->pixels));
}

static uint8_t *VCAtlas_ConvertTextureToRGBA(VCSize2us *textureSize, gDPTile *tile) {
    uint32_t maskSMask = tile->masks != 0 ? (1 << tile->masks) - 1 : 0xffff;
    uint32_t mirrorSBit = tile->masks != 0 && tile->mirrors ? 1 << tile->masks : 0;
    uint32_t maskTMask = tile->maskt != 0 ? (1 << tile->maskt) - 1 : 0xffff;
    uint32_t mirrorTBit = tile->maskt != 0 && tile->mirrort ? 1 << tile->maskt : 0;

    uint8_t *result = (uint8_t *)malloc(textureSize->width * textureSize->height * 4);
    if (result == NULL)
        abort();

    for (uint32_t v = 0; v < textureSize->height; v++) {
        uint32_t t = v & maskTMask;
        if (t & mirrorTBit)
            t ^= maskTMask;
        for (uint32_t u = 0; u < textureSize->width; u++) {
            uint32_t s = u & maskSMask;
            if (u & mirrorSBit)
                s ^= maskSMask;
            int32_t color = TextureCache_GetTexel(tile, s, t);
            result[(v * textureSize->width + u) * 4 + 0] = color;
            result[(v * textureSize->width + u) * 4 + 1] = color >> 8;
            result[(v * textureSize->width + u) * 4 + 2] = color >> 16;
            result[(v * textureSize->width + u) * 4 + 3] = color >> 24;
        }
    }
    return result;
}

static uint8_t *VCAtlas_ConvertBGImageTextureToRGBA(VCSize2us *textureSize) {
    uint32_t bpp = TextureCache_SizeToBPP(gSP.bgImage.size);
    uint32_t length = textureSize->width * textureSize->height * bpp / 8;
    uint8_t *swapped = (uint8_t *)malloc(length);
    if (swapped == NULL)
        abort();

    UnswapCopy(&RDRAM[gSP.bgImage.address], swapped, length);

    uint8_t *result = (uint8_t *)malloc(textureSize->width * textureSize->height * 4);
    if (result == NULL)
        abort();

    for (uint32_t v = 0; v < textureSize->height; v++) {
        uint32_t t = v;
        for (uint32_t u = 0; u < textureSize->width; u++) {
            uint32_t s = u;
            int32_t color = TextureCache_GetTexelFromBGImage(swapped, s, t);
            result[(v * textureSize->width + u) * 4 + 0] = color;
            result[(v * textureSize->width + u) * 4 + 1] = color >> 8;
            result[(v * textureSize->width + u) * 4 + 2] = color >> 16;
            result[(v * textureSize->width + u) * 4 + 3] = color >> 24;
        }
    }

    free(swapped);
    return result;
}

VCCachedTexture *VCAtlas_GetOrUploadTexture(VCAtlas *atlas, VCRenderer *renderer, gDPTile *tile) {
    uint8_t tileIndex = (uint8_t)((ptrdiff_t)(tile - &gDP.tiles[0]) / sizeof(gDP.tiles[0]));
    uint32_t currentEpoch = renderer->currentEpoch;
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE && atlas->cachedTileTextures[tileIndex] != NULL) {
        VCCachedTexture *cachedTexture = atlas->cachedTileTextures[tileIndex];
        cachedTexture->lastUsedEpoch = currentEpoch;
        return cachedTexture;
    }

    VCSize2us textureSize;
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        textureSize.width = abs((int32_t)tile->lrs - (int32_t)tile->uls) + 1;
        textureSize.height = abs((int32_t)tile->lrt - (int32_t)tile->ult) + 1;
    } else {
        textureSize.width = gSP.bgImage.width;
        textureSize.height = gSP.bgImage.height;
    }
    
    uint8_t size = gDP.textureMode != TEXTUREMODE_BGIMAGE ? tile->size : gSP.bgImage.size;
    uint32_t bpp = TextureCache_SizeToBPP(size);

    uint32_t line;
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        line = tile->line;
        if (size == G_IM_SIZ_32b)
            line <<= 1;
    } else {
        line = textureSize.width * bpp / 8;
    }

    XXH32_reset(atlas->hashState, HASH_SEED);
    XXH32_update(atlas->hashState, &tile->format, sizeof(tile->format));
    XXH32_update(atlas->hashState, &size, sizeof(size));

    static uint8_t *buffer = NULL;
    static size_t bufferSize = 0;

    size_t neededSize = textureSize.width * textureSize.height * bpp / 8;
    if (bufferSize < neededSize) {
        free(buffer);
        buffer = (uint8_t *)malloc(neededSize);
        bufferSize = neededSize;
    }

    // Hash the texture data.
    assert(textureSize.width < 1024 && textureSize.height < 1024);
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        for (uint32_t t = 0; t < textureSize.height; t++) {
            uint64_t *scanlineStart = &TMEM[tile->tmem] + t * line;
            memcpy(&buffer[t * textureSize.width * bpp / 8],
                   scanlineStart,
                   textureSize.width * bpp / 8);
        }
    } else {
        for (int t = 0; t < textureSize.height; t++) {
            uint64_t *scanlineStart = (uint64_t *)(&RDRAM[gSP.bgImage.address] + t * line);
            memcpy(&buffer[t * textureSize.width * bpp / 8],
                   scanlineStart,
                   textureSize.width * bpp / 8);
        }
    }
    XXH32_update(atlas->hashState, buffer, neededSize);

    // Hash the palette too, if applicable.
    uint32_t palette =
        gDP.textureMode != TEXTUREMODE_BGIMAGE ? tile->palette : gSP.bgImage.palette;
    if (tile->format == G_IM_FMT_CI) {
        uint16_t paletteBuffer[16];
        for (uint32_t entry = 0; entry < 16; entry++)
            paletteBuffer[entry] = *(uint16_t *)&TMEM[256 + (palette << 4) + entry];
        XXH32_update(atlas->hashState, paletteBuffer, sizeof(paletteBuffer));
    }

    XXH32_hash_t tmemHash = XXH32_digest(atlas->hashState);
    VCCachedTexture *cachedTexture = NULL;
    if ((cachedTexture = VCAtlas_LookUpTextureByTMEMHash(atlas, tmemHash)) == NULL) {
#ifdef VC_TEXTURE_SPEW
        fprintf(stderr,
                "cache miss: format=%d size=%d line=%d size=%dx%d\n",
                (int)tile->format,
                (int)tile->size,
                (int)tile->line,
                (int)textureSize.width,
                (int)textureSize.height);
#endif
        if (gDP.textureMode == TEXTUREMODE_FRAMEBUFFER) {
            fprintf(stderr, "*** framebuffer image detected! expect corruption.\n");
        }

        uint8_t *texturePixels;
        if (gDP.textureMode != TEXTUREMODE_BGIMAGE)
            texturePixels = VCAtlas_ConvertTextureToRGBA(&textureSize, tile);
        else
            texturePixels = VCAtlas_ConvertBGImageTextureToRGBA(&textureSize);

        bool repeatsX = gDP.textureMode != TEXTUREMODE_BGIMAGE && (tile->cms & G_TX_CLAMP) == 0;
        bool repeatsY = gDP.textureMode != TEXTUREMODE_BGIMAGE && (tile->cmt & G_TX_CLAMP) == 0;
        bool mirrorX = gDP.textureMode != TEXTUREMODE_BGIMAGE && tile->mirrors && tile->masks != 0;
        bool mirrorY = gDP.textureMode != TEXTUREMODE_BGIMAGE && tile->mirrort && tile->maskt != 0;
        cachedTexture = VCAtlas_CacheTexture(atlas,
                                             &textureSize,
                                             tmemHash,
                                             texturePixels,
                                             renderer->currentEpoch,
                                             repeatsX,
                                             repeatsY,
                                             mirrorX,
                                             mirrorY);
        assert(cachedTexture != NULL);
        free(texturePixels);

        VCDebugger_IncrementSample(renderer->debugger,
                                   &renderer->debugger->stats.texturesUploaded);
    }

    cachedTexture->lastUsedEpoch = currentEpoch;
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE)
        atlas->cachedTileTextures[tileIndex] = cachedTexture;
    return cachedTexture;
}

void VCAtlas_InvalidateCache(VCAtlas *atlas) {
    for (uint32_t i = 0; i < VC_ATLAS_TILE_COUNT; i++)
        atlas->cachedTileTextures[i] = NULL;
}

static void VCCachedTexture_Destroy(VCCachedTexture *cachedTexture) {
    if (cachedTexture == NULL)
        return;
    free(cachedTexture->info.pixels);
    free(cachedTexture);
}

void VCAtlas_Trim(VCAtlas *atlas, uint32_t currentEpoch) {
    VCCachedTexture *cachedTexture = NULL, *tempCachedTexture = NULL;
    bool cacheNeedsInvalidation = false;
    HASH_ITER(hh, atlas->cachedTextures, cachedTexture, tempCachedTexture) {
        if (atlas->textureBytesUsed <= MAX_TEXTURE_BYTES_USED)
            return;
        if (cachedTexture->lastUsedEpoch == currentEpoch)
            continue;
        VCRectus uv = VCTextureInfo_UVIncludingBorder(&cachedTexture->info);
        size_t bytesUsedByTexture = uv.size.width * uv.size.height * BYTES_PER_PIXEL;
        assert(atlas->textureBytesUsed >= bytesUsedByTexture);
        atlas->textureBytesUsed -= bytesUsedByTexture;

        HASH_DEL(atlas->cachedTextures, cachedTexture);
        VCCachedTexture_Destroy(cachedTexture);
        cacheNeedsInvalidation = true;
    }

    if (cacheNeedsInvalidation)
        VCAtlas_InvalidateCache(atlas);
}

