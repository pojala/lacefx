/*
 *  LXStringUtils.h
 *  Lacefx
 *
 *  Copyright 2008 Lacquer oy/ltd.
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXSTRINGUTILS_H_
#define _LXSTRINGUTILS_H_

#include "LXBasicTypes.h"
#include "LXPool.h"


#ifdef __cplusplus
extern "C" {
#endif

// strdup using the Lacefx allocator -- returned string must be freed with _lx_free()
LXEXPORT LXFUNCATTR_MALLOC char *_lx_strdup(const char *str);

// regular strncmp and strncpy are provided here for platforms like Win32+MinGW which don't have these;
// also char16_t equivalents just in case they're needed in some cross-platform code.
LXEXPORT int _lx_strncmp(const char *s1, const char *s2, size_t n);
LXEXPORT char *_lx_strncpy(char * LXRESTRICT dst, const char * LXRESTRICT src, size_t n);

LXEXPORT int _lx_strncmp_16(const char16_t *s1, const char16_t *s2, size_t n);
LXEXPORT char16_t *_lx_strncpy_16(char16_t * LXRESTRICT dst, const char16_t * LXRESTRICT src, size_t n);


// strdup equivalent for the LXUnibuffer struct type
LXINLINE LXUnibuffer LXStrUnibufferCopy(const LXUnibuffer *uni)
{
    LXUnibuffer copiedUni = { 0, NULL };
    if (uni) {
        copiedUni.numOfChar16 = uni->numOfChar16;
        copiedUni.unistr = (char16_t *) _lx_malloc(uni->numOfChar16 * sizeof(char16_t));
        memcpy(copiedUni.unistr, uni->unistr, uni->numOfChar16 * sizeof(char16_t));
    }
    return copiedUni;
}

// a convenience function that calls _lx_free() on the unistr buffer and sets the string length to 0.
// NOTE: does not free 'unibuffer' itself, just its buffer!
LXEXPORT void LXStrUnibufferDestroy(LXUnibuffer *unibuffer);


// cross-platform wrappers for utf8<->utf16 conversions; implementations use platform-native conversion routines.
//
// created string buffers are null-terminated in both UTF-8 and UTF-16 versions
// (although this is not a requirement for any API in Lacefx that takes UTF-16 strings).
//
// returned objects should always be freed with _lx_free() or LXStrUnibufferDestroy()
// to avoid trouble with different C runtimes across DLL boundaries on Windows.

LXEXPORT LXFUNCATTR_MALLOC char16_t *LXStrCreateUTF16_from_UTF8(const char *utf8Str, size_t utf8Len, size_t *outUTF16Len);

LXEXPORT LXFUNCATTR_MALLOC char *LXStrCreateUTF8_from_UTF16(const char16_t *utf16Str, size_t utf16Len, size_t *outUTF8Len);


// UTF-8 convenience functions that return autoreleased buffers.
// there needs to be a default LXPool in place (use LXPoolCreateForThread() to create it).
// (within Lacefx plugin calls, the host app always provides a pool.)

LXINLINE LXUnibuffer LXStrUnibufferFromUTF8(const char *utf8Str)
{
    LXUnibuffer unibuf = { 0, NULL };
    unibuf.unistr = LXStrCreateUTF16_from_UTF8(utf8Str, strlen(utf8Str), &unibuf.numOfChar16);
    LXAutoreleaseCPtr(unibuf.unistr);
    return unibuf;
}

LXINLINE char *LXStrUTF8FromUnibuffer(LXUnibuffer unibuf)
{
    if ( !unibuf.unistr) return NULL;
    size_t utf8Len = 0;
    char *utf8Str = LXStrCreateUTF8_from_UTF16(unibuf.unistr, unibuf.numOfChar16, &utf8Len);
    LXAutoreleaseCPtr(utf8Str);
    return utf8Str;
}

LXINLINE char16_t *LXStrUnibufferCopyAppendZero(LXUnibuffer unibuf)  // creates a null-terminated UTF16 string suitable for Windows APIs
{
    char16_t *wsz = (char16_t *) _lx_malloc((unibuf.numOfChar16 + 1) * sizeof(char16_t));
    if (unibuf.numOfChar16 > 0)
        memcpy(wsz, unibuf.unistr, unibuf.numOfChar16*sizeof(char16_t));
    wsz[unibuf.numOfChar16] = 0;
    return wsz;
}


// checks an UTF-16 string for a byte-order mark (BOM);
// if found, removes it and swaps the string to native byte order, returning a new malloced string in outbuf.
// the returned string in outBuf should be freed with _lx_free() or LXStrUnibufferDestroy().
//
// returns YES if outBuf was modified, NO if the string didn't need modification.
//
LXEXPORT LXBool LXStrTrimBOMAndSwapToNative(LXUnibuffer *inBuf, LXUnibuffer *outBuf);


// cross-platform path concatenation util; if either argument is NULL, returns NULL.
LXEXPORT char *LXStrCreateUTF8ByAppendingPathComponent(const char *basePath, const char *comp);



// MinGW uses the old MSVC 6 runtime, which doesn't support the C99 "%lld" format string spec for 64-bit ints.
// these are helper functions for converting an int64 to a printable UTF8 string.
// returned string must be destroyed with _lx_free().
//
LXEXPORT char *LXStrCreateFromInt64(int64_t s64);
LXEXPORT char *LXStrCreateFromUint64(uint64_t u64);


#if defined(LXPLATFORM_WIN)
  // printf on MinGW is non-threadsafe and otherwise generally useless,
  // so this call outputs to Win32's standard console handle instead.
  LXEXPORT int LXPrintf(const char * LXRESTRICT, ...);
  
  #define LXPRINTF_SPEC_PREFIX_INT64                  "%I64"  // MinGW uses the old MSVC runtime which only supports this spec
 
#else
  // Mac OS X printf is already thread-safe
  #define LXPrintf(format, args...)                   printf(format , ## args);
  
  #define LXPRINTF_SPEC_PREFIX_INT64                  "%ll"
#endif


#ifdef __cplusplus
}
#endif


#endif
