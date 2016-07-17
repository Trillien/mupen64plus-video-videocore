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

#define HASH_SEED                           0xdeadbeef
#define INITIAL_STORED_TEXTURES_CAPACITY    16

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
    atlas->uploadedTextures = NULL;

    VCAtlas_InitFreeList(atlas);

    atlas->hashState = XXH32_createState();

    GL(glGenTextures(1, &atlas->texture));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, atlas->texture));
    char *pixels = (char *)malloc(VC_ATLAS_TEXTURE_SIZE * VC_ATLAS_TEXTURE_SIZE * 4);
    for (int i = 0; i < VC_ATLAS_TEXTURE_SIZE * VC_ATLAS_TEXTURE_SIZE; i++) {
        pixels[i * 4 + 0] = 0xff;
        pixels[i * 4 + 1] = 0xff;
        pixels[i * 4 + 2] = 0xff;
        pixels[i * 4 + 3] = 0xff;
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

bool VCAtlas_Allocate(VCAtlas *atlas, VCRectus *result, VCSize2us *size) {
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
    result->origin = chosenRect.origin;
    result->size = *size;

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

static bool VCAtlas_LookUpTextureByTMEMHash(VCAtlas *atlas,
                                            VCTextureInfo *info,
                                            XXH32_hash_t tmemHash) {
    VCUploadedTexture *uploadedTexture = NULL;
    HASH_FIND_INT(atlas->uploadedTextures, &tmemHash, uploadedTexture);
    if (uploadedTexture == NULL)
        return false;
    
    // Move the item to the end of the list to maintain LRU sorting.
    HASH_DEL(atlas->uploadedTextures, uploadedTexture);
    HASH_ADD_INT(atlas->uploadedTextures, tmemHash, uploadedTexture);

    *info = uploadedTexture->info;
    return true;
}

static void VCAtlas_Clear(VCAtlas *atlas) {
    VCUploadedTexture *currentUploadedTexture, *temp_allocated_texture;
    HASH_ITER(hh, atlas->uploadedTextures, currentUploadedTexture, temp_allocated_texture) {
        HASH_DEL(atlas->uploadedTextures, currentUploadedTexture);
        free(currentUploadedTexture);
    }

    free(atlas->freeList);
    VCAtlas_InitFreeList(atlas);
}

static void VCAtlas_Upload(VCAtlas *atlas,
                           VCRenderCommand *command,
                           VCTextureInfo *info,
                           VCSize2us *size,
                           XXH32_hash_t tmemHash,
                           uint8_t *pixels,
                           bool repeatX,
                           bool repeatY,
                           bool mirrorX,
                           bool mirrorY) {
    VCUploadedTexture *uploadedTexture = (VCUploadedTexture *)malloc(sizeof(VCUploadedTexture));

    VCSize2us mirrorFactors = { (uint16_t)(mirrorX ? 2 : 1), (uint16_t)(mirrorY ? 2 : 1) };
    VCSize2us sizeIncludingMirror = {
        (uint16_t)(size->width * mirrorFactors.width),
        (uint16_t)(size->height * mirrorFactors.height)
    };
    VCSize2us sizeIncludingBorder = {
        (uint16_t)(sizeIncludingMirror.width + 2),
        (uint16_t)(sizeIncludingMirror.height + 2)
    };
    VCRectus uvIncludingBorder;
    if (!VCAtlas_Allocate(atlas, &uvIncludingBorder, &sizeIncludingBorder)) {
        VCAtlas_Clear(atlas);
        if (!VCAtlas_Allocate(atlas, &uvIncludingBorder, &sizeIncludingBorder)) {
            fprintf(stderr, "Couldn't allocate texture in atlas!\n");
            abort();
        }
    }

    info->uv.origin.x = uvIncludingBorder.origin.x + 1;
    info->uv.origin.y = uvIncludingBorder.origin.y + 1;
    info->uv.size.width = uvIncludingBorder.size.width - 2;
    info->uv.size.height = uvIncludingBorder.size.height - 2;
    info->repeatX = repeatX;
    info->repeatY = repeatY;
    info->mirrorX = mirrorX;
    info->mirrorY = mirrorY;
    uploadedTexture->info = *info;
    uploadedTexture->tmemHash = tmemHash;
    HASH_ADD_INT(atlas->uploadedTextures, tmemHash, uploadedTexture);
    
    // Add a border to prevent bleed.
    uint8_t *pixelsWithBorder = (uint8_t *)
        malloc(sizeIncludingBorder.width * sizeIncludingBorder.height * 4);
    uint32_t stride = size->width * 4;
    uint32_t strideWithBorder = sizeIncludingBorder.width * 4;
    for (uint32_t y = 0; y < size->height; y++) {
        memcpy(&pixelsWithBorder[(y + 1) * strideWithBorder + 4],
               &pixels[y * stride],
               stride);

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

    command->command = VC_RENDER_COMMAND_UPLOAD_TEXTURE;
    command->uv = uvIncludingBorder;
    command->pixels = pixelsWithBorder;
}

void VCAtlas_Bind(VCAtlas *atlas) {
    GL(glBindTexture(GL_TEXTURE_2D, atlas->texture));
}

GLint VCAtlas_GetGLTexture(VCAtlas *atlas) {
    return atlas->texture;
}

void VCAtlas_FillTextureBounds(VCRects *textureBounds, VCTextureInfo *textureInfo) {
    textureBounds->origin.x = (int16_t)textureInfo->uv.origin.x;
    textureBounds->origin.y = (int16_t)textureInfo->uv.origin.y;
    textureBounds->size.width = (int16_t)textureInfo->uv.size.width;
    textureBounds->size.height = (int16_t)textureInfo->uv.size.height;
    if (textureInfo->repeatX)
        textureBounds->size.width = -textureBounds->size.width;
    if (textureInfo->repeatY)
        textureBounds->size.height = -textureBounds->size.height;
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
    free(command->pixels);
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

void VCAtlas_GetOrUploadTexture(VCAtlas *atlas,
                                VCRenderer *renderer,
                                VCTextureInfo *textureInfo,
                                gDPTile *tile) {
    uint8_t tileIndex = (uint8_t)((ptrdiff_t)(tile - &gDP.tiles[0]) / sizeof(gDP.tiles[0]));
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE && atlas->textureInfoValid[tileIndex]) {
        *textureInfo = atlas->textureInfo[tileIndex];
        return;
    }

    VCSize2us textureSize;
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        textureSize.width = abs((int32_t)tile->lrs - (int32_t)tile->uls) + 1;
        textureSize.height = abs((int32_t)tile->lrt - (int32_t)tile->ult) + 1;
    } else {
        textureSize.width = gSP.bgImage.width;
        textureSize.height = gSP.bgImage.height;
    }
    
    XXH32_reset(atlas->hashState, HASH_SEED);
    XXH32_update(atlas->hashState, &gDP.textureMode, sizeof(gDP.textureMode));
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE)
        XXH32_update(atlas->hashState, tile, sizeof(gDPTile));
    else
        XXH32_update(atlas->hashState, &gSP.bgImage, sizeof(gSP.bgImage));

    uint8_t size = gDP.textureMode != TEXTUREMODE_BGIMAGE ? tile->size : gSP.bgImage.size;
    uint32_t bpp = TextureCache_SizeToBPP(size);

    static uint8_t *buffer = NULL;
    static size_t bufferSize = 0;
    size_t neededSize = textureSize.width * textureSize.height * bpp / 8;
    if (bufferSize < neededSize) {
        free(buffer);
        buffer = (uint8_t *)malloc(neededSize);
        bufferSize = neededSize;
    }

    uint32_t line;
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        line = tile->line;
        if (size == G_IM_SIZ_32b)
            line <<= 1;
    } else {
        line = textureSize.width * bpp / 8;
    }

    // Hash the texture data.
    assert(textureSize.width < 1024 && textureSize.height < 1024);
    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        for (int t = 0; t < textureSize.height; t++) {
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
        neededSize = 2 * 16;
        if (bufferSize < neededSize) {
            free(buffer);
            buffer = (uint8_t *)malloc(neededSize);
            bufferSize = neededSize;
        }

        // TODO: CI4RGBA_RGBA5551
        memcpy(buffer, &TMEM[256 + (palette << 4)], 2 * 16);
        XXH32_update(atlas->hashState, buffer, 2 * 16);
    }

    XXH32_hash_t tmemHash = XXH32_digest(atlas->hashState);
    if (!VCAtlas_LookUpTextureByTMEMHash(atlas, textureInfo, tmemHash)) {
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
        VCRenderCommand command;
        VCAtlas_Upload(atlas,
                       &command,
                       textureInfo,
                       &textureSize,
                       tmemHash,
                       texturePixels,
                       repeatsX,
                       repeatsY,
                       mirrorX,
                       mirrorY);
        free(texturePixels);
        if (command.command != VC_RENDER_COMMAND_NONE)
            VCRenderer_EnqueueCommand(renderer, &command);

        VCDebugger_IncrementSample(renderer->debugger,
                                   &renderer->debugger->stats.texturesUploaded);
    }

    if (gDP.textureMode != TEXTUREMODE_BGIMAGE) {
        atlas->textureInfo[tileIndex] = *textureInfo;
        atlas->textureInfoValid[tileIndex] = true;
    }
}

void VCAtlas_InvalidateCache(VCAtlas *atlas) {
    for (uint32_t i = 0;
         i < sizeof(atlas->textureInfoValid) / sizeof(atlas->textureInfoValid[0]);
         i++) {
        atlas->textureInfoValid[i] = false;
    }
}

