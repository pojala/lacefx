/*
 *  LXPixelBuffer_winimage.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 9.9.2007.
 *  Copyright 2007 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXBasicTypes.h"
#include "LXMutex.h"
#include "LXPixelBuffer.h"
#include "LXPixelBuffer_priv.h"
#include "LXStringUtils.h"
#include "LXImageFunctions.h"
#include <png.h>


// BMP loader, located in LXPixelBuffer_bmp.c
extern uint8_t *stbi_bmp_load_from_memory(const uint8_t *buffer, int len, int *x, int *y, int *comp, int req_comp);



char *LXStrCreateUTF8_from_UTF16(const char16_t *utf16Str, size_t utf16Len, size_t *outUTF8Len)
{
    if (outUTF8Len) *outUTF8Len = 0;
    
    // MSDN says: "WideCharToMultiByte does not null-terminate an output string if the input string length is explicitly specified without a terminating null character."
    // --> the returned string len will not include the null terminator, so add one
    size_t newSizeInChar = WideCharToMultiByte(CP_UTF8, 0,  utf16Str, utf16Len,  NULL, 0,  NULL, NULL);
    
    if (newSizeInChar < 1)
        return NULL;
    
    newSizeInChar++;
    
    char *newBuf = (char *) _lx_malloc(newSizeInChar);
    
    int result = WideCharToMultiByte(CP_UTF8, 0,  utf16Str, utf16Len,  newBuf, newSizeInChar,  NULL, NULL);
    
    if (result > 0) {
        newBuf[newSizeInChar-1] = 0;
    
        if (outUTF8Len) *outUTF8Len = strlen(newBuf);
        return newBuf;
    } else {
        LXPrintf("*** %s: failed (srclen %i, error: %i)\n", __func__, utf16Len, GetLastError());
        _lx_free(newBuf);
        return NULL;
    }
}

char16_t *LXStrCreateUTF16_from_UTF8(const char *utf8Str, size_t utf8Len, size_t *outUTF16Len)
{
    if (outUTF16Len) *outUTF16Len = 0;

    size_t newSizeInChar16 = MultiByteToWideChar(CP_UTF8, 0,  utf8Str, utf8Len,  NULL, 0);
    
    if (newSizeInChar16 < 1)
        return NULL;
    
    newSizeInChar16++;
    
    char16_t *newBuf = (char16_t *) _lx_malloc(newSizeInChar16 * sizeof(char16_t));
    
    int result = MultiByteToWideChar(CP_UTF8, 0,  utf8Str, utf8Len,  newBuf, newSizeInChar16);
    
    if (result > 0) {
        newBuf[newSizeInChar16-1] = 0;
        
        if (outUTF16Len) *outUTF16Len = wcslen(newBuf);
        //if (outUTF16Len) *outUTF16Len = newSizeInChar16 - 1;  // exclude null terminator from count
        return newBuf;
    } else {
        LXPrintf("*** %s: failed (srclen %i, error: %i)\n", __func__, utf8Len, GetLastError());
        _lx_free(newBuf);
        return NULL;
    }
}


///char *LXStrCreateUTF8_from_UTF16(const char16_t *utf16Str, size_t utf16Len, size_t *outUTF8Len);


// Win32 implementation of this platform-independent wrapper
LXSuccess LXOpenFileForReadingWithUnipath(const char16_t *pathBuf, size_t pathLen, LXFilePtr *outFile)
{
	FILE *file = NULL;
    LXSuccess retVal = NO;
	const wchar_t rbWstr[3] = { 'r', 'b', 0 };

    // ensure that we have a char16 zero at the end of the buffer
    char16_t *tempBuf = (char16_t *) _lx_calloc(pathLen + 1, sizeof(char16_t));
    memcpy(tempBuf,  pathBuf,  pathLen * sizeof(char16_t));

	if (NULL != (file = _wfopen(tempBuf, rbWstr))) {
        retVal = YES;
    }
    _lx_free(tempBuf);

    if (outFile) *outFile = file;
    return retVal;
}

LXSuccess LXOpenFileForWritingWithUnipath(const char16_t *pathBuf, size_t pathLen, LXFilePtr *outFile)
{
	FILE *file = NULL;
    LXSuccess retVal = NO;
	const wchar_t wbWstr[3] = { 'w', 'b', 0 };

    // ensure that we have a char16 zero at the end of the buffer
    char16_t *tempBuf = (char16_t *) _lx_calloc(pathLen + 1, sizeof(char16_t));
    memcpy(tempBuf,  pathBuf,  pathLen * sizeof(char16_t));

	if (NULL != (file = _wfopen(tempBuf, wbWstr))) {
        retVal = YES;
    }
    _lx_free(tempBuf);

    if (outFile) *outFile = file;
    return retVal;
}



enum {
	err_cantOpen = 1,
	err_cantRead = 2,
	err_noData = 3,
	err_unknown = 1024	
};



static int LoadBMPImage(wchar_t *filepath, size_t filePathLen,
                                              uint8_t **outBuf,
											  int *outW, int *outH, size_t *outRowBytes, int *outNumCh)
{
	FILE *file = NULL;
	size_t n = 0;
    
    if ( !LXOpenFileForReadingWithUnipath(filepath, filePathLen, (LXFilePtr *)&file))
        return err_cantOpen;

   	fseek(file, 0, SEEK_END);
	size_t fileLen = ftell(file);
	uint8_t *fileData = (uint8_t *)_lx_malloc(fileLen);
	
	//LXPrintf("going to read BMP, filebytes: %u\n", fileLen);
	
	fseek(file, 0, SEEK_SET);
	n = fread(fileData, 1, fileLen, file);
	fclose(file);
	file = NULL;
	if (n != fileLen || n == 0)
		return err_cantRead;

	int w = 0, h = 0, comp = 0;
	
	uint8_t *imageBuf = stbi_bmp_load_from_memory(fileData, (int)fileLen, &w, &h, &comp, 4 /*STBI_rgb_alpha*/);
	
	//LXPrintf("loaded BMP: %p, %i * %i, %i\n", imageBuf, w, h, comp);
	
	_lx_free(fileData);
	fileData = NULL;
	
	if ( !imageBuf)
		return err_noData;
	
	*outBuf = (char *)imageBuf;
	*outW = w;
	*outH = h;
	*outRowBytes = w*4;
	*outNumCh = 4;
		
	return 0;
}


