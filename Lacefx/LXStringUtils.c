/*
 *  LXStringUtils.c
 *  Lacefx
 *
 *  Copyright 2008 Lacquer oy/ltd. 
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXStringUtils.h"
#include "LXBasicTypes.h"

#if defined(LXPLATFORM_WIN)
 #include <malloc.h>
 #define ALIGNMENT 16
 
 #if defined(__MSVC__) || (defined(__MSVCRT_VERSION__) && (__MSVCRT_VERSION__ >= 0x0700))
  #define ALIGNED_MALLOC  _aligned_malloc
  #define ALIGNED_REALLOC _aligned_realloc
  #define ALIGNED_FREE    _aligned_free
 #else
  #define ALIGNED_MALLOC  __mingw_aligned_malloc
  #define ALIGNED_REALLOC __mingw_aligned_realloc
  #define ALIGNED_FREE    __mingw_aligned_free
 #endif
#endif


void *_lx_malloc(size_t size)
{
    if (size == 0) return NULL;
    
  #if defined(ALIGNED_MALLOC)
    return ALIGNED_MALLOC(size, ALIGNMENT);
  #else
  
    return malloc(size);
  #endif
}

void *_lx_calloc(size_t count, size_t size)
{
    if (size == 0 || count == 0) return NULL;
    
  #if defined(ALIGNED_MALLOC)
    size_t realSize = count*size;
    void *buf = ALIGNED_MALLOC(realSize, ALIGNMENT);
    memset(buf, 0, realSize);
    return buf;
  #else
  
    return calloc(count, size);
  #endif
}

void *_lx_realloc(void *ptr, size_t size)
{
    if ( !ptr) return _lx_malloc(size);

  #if defined(ALIGNED_MALLOC)
    return ALIGNED_REALLOC(ptr, size, ALIGNMENT);
  #else
  
    return realloc(ptr, size);
  #endif
}

void _lx_free(void *ptr)
{
  #if defined(ALIGNED_MALLOC)
    if (ptr) ALIGNED_FREE(ptr);
    
  #else
    if (ptr) free(ptr);
  #endif
}

char *_lx_strdup(const char *str)
{
    if ( !str) return NULL;
    
    size_t len = strlen(str);
    char *ns = _lx_malloc(len+1);
    
    if (len > 0)
        memcpy(ns, str, len);
    ns[len] = 0;
    return ns;
}


#if defined(__SSE2__)


LXFUNCATTR_SSE static void _aligned_memcpy_sse2(void * LXRESTRICT dst, const void * LXRESTRICT src, const unsigned long size);



static void _aligned_memcpy_sse2(void *adst, const void *asrc, const unsigned long size)
{
#if defined(__MSVC__)
  __asm {
    mov esi, asrc;    // src pointer
    mov edi, adst;    // dst pointer
    mov ecx, size;   // counter
    shr ecx, 7;      // 128 bytes at once

    loop:
      prefetchnta 128[ESI];  // SSE2 prefetch
      prefetchnta 160[ESI];
      prefetchnta 192[ESI];
      prefetchnta 224[ESI];

      movdqa xmm0, 0[ESI];
      movdqa xmm1, 16[ESI];
      movdqa xmm2, 32[ESI];
      movdqa xmm3, 48[ESI];
      movdqa xmm4, 64[ESI];
      movdqa xmm5, 80[ESI];
      movdqa xmm6, 96[ESI];
      movdqa xmm7, 112[ESI];

      movntdq 0[EDI], xmm0;
      movntdq 16[EDI], xmm1;
      movntdq 32[EDI], xmm2;
      movntdq 48[EDI], xmm3;
      movntdq 64[EDI], xmm4;
      movntdq 80[EDI], xmm5;
      movntdq 96[EDI], xmm6;
      movntdq 112[EDI], xmm7;

      add esi, 128;
      add edi, 128;
      dec ecx;
      jnz loop;
  }

#else
    void * LXRESTRICT dst = adst;
    void * LXRESTRICT src = (void *)asrc;
    unsigned long counter = size;    
    __asm__(
"shr $7, %%ecx; \n"
"loop: \n"

"     prefetchnta 128(%%esi);"
"     prefetchnta 160(%%esi);"
"     prefetchnta 192(%%esi);"
"     prefetchnta 224(%%esi);"

"     movdqa 0(%%esi), %%xmm0;"
"     movdqa 16(%%esi), %%xmm1;"
"     movdqa 32(%%esi), %%xmm2;"
"     movdqa 48(%%esi), %%xmm3;"
"     movdqa 64(%%esi), %%xmm4;"
"     movdqa 80(%%esi), %%xmm5;"
"     movdqa 96(%%esi), %%xmm6;"
"     movdqa 112(%%esi), %%xmm7;"

"     movntdq %%xmm0, 0(%%edi);"
"     movntdq %%xmm1, 16(%%edi);"
"     movntdq %%xmm2, 32(%%edi);"
"     movntdq %%xmm3, 48(%%edi);"
"     movntdq %%xmm4, 64(%%edi);"
"     movntdq %%xmm5, 80(%%edi);"
"     movntdq %%xmm6, 96(%%edi);"
"     movntdq %%xmm7, 112(%%edi);"

"     add $128, %%esi;"
"     add $128, %%edi;"
"     dec %%ecx;"
"     jnz loop; \n"
        : /* outputs */ "=S" (src), "=D" (dst), "=c" (counter)
        : /* inputs */  "S" (src), "D" (dst), "c" (counter)
        : /* clobber list */ "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
        );

