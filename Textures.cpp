#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#include <memory.h>
#include "Textures.h"
#include "GBI.h"
#include "RSP.h"
#include "gDP.h"
#include "gSP.h"
#include "N64.h"
#include "CRC.h"
#include "2xSAI.h"
#include "FrameBuffer.h"
#include "convert.h"

typedef u32 (*GetTexelFunc)( u64 *src, u16 x, u16 i, u8 palette );

inline u32 GetNone( u64 *src, u16 x, u16 i, u8 palette )
{
	return 0x00000000;
}

inline u32 GetCI4IA_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return IA88_RGBA4444( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return IA88_RGBA4444( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetCI4IA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

#ifdef VC_TEXTURE_SPEW
    fprintf(stderr, "CI4IA accessing %d\n", (int)((x>>1)^(i<<1)));
#endif
	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return IA88_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return IA88_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetCI4RGBA_RGBA5551( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

    //fprintf(stderr, "CI4RGBA accessing %d\n", (int)((x>>1)^(i<<1)));
	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return RGBA5551_RGBA5551( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return RGBA5551_RGBA5551( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetCI4RGBA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

#ifdef VC_TEXTURE_SPEW
    fprintf(stderr, "CI4RGBA accessing %d", (int)((x>>1)^(i<<1)));
#endif
	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	if (x & 1)
		return RGBA5551_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
	else
		return RGBA5551_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

inline u32 GetIA31_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return IA31_RGBA8888( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetIA31_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return IA31_RGBA4444( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetI4_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return I4_RGBA8888( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetI4_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	u8 color4B;

	color4B = ((u8*)src)[(x>>1)^(i<<1)];

	return I4_RGBA4444( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

inline u32 GetCI8IA_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA4444( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetCI8IA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA8888( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetCI8RGBA_RGBA5551( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA5551( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetCI8RGBA_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA8888( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

inline u32 GetIA44_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA44_RGBA8888(((u8*)src)[x^(i<<1)]);
}

inline u32 GetIA44_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA44_RGBA4444(((u8*)src)[x^(i<<1)]);
}

inline u32 GetI8_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return I8_RGBA8888(((u8*)src)[x^(i<<1)]);
}

inline u32 GetI8_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return I8_RGBA4444(((u8*)src)[x^(i<<1)]);
}

inline u32 GetRGBA5551_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA8888( ((u16*)src)[x^i] );
}

inline u32 GetRGBA5551_RGBA5551( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA5551_RGBA5551( ((u16*)src)[x^i] );
}

inline u32 GetIA88_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA8888(((u16*)src)[x^i]);
}

inline u32 GetIA88_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return IA88_RGBA4444(((u16*)src)[x^i]);
}

inline u32 GetRGBA8888_RGBA8888( u64 *src, u16 x, u16 i, u8 palette )
{
	return ((u32*)src)[x^i];
}

inline u32 GetRGBA8888_RGBA4444( u64 *src, u16 x, u16 i, u8 palette )
{
	return RGBA8888_RGBA4444(((u32*)src)[x^i]);
}

const struct
{
	GetTexelFunc	Get16;
	GetTexelFunc	Get32;
	u32				lineShift, maxTexels;
} imageFormat[4][5] =
{ //		Get16					Get32					autoFormat
	{ // 4-bit
		{	GetCI4RGBA_RGBA5551, GetCI4RGBA_RGBA8888, 4, 4096 }, // CI (Banjo-Kazooie uses this, doesn't make sense, but it works...)
		{	GetNone,							GetNone,				 4, 8192 }, // YUV
		{	GetCI4RGBA_RGBA5551,				GetCI4RGBA_RGBA8888,	 4, 4096 }, // CI
		{	GetIA31_RGBA4444,					GetIA31_RGBA8888,		 4, 8192 }, // IA
		{	GetI4_RGBA4444,						GetI4_RGBA8888,			 4, 8192 }, // I
	},
	{ // 8-bit
		{	GetCI8RGBA_RGBA5551,				GetCI8RGBA_RGBA8888,	 3, 2048 }, // RGBA
		{	GetNone,							GetNone,				 0, 4096 }, // YUV
		{	GetCI8RGBA_RGBA5551,				GetCI8RGBA_RGBA8888,     3, 2048 }, // CI
		{	GetIA44_RGBA4444,					GetIA44_RGBA8888,		 3, 4096 }, // IA
		{	GetI8_RGBA4444,						GetI8_RGBA8888,			 3, 4096 }, // I
	},
	{ // 16-bit
		{	GetRGBA5551_RGBA5551,				GetRGBA5551_RGBA8888,	 2, 2048 }, // RGBA
		{	GetNone,							GetNone,				 2, 2048 }, // YUV
		{	GetNone,							GetNone,				 0, 2048 }, // CI
		{	GetIA88_RGBA4444,					GetIA88_RGBA8888,		 2, 2048 }, // IA
		{	GetNone,							GetNone,				 0, 2048 }, // I
	},
	{ // 32-bit
		{	GetRGBA8888_RGBA4444,				GetRGBA8888_RGBA8888,	 2, 1024 }, // RGBA
		{	GetNone,							GetNone,				 0, 1024 }, // YUV
		{	GetNone,							GetNone,				 0, 1024 }, // CI
		{	GetNone,							GetNone,				 0, 1024 }, // IA
		{	GetNone,							GetNone,				 0, 1024 }, // I
	}
};

int32_t TextureCache_GetTexel(gDPTile *tile, int32_t s, int32_t t) {
    uint32_t line = tile->line;
    if (tile->size == G_IM_SIZ_32b)
        line <<= 1;
    uint64_t *src = &TMEM[tile->tmem] + line * t;
    int32_t i = (t & 1) << 1;
#ifdef VC_TEXTURE_SPEW
    if (tile->format == G_IM_FMT_IA && tile->size == G_IM_SIZ_4b)
        fprintf(stderr, "getting texel (%d,%d) ", (int)s, (int)t);
#endif
    int32_t texel = imageFormat[tile->size][tile->format].Get32(src, s, i, tile->palette);
#ifdef VC_TEXTURE_SPEW
    if (tile->format == G_IM_FMT_IA && tile->size == G_IM_SIZ_4b)
        fprintf(stderr, "\n");
#endif
    return texel;
}

int32_t TextureCache_GetTexelFromBGImage(uint8_t *swapped, int32_t s, int32_t t) {
    uint32_t bpp = TextureCache_SizeToBPP(gSP.bgImage.size);
    uint32_t line = gSP.bgImage.width * bpp / 8;
    uint64_t *src = (uint64_t *)(swapped + line * t);
    return imageFormat[gSP.bgImage.size][gSP.bgImage.format].Get32(src, s, 0, gSP.bgImage.palette);
}

#if 0
void TextureCache_LoadBackground( CachedTexture *texInfo )
{
	u32 *dest, *scaledDest;

	u8 *swapped, *src;
	u32 numBytes, bpl;
	u32 x, y, /*i,*/ j, tx, ty;
	u16 clampSClamp;
	u16 clampTClamp;
	GetTexelFunc	GetTexel;
	GLuint			glInternalFormat;
	GLenum			glType;

	if (((imageFormat[texInfo->size][texInfo->format].autoFormat == GL_RGBA8) || 
		((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16)) || (cache.bitDepth == 2)) && (cache.bitDepth != 0))
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 2;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA8888;
			else
				GetTexel = GetCI8IA_RGBA8888;

			glInternalFormat = GL_RGBA8;
			glType = GL_UNSIGNED_BYTE;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get32;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat32;
			glType = imageFormat[texInfo->size][texInfo->format].glType32;
		}
	}
	else
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 1;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA4444;
			else
				GetTexel = GetCI8IA_RGBA4444;

			glInternalFormat = GL_RGBA4;
			glType = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get16;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat16;
			glType = imageFormat[texInfo->size][texInfo->format].glType16;
		}
	}

	bpl = gSP.bgImage.width << gSP.bgImage.size >> 1;
	numBytes = bpl * gSP.bgImage.height;
	swapped = (u8*)malloc( numBytes );
	UnswapCopy( &RDRAM[gSP.bgImage.address], swapped, numBytes );
	dest = (u32*)malloc( texInfo->textureBytes );

	clampSClamp = texInfo->width - 1;
	clampTClamp = texInfo->height - 1;

	j = 0;
	for (y = 0; y < texInfo->realHeight; y++)
	{
		ty = min(y, clampTClamp);

		src = &swapped[bpl * ty];

		for (x = 0; x < texInfo->realWidth; x++)
		{
			tx = min(x, clampSClamp);

			if (glInternalFormat == GL_RGBA8)
				((u32*)dest)[j++] = GetTexel( (u64*)src, tx, 0, texInfo->palette );
			else
				((u16*)dest)[j++] = GetTexel( (u64*)src, tx, 0, texInfo->palette );
		}
	}

	if (cache.enable2xSaI)
	{
		texInfo->textureBytes <<= 2;

		scaledDest = (u32*)malloc( texInfo->textureBytes );

		if (glInternalFormat == GL_RGBA8)
			_2xSaI8888( (u32*)dest, (u32*)scaledDest, texInfo->realWidth, texInfo->realHeight, texInfo->clampS, texInfo->clampT );
		else if (glInternalFormat == GL_RGBA4)
			_2xSaI4444( (u16*)dest, (u16*)scaledDest, texInfo->realWidth, texInfo->realHeight, texInfo->clampS, texInfo->clampT );
		else
			_2xSaI5551( (u16*)dest, (u16*)scaledDest, texInfo->realWidth, texInfo->realHeight, texInfo->clampS, texInfo->clampT );

		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth << 1, texInfo->realHeight << 1, 0, GL_RGBA, glType, scaledDest );

		free( dest );
		free( scaledDest );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth, texInfo->realHeight, 0, GL_RGBA, glType, dest );
		free( dest );
	}
}

void TextureCache_Load( CachedTexture *texInfo )
{
	u32 *dest, *scaledDest;

	u64 *src;
	u16 x, y, i, j, tx, ty, line;
	u16 mirrorSBit, maskSMask, clampSClamp;
	u16 mirrorTBit, maskTMask, clampTClamp;
	GetTexelFunc	GetTexel;
	GLuint			glInternalFormat;
	GLenum			glType;

	if (((imageFormat[texInfo->size][texInfo->format].autoFormat == GL_RGBA8) || 
		((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16)) || (cache.bitDepth == 2)) && (cache.bitDepth != 0))
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 2;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA8888;
			else
				GetTexel = GetCI8IA_RGBA8888;

			glInternalFormat = GL_RGBA8;
			glType = GL_UNSIGNED_BYTE;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get32;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat32;
			glType = imageFormat[texInfo->size][texInfo->format].glType32;
		}
	}
	else
	{
	 	texInfo->textureBytes = (texInfo->realWidth * texInfo->realHeight) << 1;
		if ((texInfo->format == G_IM_FMT_CI) && (gDP.otherMode.textureLUT == G_TT_IA16))
		{
			if (texInfo->size == G_IM_SIZ_4b)
				GetTexel = GetCI4IA_RGBA4444;
			else
				GetTexel = GetCI8IA_RGBA4444;

			glInternalFormat = GL_RGBA4;
			glType = GL_UNSIGNED_SHORT_4_4_4_4_EXT;
		}
		else
		{
			GetTexel = imageFormat[texInfo->size][texInfo->format].Get16;
			glInternalFormat = imageFormat[texInfo->size][texInfo->format].glInternalFormat16;
			glType = imageFormat[texInfo->size][texInfo->format].glType16;
		}
	}

	dest = (u32*)malloc( texInfo->textureBytes );

	line = texInfo->line;

	if (texInfo->size == G_IM_SIZ_32b)
		line <<= 1;

	if (texInfo->maskS)
	{
		clampSClamp = texInfo->clampS ? texInfo->clampWidth - 1 : (texInfo->mirrorS ? (texInfo->width << 1) - 1 : texInfo->width - 1);
		maskSMask = (1 << texInfo->maskS) - 1;
		mirrorSBit = texInfo->mirrorS ? 1 << texInfo->maskS : 0;
	}
	else
	{
		clampSClamp = min( texInfo->clampWidth, texInfo->width ) - 1;
		maskSMask = 0xFFFF;
		mirrorSBit = 0x0000;
	}

	if (texInfo->maskT)
	{
		clampTClamp = texInfo->clampT ? texInfo->clampHeight - 1 : (texInfo->mirrorT ? (texInfo->height << 1) - 1: texInfo->height - 1);
		maskTMask = (1 << texInfo->maskT) - 1;
		mirrorTBit = texInfo->mirrorT ?	1 << texInfo->maskT : 0;
	}
	else
	{
		clampTClamp = min( texInfo->clampHeight, texInfo->height ) - 1;
		maskTMask = 0xFFFF;
		mirrorTBit = 0x0000;
	}

	// Hack for Zelda warp texture
	if (((texInfo->tMem << 3) + (texInfo->width * texInfo->height << texInfo->size >> 1)) > 4096)
		texInfo->tMem = 0;

	j = 0;
	for (y = 0; y < texInfo->realHeight; y++)
	{
		ty = min(y, clampTClamp) & maskTMask;

		if (y & mirrorTBit)
			ty ^= maskTMask;

		src = &TMEM[texInfo->tMem] + line * ty;

		i = (ty & 1) << 1;
		for (x = 0; x < texInfo->realWidth; x++)
		{
			tx = min(x, clampSClamp) & maskSMask;

			if (x & mirrorSBit)
				tx ^= maskSMask;

			if (glInternalFormat == GL_RGBA8)
				((u32*)dest)[j++] = GetTexel( src, tx, i, texInfo->palette );
			else
				((u16*)dest)[j++] = GetTexel( src, tx, i, texInfo->palette );
		}
	}

	if (cache.enable2xSaI)
	{
		texInfo->textureBytes <<= 2;

		scaledDest = (u32*)malloc( texInfo->textureBytes );

		if (glInternalFormat == GL_RGBA8)
			_2xSaI8888( (u32*)dest, (u32*)scaledDest, texInfo->realWidth, texInfo->realHeight, 1, 1 );//texInfo->clampS, texInfo->clampT );
		else if (glInternalFormat == GL_RGBA4)
			_2xSaI4444( (u16*)dest, (u16*)scaledDest, texInfo->realWidth, texInfo->realHeight, 1, 1 );//texInfo->clampS, texInfo->clampT );
		else
			_2xSaI5551( (u16*)dest, (u16*)scaledDest, texInfo->realWidth, texInfo->realHeight, 1, 1 );//texInfo->clampS, texInfo->clampT );

		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth << 1, texInfo->realHeight << 1, 0, GL_RGBA, glType, scaledDest );

		free( dest );
		free( scaledDest );
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, glInternalFormat, texInfo->realWidth, texInfo->realHeight, 0, GL_RGBA, glType, dest );
		free( dest );
	}
}
#endif


