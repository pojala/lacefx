/*
 *  LXPatternGen.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 13.4.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXPatternGen.h"
#include "LXBasicTypes.h"
#include <math.h>

#ifdef __BIG_ENDIAN__
#define SHR 24
#define SHG 16
#define SHB 8
#define SHA 0
#else
#define SHR 0
#define SHG 8
#define SHB 16
#define SHA 24
#endif


LXPixelBufferRef LXPatternCreateBlank(LXSize size)
{
    LXError err;
    memset(&err, 0, sizeof(LXError));

    LXPixelBufferRef pixBuf = LXPixelBufferCreate(NULL, size.w, size.h, kLX_RGBA_INT8, NULL);
    if ( !pixBuf) {
        printf("** %s failed: %i\n", __func__, err.errorID);
        return NULL;
    }
    
    LXPixelBufferSetPrefersHWCaching(pixBuf, YES);

    int w = size.w;
    int h = size.h;
    size_t rowBytes = 0;
    uint8_t *texData = LXPixelBufferLockPixels(pixBuf, &rowBytes, NULL, NULL);

    if (texData) {
		int x, y;
		for (y = 0; y < h; y++) {
            unsigned int *td = (unsigned int *)(texData + rowBytes * y);
            
            for (x = 0; x < w; x++) {
                *td = (0xff << SHA);  // black with full alpha
                td++;
			}
		}
        
        LXPixelBufferUnlockPixels(pixBuf);
    }

    return pixBuf;
}


LXPixelBufferRef LXPatternCreateGrid(LXSize size)
{
    LXError err;
    memset(&err, 0, sizeof(LXError));

    const int w = round(size.w);
    const int h = round(size.h);

    LXPixelBufferRef pixBuf = LXPixelBufferCreate(NULL, size.w, size.h, kLX_RGBA_INT8, NULL);
    if ( !pixBuf) {
        printf("** %s failed: %i\n", __func__, err.errorID);
        return NULL;
    }
    
    LXPixelBufferSetPrefersHWCaching(pixBuf, YES);

    size_t rowBytes = 0;
    uint8_t *texData = LXPixelBufferLockPixels(pixBuf, &rowBytes, NULL, NULL);

    if (texData) {
		const unsigned int color = 0xffffffff;      // white
		const unsigned int black = (0xff << SHA);   // black w/ full alpha
		const unsigned int gsize = 16;  // grid size in pixels
		int x, y;
		for (y = 0; y < h; y++) {
            unsigned int *td = (unsigned int *)(texData + rowBytes * y);
            
            unsigned int yd = y % gsize;
            
			for (x = 0; x < w; x++) {
				if (yd == 0 || y == h-1 || x == w-1 || x % gsize == 0)
					*td = color;
				else
					*td = black;
				td++;
			}
		}
        
        LXPixelBufferUnlockPixels(pixBuf);
    }

    return pixBuf;
}


static LXBool isPowerOf2(LXInteger v)
{
    LXInteger n = 0;
    LXInteger i;
    for (i = 0; i < 32; i++) {
        if (v >> i)  n++;
        if (n > 1)   break;
    }
    return (n <= 1) ? YES : NO;
}


LXPixelBufferRef LXPatternCreateGridWithCellSizeAndLineWidth(LXSize size, const int gsize, const int linew)
{
    LXError err;
    memset(&err, 0, sizeof(LXError));

    LXPixelBufferRef pixBuf = LXPixelBufferCreate(NULL, size.w, size.h, kLX_RGBA_INT8, NULL);
    if ( !pixBuf) {
        printf("** %s failed: %i\n", __func__, err.errorID);
        return NULL;
    }
    
    LXPixelBufferSetPrefersHWCaching(pixBuf, YES);

    LXInteger w = size.w;
    LXInteger h = size.h;
    size_t rowBytes = 0;
    uint8_t *texData = LXPixelBufferLockPixels(pixBuf, &rowBytes, NULL, NULL);
    
    if (texData) {
        const unsigned int color = 0xffffffff;      // white
        const unsigned int black = (0xff << SHA);   // black w/ full alpha
        LXUInteger x, y;
        if (isPowerOf2(gsize)) {
            const unsigned int gmask = (gsize - 1);  // gsize must be a power of 2
            for (y = 0; y < h; y++) {
                unsigned int *td = (unsigned int *)(texData + rowBytes * y);
                unsigned int ym = y & gmask;
                
                for (x = 0; x < w; x++) {
                    unsigned int xm = x & gmask;
                    
                    if (ym < linew || xm < linew)
                        *td = color;
                    else
                        *td = black;
                    td++;
                }
            }
        } else {
            for (y = 0; y < h; y++) {
                unsigned int *td = (unsigned int *)(texData + rowBytes * y);
                LXBool ym = y % gsize;
                
                for (x = 0; x < w; x++) {
                    unsigned int xm = x % gsize;
                    
                    if (ym < linew || xm < linew)
                        *td = color;
                    else
                        *td = black;
                    td++;
                }
            }            
        }
        LXPixelBufferUnlockPixels(pixBuf);
    }

    return pixBuf;
}

LXINLINE uint8_t LXClampF_to_u8(LXFloat f)
{
    f = MIN(1.0, MAX(0.0, f));
    f *= 255.0;
    return (uint8_t)f;
}

LXINLINE uint32_t LXRGBA_to_u32(LXRGBA rgba)
{
    uint32_t c = 0;
    c |= LXClampF_to_u8(rgba.r) << SHR;
    c |= LXClampF_to_u8(rgba.g) << SHG;
    c |= LXClampF_to_u8(rgba.b) << SHB;
    c |= LXClampF_to_u8(rgba.a) << SHA;
    return c;
}

LXPixelBufferRef LXPatternCreateCheckerboardWithTileSizeAndColors(LXSize size, const unsigned int tilesize, LXRGBA rgba1, LXRGBA rgba2)
{
    LXError err;
    memset(&err, 0, sizeof(LXError));

    const int w = round(size.w);
    const int h = round(size.h);

    LXPixelBufferRef pixBuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, NULL);
    if ( !pixBuf) {
        printf("** %s failed: err %i (req size %i * %i)\n", __func__, err.errorID, w, h);
        return NULL;
    }

    LXPixelBufferSetPrefersHWCaching(pixBuf, YES);

    size_t rowBytes = 0;
    uint8_t *texData = LXPixelBufferLockPixels(pixBuf, &rowBytes, NULL, NULL);

    if (texData) {
		const unsigned int color1 = LXRGBA_to_u32(rgba1);
		const unsigned int color2 = LXRGBA_to_u32(rgba2);
		unsigned int x, y;
		for (y = 0; y < h; y++) {
            unsigned int *td = (unsigned int *)(texData + rowBytes * y);
        
            unsigned int ym = (y / tilesize) % 2;
            
			for (x = 0; x < w; x++) {
                unsigned int xm = (x / tilesize) % 2;
                
                *td = ((ym && !xm) || (xm && !ym)) ? color2 : color1;
				td++;
			}
		}
        
        LXPixelBufferUnlockPixels(pixBuf);
    }

    return pixBuf;
}


LXPixelBufferRef LXPatternCreateGradientColorBars(LXSize size)
{
    LXError err;
    memset(&err, 0, sizeof(LXError));

    LXPixelBufferRef pixBuf = LXPixelBufferCreate(NULL, size.w, size.h, kLX_RGBA_INT8, NULL);
    if ( !pixBuf) {
        printf("** %s failed: %i\n", __func__, err.errorID);
        return NULL;
    }
    
    LXPixelBufferSetPrefersHWCaching(pixBuf, YES);

    int w = size.w;
    int h = size.h;
    size_t rowBytes = 0;
    uint8_t *texData = LXPixelBufferLockPixels(pixBuf, &rowBytes, NULL, NULL);

    if (texData) {
		const unsigned int CLRVAL = 255;
		const unsigned int white = 0xffffffff;
		const unsigned int black = (0xff << SHA);
		const unsigned int yellow =  (0xff << SHA) | (CLRVAL << SHR) | (CLRVAL << SHG);
		const unsigned int cyan =    (0xff << SHA) | (CLRVAL << SHG) | (CLRVAL << SHB);
		const unsigned int green =   (0xff << SHA) | (CLRVAL << SHG);
		const unsigned int magenta = (0xff << SHA) | (CLRVAL << SHR) | (CLRVAL << SHB);
		const unsigned int red   =   (0xff << SHA) | (CLRVAL << SHR);
		const unsigned int blue  =   (0xff << SHA) | (CLRVAL << SHB);
		unsigned int colors[8] = { white, yellow, cyan, green,  magenta, red, blue, black };
		
		//const int gsize = 16;
		int x, y, n;
		for (y = 0; y < h; y++) {
            unsigned int *td = (unsigned int *)(texData + rowBytes * y);        
        
			const double m = ((h-1 - y) / (double)(h-1));
			for (n = 0; n < 8; n++) {
				unsigned int cval = colors[n];
				cval =	((unsigned int)(((cval >> SHR) & 0xff) * m) << SHR) |
						((unsigned int)(((cval >> SHG) & 0xff) * m) << SHG) |
						((unsigned int)(((cval >> SHB) & 0xff) * m) << SHB) |
						(0xff << SHA);
				
				for (x = 0; x < w/8; x++) {
					*td = cval;
					td++;
				}
			}
		}
        
        LXPixelBufferUnlockPixels(pixBuf);
    }

    return pixBuf;
}


LXPixelBufferRef LXPatternCreate75PercentColorBars(LXSize size)
{
    LXError err;
    memset(&err, 0, sizeof(LXError));

    LXPixelBufferRef pixBuf = LXPixelBufferCreate(NULL, size.w, size.h, kLX_RGBA_INT8, NULL);
    if ( !pixBuf) {
        printf("** %s failed: %i\n", __func__, err.errorID);
        return NULL;
    }

    LXPixelBufferSetPrefersHWCaching(pixBuf, YES);

    int w = size.w;
    int h = size.h;
    size_t rowBytes = 0;
    uint8_t *texData = LXPixelBufferLockPixels(pixBuf, &rowBytes, NULL, NULL);

    if (texData) {
		const unsigned int CLRVAL = 192;   // 75% of 255
		const unsigned int white = 0xffffffff;
		const unsigned int black = (0xff << SHA);
		const unsigned int yellow =  (0xff << SHA) | (CLRVAL << SHR) | (CLRVAL << SHG);
		const unsigned int cyan =    (0xff << SHA) | (CLRVAL << SHG) | (CLRVAL << SHB);
		const unsigned int green =   (0xff << SHA) | (CLRVAL << SHG);
		const unsigned int magenta = (0xff << SHA) | (CLRVAL << SHR) | (CLRVAL << SHB);
		const unsigned int red   =   (0xff << SHA) | (CLRVAL << SHR);
		const unsigned int blue  =   (0xff << SHA) | (CLRVAL << SHB);
		unsigned int colors[8] = { white, yellow, cyan, green,  magenta, red, blue, black };
		
		//const int gsize = 16;
		int x, y, n;
		for (y = 0; y < h; y++) {
            unsigned int *td = (unsigned int *)(texData + rowBytes * y);        
        
			for (n = 0; n < 8; n++) {
				for (x = 0; x < w/8; x++) {
					*td = colors[n];
					td++;
				}
			}
		}  
        
        LXPixelBufferUnlockPixels(pixBuf);
    }

    return pixBuf;
}