static int LoadPNGImage(const wchar_t *filepath, size_t filePathLen,
                                                uint8_t **outBuf,
											    int *outW, int *outH, size_t *outRowBytes, int *outNumCh)
{
	FILE *fp = NULL;
    
    if ( !LXOpenFileForReadingWithUnipath(filepath, filePathLen, (LXFilePtr *)&fp))
        return err_cantOpen;

    png_structp pngref;
    png_infop pnginfo;
    unsigned int sig_read = 0;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
      
    pngref = png_create_read_struct(PNG_LIBPNG_VER_STRING,  NULL, NULL, NULL);
   									//png_voidp user_error_ptr, user_error_fn, user_warning_fn);

   if (pngref == NULL) {
      fclose(fp);
      return err_cantRead;
   }

   pnginfo = png_create_info_struct(pngref);
   if (pnginfo == NULL) {
      png_destroy_read_struct(&pngref, NULL, NULL);
      fclose(fp);
      return err_cantRead;
   }

   // error handler stuff
   if (setjmp(png_jmpbuf(pngref)))
   {
      png_destroy_read_struct(&pngref, &pnginfo, NULL);
      fclose(fp);
      return err_cantRead;
   }

   /* Set up the input control if you are using standard C streams */
   png_init_io(pngref, fp);

   /* If we have already read some of the signature */
   png_set_sig_bytes(pngref, sig_read);

   png_read_png(pngref, pnginfo, PNG_TRANSFORM_IDENTITY, NULL);

   int w = pnginfo->width;
   int h = pnginfo->height;
   int rowb = pnginfo->rowbytes;
   int numCh = pnginfo->channels;
   int bpp = pnginfo->pixel_depth;
   int dstRowb = w * 4;
   
   //LXPrintf("%s, rowbytes %i, nc %i\n", __func__, rowb, numCh);
   
   if (outW) *outW = w;
   if (outH) *outH = h;
   if (outRowBytes) *outRowBytes = dstRowb;
   if (outNumCh) *outNumCh = numCh;
   if (outBuf && pnginfo->row_pointers) {
		*outBuf = (char *) _lx_malloc(dstRowb * h);
	   
        int x, y;
	   	if (numCh == 4) {
	  		for (y = 0; y < h; y++) {
                ///memcpy((*outBuf) + y*dstRowb, pnginfo->row_pointers[y], MIN(dstRowb, rowb));
		   		uint8_t *src = pnginfo->row_pointers[y];
		   		uint8_t *dst = (uint8_t *)(*outBuf) + y*dstRowb;
		   		for (x = 0; x < w; x++) {
			   		dst[0] = src[0];
			   		dst[1] = src[1];
			   		dst[2] = src[2];
			   		dst[3] = src[3];
			   		src += 4;
			   		dst += 4;
		   		}
            }
   		}
   		else if (numCh == 3) {
	   		for (y = 0; y < h; y++) {
		   		uint8_t *src = pnginfo->row_pointers[y];
		   		uint8_t *dst = (uint8_t *)(*outBuf) + y*dstRowb;
		   		for (x = 0; x < w; x++) {
			   		dst[0] = src[0];
			   		dst[1] = src[1];
			   		dst[2] = src[2];
			   		dst[3] = 255;  // alpha
			   		src += 3;
			   		dst += 4;
		   		}
	   		}
   		}        
    } else {
   		printf("** png outbuf missing (%p, %p)\n", outBuf, pnginfo->row_pointers);
	}
   
   /* clean up after the read, and free any memory allocated - REQUIRED */
   png_destroy_read_struct(&pngref, &pnginfo, NULL);

   /* close the file */
   fclose(fp);
   return 0;   
}