#endif
}

#endif // SSE2


void *_lx_memcpy_aligned(void * LXRESTRICT dst, const void * LXRESTRICT src, size_t n)
{
#if 0 && defined(__SSE2__) && !defined(__APPLE__)
    unsigned long vecn = (n >> 7) * 128;
    
    _aligned_memcpy_sse2(dst, src, vecn);
    
    if (n > vecn) {
        memcpy(dst + vecn, src + vecn, n - vecn);
    }
    
#else
    memcpy(dst, src, n);
#endif
    return dst;
}


// ---- 

void LXStrUnibufferDestroy(LXUnibuffer *unibuffer)
{
    if ( !unibuffer) return;
    
    _lx_free(unibuffer->unistr);
    
    unibuffer->unistr = NULL;
    unibuffer->numOfChar16 = 0;
}


LXBool LXStrTrimBOMAndSwapToNative(LXUnibuffer *inBuf, LXUnibuffer *outBuf)
{
    if ( !inBuf || !outBuf)
        return NO;
        
	unsigned int i;
    char16_t *tempStr = NULL;
    //LXBool didSkipBOM = NO;
    //LXBool didSwapBytes = NO;

    uint32_t lenInBytes = inBuf->numOfChar16 * 2;
    char16_t *unistr = inBuf->unistr;

    if (lenInBytes > 2 && unistr[0] == kBOM_UTF16) {  // BOM is native byte order, just skip it
        lenInBytes -= 2;
        unistr++;  // skip past BOM
        //didSkipBOM = YES;
        
        tempStr = (char16_t *) _lx_malloc(lenInBytes);
        memcpy(tempStr, unistr, lenInBytes);
    }
    else if (lenInBytes > 2 && unistr[0] == kBOM_UTF16_SWAPPED) {  // must swap byte order
        lenInBytes -= 2;
        unistr++;
        //didSwapBytes = YES;

        tempStr = (char16_t *) _lx_malloc(lenInBytes);
        
		{
        char *s = (char *)unistr;
        char *d = (char *)tempStr;
        for (i = 0; i < lenInBytes; i+=2) {
            d[0] = s[1];
            d[1] = s[0];
            d += 2;
            s += 2;
        }
		}
    }
	
    if (tempStr) {
        *outBuf = LXMakeUnibuffer(lenInBytes / 2, tempStr);
        return YES;
    } else {
        return NO;
    }
}


// ---- implementations for library functions missing from old MSVC runtime ---

int _lx_strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0) return 0;
    
    do {
        if (*s1 != *s2++) {
            return (*(unsigned char *)s1 - *(unsigned char *)(s2 - 1));
        }
        if (*s1++ == 0) {
            break;
        }
    }
    while (--n != 0);
    return 0;
}

char *_lx_strncpy(char * LXRESTRICT d, const char * LXRESTRICT s, size_t n)
{
    char *retval = d;
    
    if (n == 0) return retval;
    
    do {
        if ((*d++ = *s++) == 0) {
            while (--n != 0) {
                *d++ = 0;
            }
            break;
        }
    }
    while (--n != 0);
    return retval;
}


int _lx_strncmp_16(const char16_t *s1, const char16_t *s2, size_t n)
{
    if (n == 0) return 0;
    
    do {
        if (*s1 != *s2++) {
            return (*(unsigned short *)s1 - *(unsigned short *)(s2 - 1));
        }
        if (*s1++ == 0) {
            break;
        }
    }
    while (--n != 0);
    return 0;
}

char16_t *_lx_strncpy_16(char16_t * LXRESTRICT d, const char16_t * LXRESTRICT s, size_t n)
{
    char16_t *retval = d;

    if (n == 0) return retval;
    
    do {
        if ((*d++ = *s++) == 0) {
            while (--n != 0) {
                *d++ = 0;
            }
            break;
        }
    }
    while (--n != 0);
    return retval;
}


char *LXStrCreateFromInt64(int64_t s64)
{
    char *buf = _lx_malloc(64);
    sprintf(buf, "" LXPRINTF_SPEC_PREFIX_INT64 "d", s64);
    return buf;
}

char *LXStrCreateFromUint64(uint64_t u64)
{
    char *buf = _lx_malloc(64);
    sprintf(buf, "" LXPRINTF_SPEC_PREFIX_INT64 "u", u64);
    return buf;
}


char *LXStrCreateUTF8ByAppendingPathComponent(const char *basePath, const char *comp)
{
    char sep = '/';
#if defined(LXPLATFORM_WIN)
    sep = '\\';
#endif

    if ( !basePath || !comp) return NULL;
    size_t baseLen = strlen(basePath);
    size_t compLen = strlen(comp);
    
    size_t totalLen = baseLen + 1 + compLen;
    
    char *buf = _lx_malloc(totalLen + 1);
    
    memcpy(buf, basePath, baseLen);
    buf[baseLen] = sep;
    
    memcpy(buf + baseLen + 1, comp, compLen);
    buf[totalLen] = 0;
    
    return buf;
}


