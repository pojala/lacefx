/*
 *  LXBasicTypes.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 18.8.2007.
 *  Copyright 2007-12 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#if defined(__OBJC__) && !defined(LX_NO_FOUNDATION)
 #import <Foundation/Foundation.h>
#endif


#ifndef _LXBASICTYPES_H_
#define _LXBASICTYPES_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// --- platform detection defines ---
#if defined(__LP64__) || defined(__LLP64__) || defined(__WIN64__) || defined(CONFIG_64BIT)
 #define LX64BIT 1
#endif

#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WIN64__)
 #define LXPLATFORM_WIN 1
#endif

#if defined(__APPLE__)
 #include "TargetConditionals.h" 
 #if defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
  #define LXPLATFORM_IOS 1
 #else
  #define LXPLATFORM_MAC 1

  #if defined(__arm64__)
   #define LXPLATFORM_MAC_ARM64 1
  #endif

 #endif
#endif

#if defined(__linux__) || defined(__LINUX__)
 #define LXPLATFORM_LINUX 1
#endif

#if defined(WORDS_BIGENDIAN) && !defined(__BIG_ENDIAN__)
 #define __BIG_ENDIAN__ 1
#endif

// --- compiler detection defines ---
#if defined(__GNUC__) && !defined(__GCC__)
 #define __GCC__ 1
#endif
#if defined(__APPLE__) && !defined(__GCC__)
 #error "This Apple compiler probably acts like gcc but doesn't report as such, needs to be detected correctly"
#endif

#if (defined(__INTEL_COMPILER) || defined(__ICC)) && !defined(__ICC__)
 #define __ICC__ 1
#endif

#if (defined(MSC_VER) || defined(_MSC_VER)) && !defined(__ICC__) && !defined(__GCC__)
 #ifndef __MSVC__
  #define __MSVC__ 1
 #endif
#endif

#if !defined(__GCC__) && !defined(__ICC__) && !defined(__MSVC__)
 #error "unknown compiler"
#endif


// __func__ is useful; of course MSVC has a different name for it
#if defined(__MSVC__) && !defined(__func__)
 #define __func__ __FUNCTION__
#endif


// can't use BOOL safely because it has different definitions in Cocoa and Windows API.
// LXBool matches Cocoa's BOOL definition, so it can be used interchangeably.
typedef signed char LXBool;
typedef LXBool LXSuccess;

#if !defined(YES)
 #define YES ((LXBool)1)
 #define NO  ((LXBool)0)
#endif

// standard C99 integer types
#if !defined(__MSVC__)
 #include <stdint.h>
#endif

// can't use C99 standard's wchar_t for UTF-16 strings because it's 2 bytes on Windows but 4 bytes on OS X.
#if !defined(_CHAR16_T)
 #if defined(LXPLATFORM_WIN)
  typedef wchar_t char16_t;
 #else
  typedef unsigned short char16_t;
 #endif
 #define _CHAR16_T 1
#endif

#if !defined(_INT8_T) && !defined(_STDINT_H)
 typedef signed char int8_t;
 typedef unsigned char uint8_t;
 #define _INT8_T 1
#endif
#if !defined(_INT16_T) && !defined(_STDINT_H)
 typedef short int16_t;
 typedef unsigned short uint16_t;
 #define _INT16_T 1
#endif
#if !defined(_INT32_T) && !defined(_STDINT_H)
 typedef int int32_t;
 typedef unsigned int uint32_t;
 #define _INT32_T 1
#endif
#if !defined(_INT64_T) && !defined(_STDINT_H)
 typedef long long int64_t;
 typedef unsigned long long uint64_t;
 #define _INT64_T 1
#endif

#if !defined(INT32_MAX)
 #define INT32_MAX   0x7fffffffL
 #define INT32_MIN   (0L - 0x80000000L)
 #define UINT32_MAX  0xffffffffUL
#endif
#if !defined(INT64_MAX)
 #define INT64_MAX   9223372036854775807
 #define INT64_MIN   -9223372036854775808
 #define UINT64_MAX  18446744073709551615
#endif

// platform-dependent 128-bit vector types
#if defined(__BIG_ENDIAN__)
 #if defined(__ppc64__) && !defined(__VEC__)
  #define __VEC__ 1
 #endif
 #if defined(__VEC__)
  // PowerPC Altivec intrinsic format
  typedef vector float vfloat_t;
  typedef vector signed char vsint8_t;
  typedef vector signed short vsint16_t;
  typedef vector signed int vsint32_t;
  typedef vector unsigned char vuint8_t;
  typedef vector unsigned short vuint16_t;
  typedef vector unsigned int vuint32_t;
 #endif
#else
 #if defined(__SSE2__)
  #if !defined(__VEC__)
   #define __VEC__ 1
  #endif
  #if defined(__GCC__)
   #if defined(LXPLATFORM_WIN)
    #define LXFUNCATTR_SSE __attribute__((force_align_arg_pointer))
   #endif
   // this vector type definition only supported on GCC 3.5+
   typedef float           vfloat_t    __attribute__ ((__vector_size__ (16)));
   typedef double          vdouble_t   __attribute__ ((__vector_size__ (16)));
   typedef signed char     vsint8_t    __attribute__ ((__vector_size__ (16)));
   typedef signed short    vsint16_t   __attribute__ ((__vector_size__ (16)));
   typedef signed int      vsint32_t   __attribute__ ((__vector_size__ (16)));
   typedef unsigned char   vuint8_t    __attribute__ ((__vector_size__ (16)));
   typedef unsigned short  vuint16_t   __attribute__ ((__vector_size__ (16)));
   typedef unsigned int    vuint32_t   __attribute__ ((__vector_size__ (16)));
  #else
   #include <emmintrin.h>
   typedef __m128  vfloat_t;
   typedef __m128d vdouble_t;
   typedef __m128i vsint8_t;
   typedef __m128i vsint16_t;
   typedef __m128i vsint32_t; 
   typedef __m128i vuint8_t;
   typedef __m128i vuint16_t;
   typedef __m128i vuint32_t; 
  #endif
  #if !defined(LXFUNCATTR_SSE)
   #define LXFUNCATTR_SSE 
  #endif
 #endif
#endif

// LXFloat and LXInteger are native-width float and int types;
// they match definitions of CGFloat and NSInteger in Mac OS X 10.5.
// LXFloat can be explicitly forced to be a double by defining LXFLOAT_IS_DOUBLE
#if !defined(LXPLATFORM_IOS) && (defined(LX64BIT) || defined(LXFLOAT_IS_DOUBLE))
 typedef double LXFloat;
 #ifndef LXFLOAT_IS_DOUBLE
  #define LXFLOAT_IS_DOUBLE 1
 #endif
#else
 typedef float LXFloat;
#endif
#define LXFLOAT_DEFINED 1


#if defined(LXPLATFORM_WIN) && defined(LX64BIT)
 typedef long long LXInteger;
 typedef unsigned long long LXUInteger;
#elif defined(__LP64__)
 typedef long LXInteger;
 typedef unsigned long LXUInteger;
#else
 typedef int LXInteger;
 typedef unsigned int LXUInteger;
#endif
#define LXINTEGER_DEFINED 1

#if defined(LX64BIT)
 #define LXINTEGER_MIN   INT64_MIN
 #define LXINTEGER_MAX   INT64_MAX
 #define LXUINTEGER_MAX  UINT64_MAX
#else
 #define LXINTEGER_MIN   INT32_MIN
 #define LXINTEGER_MAX   INT32_MAX
 #define LXUINTEGER_MAX  UINT32_MAX
#endif

// used to ensure that enum types in Lacefx are not sized smaller than long by the compiler
#define LXENUMMAX  LXUINTEGER_MAX


// C99 restrict keyword and its compiler-specific equivalents
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
 #define LXRESTRICT restrict
#elif defined(__GCC__)
 #define LXRESTRICT __restrict__
#elif defined(__MSVC__)
 #define LXRESTRICT __restrict
#else
 #define LXRESTRICT
#endif

// inline
#if defined(__MSVC__)
 #define LXINLINE static __inline
#elif defined(__GCC__)
 #define LXINLINE static __inline__ __attribute__((always_inline))   // possibly should be 'extern inline' on some GCC platforms?
#else
 #define LXINLINE static inline
#endif

#if defined(__GCC__)
 #define LXFUNCATTR_PURE    __attribute__ ((pure))
 #define LXFUNCATTR_MALLOC  __attribute__ ((malloc))
#elif defined(__MSVC__)
 #define LXFUNCATTR_PURE    __declspec(noalias)
 #define LXFUNCATTR_MALLOC  __declspec(noalias) __declspec(restrict)
#else
 #define LXFUNCATTR_PURE
 #define LXFUNCATTR_MALLOC
#endif

// extern
#ifdef __cplusplus
 #define LXEXTERN extern "C"
#else
 #define LXEXTERN extern
#endif

// export declaration for API functions
#if defined(LXPLATFORM_WIN)
  #ifdef LX_INSIDE_BUILD
    #define LXEXPORT __declspec(dllexport)
  #else
    #define LXEXPORT __declspec(dllimport)
  #endif
  #define LXPRIVATE
#elif defined(__APPLE__)
    #define LXEXPORT __attribute__ ((visibility("default")))
    #define LXPRIVATE __attribute__ ((visibility("hidden")))
#else
  #ifdef HAVE_GCCVISIBILITYPATCH
    // visibility definition in GCC; more info in the the GCC wiki: http://gcc.gnu.org/wiki/Visibility
    #define LXEXPORT __attribute__ ((visibility("default")))
    #define LXPRIVATE __attribute__ ((visibility("hidden")))
  #else
    #define LXEXPORT
    #define LXPRIVATE
  #endif
#endif

// for variables that are exported from the DLL
#define LXEXPORT_VAR        LXEXPORT extern
#define LXEXPORT_CONSTVAR   LXEXPORT extern const


// standard structure types.
// Cocoa-style utility functions (LXMakeRect, LXMakeRGBA, etc.) are available in LXBasicTypeFunctions.h

#pragma pack(push, 4)


typedef struct _LXPoint {
    LXFloat x;
    LXFloat y;
} LXPoint;

typedef struct _LXSize {
    LXFloat w;
    LXFloat h;
} LXSize;

typedef struct _LXRect {
    LXFloat x;
    LXFloat y;
    LXFloat w;
    LXFloat h;
} LXRect;

typedef struct _LXRGBA {
    LXFloat r;
    LXFloat g;
    LXFloat b;
    LXFloat a;
} LXRGBA;



// generic type for platform-dependent native object, always needs to be cast to the correct native type.
// (native type may actually be something else than a pointer, e.g. an OpenGL texture ID)
typedef LXUInteger LXUnidentifiedNativeObj;


// pixel formats
enum {
    kLX_RGBA_INT8 = 1,              // preferred default format over ARGB/BGRA
    kLX_RGBA_FLOAT16,
    kLX_RGBA_FLOAT32,
    
    kLX_Luminance_INT8 = 0x100,
    kLX_Luminance_FLOAT16,
    kLX_Luminance_FLOAT32,
    
    kLX_YCbCr422_INT8 = 0x200,      // equal to '2vuy' QuickTime pixel format and 'UYVY' Win32 pixel format

    kLX_ARGB_INT8 = 0x300,          // supported for QuickTime / CG compatibility; should only be used for textures
    kLX_BGRA_INT8,                  // Cairo only supports this format on OS X x86 so we need to support it; same caveat as above
};
typedef LXUInteger LXPixelFormat;

// standard color spaces
enum {
    kLX_CanonicalLinearRGB = 1,
    kLX_sRGB = 0x1500,
    kLX_AdobeRGB_1998 = 0x1510,
    kLX_ProPhotoRGB = 0x1530,
    kLX_YCbCr_Rec601 = 0x2500,
    kLX_YCbCr_Rec709
};
typedef LXUInteger LXColorSpaceEncoding;



// size of the LXRef base type.
// this type's memory overhead is admittedly very large because of the embedded function pointers --
// but it's meant for fairly expensive type like surfaces and pixel buffers, so we can get away with it.

#define LXREF_HEADERBYTES (4 + 4 + sizeof(char *) + 20*sizeof(void *))  // 92 bytes (on a 32-bit platform)

struct _LXUnknownOpaqueRef {
    uint8_t _lxHeader[LXREF_HEADERBYTES];
};
typedef struct _LXUnknownOpaqueRef *LXRef;


// allocators exported from the Lacefx library.
// these functions guarantee 16-byte alignment for vector math.
//
// it's not safe to use regular free() with buffers alloced with these functions for two reasons:
//
//   * the aligned allocator may be different from regular malloc (as is the case on Windows but not OS X)
//
//   * Windows DLLs loaded into the same process can be linked against different CRT versions
//     (e.g. MSVC6 and MSVC9), which will cause havoc if buffers are created and released across
//     DLL boundaries.
//
// the rationale for prefixing these with an underscore was that these are simply wrappers
// with no functional difference from the standard library function.
// probably the underscore should not be there, but there's now too much code that uses these
// so I've never bothered to make that change.

LXEXTERN LXEXPORT LXFUNCATTR_MALLOC void *_lx_malloc(size_t size);
LXEXTERN LXEXPORT LXFUNCATTR_MALLOC void *_lx_calloc(size_t count, size_t size);
LXEXTERN LXEXPORT void *_lx_realloc(void *ptr, size_t size);
LXEXTERN LXEXPORT void _lx_free(void *ptr);

LXEXTERN LXEXPORT void *_lx_memcpy_aligned(void * LXRESTRICT dst, const void * LXRESTRICT src, size_t n);


// basic UTF-16 string structure. the string is _lx_malloced and not null-terminated.
// use _lx_free() to destroy the string buffer.
// 
// rather than defining an opaque string object type, Lacefx uses this for strings and file paths.
// the rationale for using UTF-16 is that's it's easy to convert between Win32's armada of string
// types as well as Cocoa's NSString.
//
// Lacefx also provides UTF-8 <-> UTF-16 conversion wrappers in LXStringUtils.h.

typedef struct _LXUnibuffer {
    size_t      numOfChar16;
    char16_t    *unistr;
} LXUnibuffer;

LXINLINE LXFUNCATTR_PURE LXUnibuffer LXMakeUnibuffer(size_t count, char16_t *unistr) {
    LXUnibuffer ub;
    ub.numOfChar16 = count;
    ub.unistr = unistr;
    return ub;
}


// a basic error type. Lacefx functions don't return error codes directly,
// instead they take a pointer to an LXError structure and fill it out in case of error.
//
// the LXDECLERROR macro is provided for conveniently creating a zeroed LXError on the stack.
// if an error occurs, LXErrorDestroyOnStack should be used to free the contents of the LXError structure.

typedef struct _LXError {
    uint8_t     _header[LXREF_HEADERBYTES];  // header to allow for LXRef wrapping or other future expansion without breaking binary compatibility
    int32_t     errorID;         // function-dependent error code
    int32_t     specifierID;     // currently unused
    char        *location;       // name of the function where the error occurred (e.g. "LXSurfaceCreate")
    char        *description;    // error description
    void        *_res2;
} LXError;

#define LXDECLERROR(_err_)  LXError _err_;  memset(&_err_, 0, sizeof(LXError));

// convenience macro for setting an LXError's ID and string description.
// the string is copied and must be freed with _lx_free().
#define LXErrorSet(_e, _eid, _str)  { if (_e) {                      \
                                    memset(_e, 0, sizeof(LXError));  \
                                    size_t len_ = strlen(_str) + 1;   \
                                    char *errMsg_ = (char *)_lx_malloc(len_); \
                                    memcpy(errMsg_, _str, len_);          \
                                    len_ = strlen(__func__) + 1;      \
                                    char *loc_ = (char *)_lx_malloc(len_); \
                                    memcpy(loc_, __func__, len_);      \
                                    \
                                    (_e)->errorID = _eid;            \
                                    (_e)->location = loc_;            \
									(_e)->description = errMsg_;  } }

// convenience macros to free all pointers in an LXError
#define LXErrorDestroy(_e) { if (_e) {   \
                                    if ((_e)->location) _lx_free((_e)->location); \
                                    if ((_e)->description) _lx_free((_e)->description); \
                                    _lx_free(_e);  _e = NULL; } }

#define LXErrorDestroyOnStack(_e) { if ((_e).location) _lx_free((_e).location); \
                                    if ((_e).description) _lx_free((_e).description); \
                                    memset(&(_e), 0, sizeof(LXError)); }


// the Windows MSVC runtime doesn't include strncmp,
// so #define it to our own implementation.
#if defined(LXPLATFORM_WIN) && !defined(STRNCMP_DEFINED)
 #define strncmp _lx_strncmp
 #define strncpy _lx_strncpy
 #define STRNCMP_DEFINED 1
#endif
#if !defined(STRNCMP_DEFINED)
 #define STRNCMP_DEFINED 1
#endif


// another minimal string type to aid in localization.
// caller (e.g. host application) fills the "lang/country/idstr" fields in LXLocString and passes it to a callee (e.g. Lacefx plugin),
// which can then fetch the localized string from its own string storage and return it in "utf16Buf".
//
// the "ID string" is expected to be a language-invariant identifier for this string --
// it can be simply the English version, or some shorter kind of identifier (depending on the application's needs).

typedef struct _LXLocString {
    uint8_t     _header[LXREF_HEADERBYTES];   // header to allow for LXRef wrapping or other future expansion without breaking binary compatibility
    char        lang[4];        // language code
    char        country[4];     // country code
    char        *idstr;         // a zero-terminated, _lx_malloc'ed UTF-8 string identifier (this should usually be the English string)
    LXUnibuffer utf16Buf;       // the UTF-16 string, _lx_malloc'ed
} LXLocString;

// convenience macro to set only the "ID string" in a localizable string
#define LXLocStringSetIDString(_s, _idstr)  { if (_s) {  \
                                                size_t _len_ = strlen(_idstr) + 1;  \
                                                char *_str_ = (char *)_lx_malloc(_len_);  \
                                                memcpy(_str_, _idstr, _len_);  \
                                                if ((_s)->idstr)  _lx_free((_s)->idstr);  \
                                                (_s)->idstr = _str_;  }  }



// --- macros and inline functions for endianness swapping ---

enum {
    kLXUnknownEndian = 0,
    kLXLittleEndian = 1,
    kLXBigEndian = 2,
};
typedef LXUInteger LXEndianness;

#ifdef __BIG_ENDIAN__
 #define LXEndianLittle_u16(v_)   LXEndianSwap_uint16(v_)
 #define LXEndianLittle_s16(v_)   LXEndianSwap_sint16(v_)
 #define LXEndianLittle_u32(v_)   LXEndianSwap_uint32(v_)
 #define LXEndianLittle_s32(v_)   LXEndianSwap_sint32(v_)
 #define LXEndianLittle_f32(v_)   LXEndianSwap_float(v_)
 #define LXEndianLittle_f64(v_)   LXEndianSwap_double(v_)
 #define LXEndianLittle_u64(v_)   LXEndianSwap_uint64(v_)
 #define LXEndianLittle_s64(v_)   LXEndianSwap_sint64(v_)
 #define LXEndianBig_u16(v_)      (v_) 
 #define LXEndianBig_s16(v_)      (v_) 
 #define LXEndianBig_u32(v_)      (v_)
 #define LXEndianBig_s32(v_)      (v_)
 #define LXEndianBig_f32(v_)      (v_)
 #define LXEndianBig_f64(v_)      (v_) 
 #define LXEndianBig_u64(v_)      (v_)  
 #define LXEndianBig_s64(v_)      (v_)  
#else
 #define LXEndianLittle_u16(v_)   (v_)
 #define LXEndianLittle_s16(v_)   (v_)
 #define LXEndianLittle_u32(v_)   (v_)
 #define LXEndianLittle_s32(v_)   (v_)
 #define LXEndianLittle_f32(v_)   (v_)
 #define LXEndianLittle_f64(v_)   (v_)
 #define LXEndianLittle_u64(v_)   (v_)
 #define LXEndianLittle_s64(v_)   (v_)
 #define LXEndianBig_u16(v_)      LXEndianSwap_uint16(v_)
 #define LXEndianBig_s16(v_)      LXEndianSwap_sint16(v_)
 #define LXEndianBig_u32(v_)      LXEndianSwap_uint32(v_)
 #define LXEndianBig_s32(v_)      LXEndianSwap_sint32(v_)
 #define LXEndianBig_f32(v_)      LXEndianSwap_float(v_)
 #define LXEndianBig_f64(v_)      LXEndianSwap_double(v_)
 #define LXEndianBig_u64(v_)      LXEndianSwap_uint64(v_)
 #define LXEndianBig_s64(v_)      LXEndianSwap_sint64(v_)
#endif

typedef union {
    float f;     uint8_t c[4];     int32_t i;     uint32_t ui;
} LXFloat32CharUnion;

typedef union {
    double f;    uint8_t c[8];     int64_t ll;    uint64_t ull;
} LXDouble64CharUnion;

#define LXEndianSwap_uint32(v_) (  (((v_) << 24) & 0xff000000) + (((v_) << 8) & 0x00ff0000) + (((v_) >> 8) & 0x0000ff00) + (((v_) >> 24) & 0xff)  )
#define LXEndianSwap_uint16(v_) (  (((v_) << 8) & 0xff00) + (((v_) >> 8) & 0xff)  )

LXINLINE LXFUNCATTR_PURE int16_t LXEndianSwap_sint16(int16_t v) {
    uint16_t d = LXEndianSwap_uint16( *((uint16_t *)(&v)) );
    return *((int16_t *)(&d));
}

LXINLINE LXFUNCATTR_PURE int32_t LXEndianSwap_sint32(int32_t v) {
    uint32_t d = LXEndianSwap_uint32( *((uint32_t *)(&v)) );
    return *((int32_t *)(&d));
}

LXINLINE LXFUNCATTR_PURE float LXEndianSwap_float(float f) {
    LXFloat32CharUnion s, d;
    s.f = f;
    d.c[0] = s.c[3];
    d.c[1] = s.c[2];
    d.c[2] = s.c[1];
    d.c[3] = s.c[0];
    return d.f;
}

LXINLINE LXFUNCATTR_PURE double LXEndianSwap_double(double f) {
    LXDouble64CharUnion s, d;
    s.f = f;
    d.c[0] = s.c[7];
    d.c[1] = s.c[6];
    d.c[2] = s.c[5];
    d.c[3] = s.c[4];
    d.c[4] = s.c[3];
    d.c[5] = s.c[2];
    d.c[6] = s.c[1];
    d.c[7] = s.c[0];
    return d.f;
}

LXINLINE LXFUNCATTR_PURE uint64_t LXEndianSwap_uint64(uint64_t ull) {
    LXDouble64CharUnion s, d;
    s.ull = ull;
    d.c[0] = s.c[7];
    d.c[1] = s.c[6];
    d.c[2] = s.c[5];
    d.c[3] = s.c[4];
    d.c[4] = s.c[3];
    d.c[5] = s.c[2];
    d.c[6] = s.c[1];
    d.c[7] = s.c[0];
    return d.ull;
}

LXINLINE LXFUNCATTR_PURE int64_t LXEndianSwap_sint64(int64_t v) {
    uint64_t d = LXEndianSwap_uint64( *((uint64_t *)(&v)) );
    return *((int64_t *)(&d));
}



// Unicode byte order marker.
// there are functions in LXStringUtils.h for detecting and removing the byte order marker from a string.
#define kBOM_UTF16              ((char16_t)0xFEFF)
#define kBOM_UTF16_SWAPPED      ((char16_t)0xFFFE)



// --- other types, flags & miscellany ---

typedef struct _LXVersion {
    uint32_t majorVersion;
    uint32_t minorVersion;
    uint32_t milliVersion;
    uint32_t microVersion;
} LXVersion;


// storage hints can be used in Lacefx for objects that may be cached on the GPU (or other asymmetric storage)
enum {
    kLXDefaultStorage                   = 0,
    kLXStorageHint_ClientStorage        = 1,
    kLXStorageHint_Final                = 1 << 1,
    kLXStorageHint_PreferDMAToCaching   = 1 << 2,
    kLXStorageHint_PreferCaching        = 1 << 3,
};
typedef LXUInteger LXStorageHint;


// matches NSNotFound, which is defined as LONG_MAX
enum {
    kLXNotFound = LXINTEGER_MAX
};


#if !defined(MIN)
 #if defined(__GCC__)
  #define MIN(a,b)  ({ __typeof__(a) __a = (a); __typeof__(b) __b = (b); __a < __b ? __a : __b; })
  #define MAX(a,b)  ({ __typeof__(a) __a = (a); __typeof__(b) __b = (b); __a > __b ? __a : __b; })  
 #else
  #define MIN(a,b)  (((a) < (b)) ? (a) : (b))
  #define MAX(a,b)  (((a) > (b)) ? (a) : (b))
 #endif
#endif


// some basic math macros for 32-bit float.
// using these defs ensures that GCC doesn't generate function calls even with optimization disabled.
#ifndef FABSF
 #if defined(__GCC__)
  #define FABSF(f_)    __builtin_fabsf(f_)
  #define FLOORF(f_)   __builtin_floorf(f_)
  #define POWF(f_)     __builtin_powf(f_)
  #define SQRTF(f_)    __builtin_sqrtf(f_) 
 #else
  #define FABSF(f_)  fabsf(f_)
  #define FLOORF(f_) floorf(f_)
  #define POWF(f_)   powf(f_)
  #define SQRTF(f_)  sqrtf(f_)
 #endif
#endif

#if LXFLOAT_IS_DOUBLE
 #if defined(__GCC__)
  #define LXFABS(f_) __builtin_fabs(f_)
  #define LXFLOOR(f_) __builtin_floor(f_)
 #else
  #define LXFABS(f_) fabs(f_)
  #define LXFLOOR(f_) floor(f_)
 #endif
#else
 #define LXFABS(f_) FABSF(f_)
 #define LXFLOOR(f_) FLOORF(f_)
#endif


// non-branching [0,1] clamp operation for 32-bit float pixels:  clamp(x,0,1) == (1+abs(x)-abs(x-1))/2 
#ifndef CLAMP_SAT_F
 #define CLAMP_SAT_F(pxf_)  (  ( (1.0f+FABSF(pxf_) - FABSF(pxf_-1.0f)) )*0.5f  )
#endif

// add round/lround functions that are missing from MSVC's math.h
#if defined(__MSVC__)
#include <math.h>

 #if !defined(LROUND_DEFINED)
 #define LROUND_DEFINED 1
LXINLINE LXFUNCATTR_PURE long lround(double d) {
    return (long)(d>0 ? d+0.5 : ceil(d-0.5));
}
 #endif

 #if !defined(ROUND_DEFINED)
 #define ROUND_DEFINED 1 
LXINLINE LXFUNCATTR_PURE int round(double d) {
    return (d>0) ? int(d+0.5) : int(d-0.5);
}
 #endif
#endif


// debugging

#if defined(__MSVC__)
 #define LXDEBUGLOG(format, ...)   LXPrintf(__VA_ARGS__);  LXPrintf("\n");
 #define LXWARN(format, ...)       LXPrintf(__VA_ARGS__);  LXPrintf("\n");
#else

 #if (defined(RELEASE) && !LXDEBUG) || defined(LXDEBUG_DISABLE)
  #define LXDEBUGLOG(format, args...)
 #else
  #define LXDEBUGLOG(format, args...)  \
            {  fprintf(stderr, format , ## args); \
               fputc('\n', stderr);  }
 #endif    

 #define LXWARN(format, args...) \
            {  fprintf(stderr, "*** LXAPI warning: "); \
               fprintf(stderr, format , ## args); \
               fputc('\n', stderr);  }
#endif

#pragma pack(pop)

#endif // _LXBASICTYPES_H_