#ifdef __COCOTRON__
HANDLE g_lacqitDLL = NULL;
LXBool g_didTryLoadLacqit = NO;
#endif

typedef LXPixelBufferRef(*loadImageFromUnipathFuncPtr)(LXUnibuffer *unipath, LXMapPtr properties, LXError *outError);


LXPixelBufferRef LXPixelBufferCreateFromPathUsingNativeAPI_(LXUnibuffer unipath, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
	int w = 0;
	int h = 0;
	size_t srcRowBytes = 0;
	int numCh = 0;
	uint8_t *srcBuf = NULL;
	int result = 0;
    
    // by default, Lacefx image APIs are expected to return premultiplied alpha (like Mac OS X).
    // PNG files are not premultiplied, so it's done here. it can be disabled by passing this property.
    LXBool useUnpremultAlpha = NO;
    if (properties) {
        LXMapGetBool(properties, "leaveAlphaUnpremultiplied", &useUnpremultAlpha);
    }
    
    wchar_t *pathBuf = (wchar_t *)unipath.unistr;
    const size_t pathLen = unipath.numOfChar16;

	switch (imageType) {
		case kLXImage_PNG:      result = LoadPNGImage(pathBuf, pathLen,  &srcBuf, &w, &h, &srcRowBytes, &numCh);   break;
		case kLXImage_BMP:      result = LoadBMPImage(pathBuf, pathLen,  &srcBuf, &w, &h, &srcRowBytes, &numCh);   break;
        
#ifdef __COCOTRON__
        case kLXImage_JPEG:
        case kLXImage_TIFF: {
            if ( !g_lacqitDLL && !g_didTryLoadLacqit) {
                g_lacqitDLL = LoadLibraryA("Lacqit.1.0_ct.dll");
                g_didTryLoadLacqit = YES;
                if ( !g_lacqitDLL) {
                    LXPrintf("** %s: unable to load Lacqit library, can't load image types through it", __func__);
                    result = 1628;
                }
            }
            if (g_lacqitDLL) {
                loadImageFromUnipathFuncPtr loadImageFunc = (loadImageFromUnipathFuncPtr)GetProcAddress(g_lacqitDLL, "LXPixelBufferCreateFromPathUsingAppKit_");
                if ( !loadImageFunc) {
                    LXPrintf("** %s: unable to get function address from Lacqit library", __func__);
                    result = 1628;
                } else {
                    return loadImageFunc(&unipath, properties, outError);
                }
            }
            
            break;
        }
#endif

        default:  result = 1629;  break;
	}
	
	if (result != 0 || !srcBuf) {
		if (result == err_cantOpen) {
        	LXErrorSet(outError, 1621, "couldn't open image file");
    	} else if (result == err_cantRead) {
        	LXErrorSet(outError, 1622, "couldn't read image data");
    	} else if (result == 1628) {
        	LXErrorSet(outError, result, "could not open dynamic library needed for loading images");
    	} else if (result == 1629) {
        	LXErrorSet(outError, result, "unsupported image format");
        } else {
        	LXErrorSet(outError, 1623, "unknown error reading image file");
    	}
        return NULL;
    }
    
    printf("%s -- did load lxpixbuf image: %i * %i, nc %i, type %i, %p\n", __func__, w, h, numCh, imageType, srcBuf);
    fflush(stdout);
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
    if ( !newPixbuf)
        return NULL;

    size_t dstRowBytes;
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);
    LXInteger y;
    
    if (useUnpremultAlpha) {
        for (y = 0; y < h; y++) {
            uint8_t *src = srcBuf + srcRowBytes * y;
            uint8_t *dst = dstBuf + dstRowBytes * y;
            memcpy(dst, src, MIN(srcRowBytes, dstRowBytes));    
        }
    } else {
        LXImagePremultiply_RGBA_int8(w, h, srcBuf, srcRowBytes, dstBuf, dstRowBytes);
    }
    
    LXPixelBufferUnlockPixels(newPixbuf);
    
    _lx_free(srcBuf);
    
    return newPixbuf;
}

LXPixelBufferRef LXPixelBufferCreateFromFileDataUsingNativeAPI_(const uint8_t *data, size_t len, LXInteger imageType, LXMapPtr properties, LXError *outError)
{
    return NULL;  // TODO: implement this
}

LXSuccess LXPixelBufferWriteToPathUsingNativeAPI_(LXPixelBufferRef pixbuf, LXUnibuffer unipath, LXUInteger lxImageType, LXMapPtr properties, LXError *outError)
{
    return NO;  // TODO: implement something sensible here (at least PNG should be handled)
}

LXSuccess LXCopyICCProfileDataForColorSpaceEncoding(LXColorSpaceEncoding colorSpaceID, uint8_t **outData, size_t *outDataLen)
{
    return NO;  // TODO: implement loading from resources (the necessary .icc files are available in the Lacefx bundle)
}


// ------------ old implementation ------------
#if (0)

enum {
	PNGImage = 1,
	BMPImage = 2
};

static int getImageTypeFromUnipath(const char16_t *unipath, size_t len)
{
	int retVal = 1;
    size_t n = len;
	const char16_t *ptr = unipath + n - 1;
	const int EXTLEN = 15;
	char ext[16] = "";
	
	while (*ptr == 0 && (--n) > 0)
		ptr--;

	
	if (n == 0)
		return retVal;
	
	unsigned int i;
	size_t lim = (n > EXTLEN-1) ? 0 : EXTLEN-1-n;
	for (i = EXTLEN-1; i >= lim; i--) {
		ext[i] = (*(ptr--)) & 127;
		if (ext[i] == '.') {
			i++;
			break;
		}
	}

	//printf("got ext: '%s'\n", ext+i);
		
	if (0 == strcmp(ext+i, "bmp"))
		return 2;
	else
		return 1;
}


LXPixelBufferRef LXPixelBufferCreateFromFileAtPath(LXUnibuffer uni,
                                                   LXError *outError)
{
	if ( !uni.unistr || uni.numOfChar16 < 1) {	
        LXErrorSet(outError, 1699, "empty path given");
        return NULL;
    }
	
	int w = 0;
	int h = 0;
	size_t srcRowBytes = 0;
	int numCh = 0;
	uint8_t *srcBuf = NULL;
	int result = 0;

	// find extension
	int imageType = getImageTypeFromUnipath(uni.unistr, uni.numOfChar16);

	// ensure that we have a wchar zero at the end of the buffer
	wchar_t *nameBuffer = (wchar_t *)_lx_calloc(uni.numOfChar16+1, sizeof(char16_t));
	memcpy(nameBuffer, uni.unistr, uni.numOfChar16 * sizeof(char16_t));
	
	switch (imageType) {
		case PNGImage:  result = LoadPNGImage((wchar_t *)nameBuffer, (uint8_t **)&srcBuf, &w, &h, &srcRowBytes, &numCh);  break;
		case BMPImage:  result = LoadBMPImage((wchar_t *)nameBuffer, (uint8_t **)&srcBuf, &w, &h, &srcRowBytes, &numCh);  break;
	}
	
	_lx_free(nameBuffer);

	if (result != 0 || !srcBuf) {
		if (result == err_cantOpen) {
        	LXErrorSet(outError, 1621, "couldn't open image file");
    	} else if (result == err_cantRead) {
        	LXErrorSet(outError, 1622, "couldn't read image data");
    	} else {
        	LXErrorSet(outError, 1623, "unknown error reading image file");
    	}
        return NULL;
    }

    LXPrintf("..loaded lxpixbuf image: %i * %i, %i,  %p\n", w, h, numCh, srcBuf);
    
    LXPixelBufferRef newPixbuf = LXPixelBufferCreate(NULL, w, h, kLX_RGBA_INT8, outError);
    if ( !newPixbuf)
        return NULL;

    size_t dstRowBytes;
    uint8_t *dstBuf = (uint8_t *) LXPixelBufferLockPixels(newPixbuf, &dstRowBytes, NULL, NULL);
    int y;
    
        for (y = 0; y < h; y++) {
            uint8_t *src = srcBuf + srcRowBytes * y;
            uint8_t *dst = dstBuf + dstRowBytes * y;
            
            memcpy(dst, src, MIN(srcRowBytes, dstRowBytes));    
        }            
    
    LXPixelBufferUnlockPixels(newPixbuf);
    
    _lx_free(srcBuf);
    
    return newPixbuf;
}

#endif
// ^^^^^ ------------ old implementation ------------
