/*
 *  LXTransform3D.c
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 23.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXTransform3D.h"
#include "LXRef_Impl.h"

#include <assert.h>
#include <math.h>


#if defined(__SSE2__)
#include <xmmintrin.h>
#include <pmmintrin.h>  // SSE3 intrinsics

void lx_matmul_sse3_aligned(const float *in_A,
                            const float *in_x,
                            const float *out_y,
                            LXInteger n)
{
    __m128 A0 = _mm_load_ps((const float *)(in_A + 0));
    __m128 A1 = _mm_load_ps((const float *)(in_A + 4));
    __m128 A2 = _mm_load_ps((const float *)(in_A + 8));
    __m128 A3 = _mm_load_ps((const float *)(in_A + 12));
    
    for (LXInteger i = 0; i < n; i++) {
        __m128 x =  _mm_load_ps((const float*)(in_x + i*4));
        __m128 m0 = _mm_mul_ps(A0, x);
        __m128 m1 = _mm_mul_ps(A1, x);
        __m128 m2 = _mm_mul_ps(A2, x);
        __m128 m3 = _mm_mul_ps(A3, x);
        __m128 sum_01 = _mm_hadd_ps(m0, m1); 
        __m128 sum_23 = _mm_hadd_ps(m2, m3);
        __m128 result = _mm_hadd_ps(sum_01, sum_23);
        _mm_store_ps((float*)(out_y + i*4), result);
    }
}

#endif


static const LX4x4Matrix g_identityMatrix = { 1, 0, 0, 0,
                                              0, 1, 0, 0,
                                              0, 0, 1, 0,
                                              0, 0, 0, 1 };

const LX4x4Matrix * const LXIdentity4x4Matrix = &g_identityMatrix;


typedef struct {
    LXREF_STRUCT_HEADER
    
    LX4x4Matrix mat;
} LXTransform3DImpl;



LXTransform3DRef LXTransform3DRetain(LXTransform3DRef r)
{
    if ( !r) return NULL;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)r;

    LXAtomicInc_int32(&(imp->retCount));
    
    return r;
}

void LXTransform3DRelease(LXTransform3DRef r)
{
    if ( !r) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)r;
    
    int32_t refCount = LXAtomicDec_int32(&(imp->retCount));    
    if (refCount == 0) {
        LXRefWillDestroyItself((LXRef)r);
        _lx_free(imp);
    }
}


LXINLINE void setIdentityMatrix(LX4x4Matrix *mat)
{
    if ( !mat) return;
    memset(mat, 0, sizeof(LX4x4Matrix));
    mat->m11 = mat->m22 = mat->m33 = mat->m44 = 1.0;
}


const char *LXTransform3DTypeID()
{
    static const char *s = "LXTransform3D";
    return s;
}

LXTransform3DRef LXTransform3DCreateIdentity()
{
    // LX4x4Matrix is a struct, but we want its size to be equal to a 4x4 array of floats --
    // check that the compiler doesn't do any evil padding
    assert(sizeof(LX4x4Matrix) == 4*4*sizeof(LXFloat));
    
    LXTransform3DImpl *imp = (LXTransform3DImpl *) _lx_calloc(sizeof(LXTransform3DImpl), 1);

    LXREF_INIT(imp, LXTransform3DTypeID(), LXTransform3DRetain, LXTransform3DRelease);

    setIdentityMatrix(&(imp->mat));

    return (LXTransform3DRef)imp;
}

LXTransform3DRef LXTransform3DCreateWithMatrix(LX4x4Matrix *matrix)
{
    LXTransform3DRef r = LXTransform3DCreateIdentity();
    LXTransform3DImpl *imp = (LXTransform3DImpl *)r;
    
    if (matrix)
        imp->mat = *matrix;
    else
        setIdentityMatrix(&(imp->mat));
    
    return r;
}

LXTransform3DRef LXTransform3DCopy(LXTransform3DRef orig)
{
    LXTransform3DRef newR = LXTransform3DCreateIdentity();
    
    LXTransform3DImpl *newImp = (LXTransform3DImpl *)newR;
    LXTransform3DImpl *origImp = (LXTransform3DImpl *)orig;
    
    if (newImp && origImp)
        newImp->mat = origImp->mat;
    
    return newR;
}


LXTransform3DRef LXTransform3DCreateWith2DTransform(LXFloat m11, LXFloat m12, LXFloat m21, LXFloat m22, LXFloat tX, LXFloat tY)
{
    LXTransform3DRef r = LXTransform3DCreateIdentity();
    LXTransform3DImpl *imp = (LXTransform3DImpl *)r;

    imp->mat.m11 = m11;
    imp->mat.m12 = m12;
    imp->mat.m21 = m21;
    imp->mat.m22 = m22;

    imp->mat.m14 = tX;
    imp->mat.m24 = tY;
    
    //imp->mat.m41 = 1.0;
    //imp->mat.m42 = 1.0;
    
    return r;
}

LXTransform3DRef LXTransform3DCreateWithTranslation(LXFloat tx, LXFloat ty, LXFloat tz)
{
    LXTransform3DRef r = LXTransform3DCreateIdentity();
    
    LXTransform3DTranslate(r, tx, ty, tz);
    
    return r;
}

LXTransform3DRef LXTransform3DCreateWithScale(LXFloat sx, LXFloat sy, LXFloat sz)
{
    LXTransform3DRef r = LXTransform3DCreateIdentity();
    
    LXTransform3DScale(r, sx, sy, sz);
    
    return r;
}

LXTransform3DRef LXTransform3DCreateWithRotation(LXFloat angle, LXFloat x, LXFloat y, LXFloat z)
{
    LXTransform3DRef r = LXTransform3DCreateIdentity();
    
    LXTransform3DRotate(r, angle, x, y, z);
    
    return r;
}

void LXTransform3DSetIdentity(LXTransform3DRef r)
{
    if ( !r) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)r;
    
    setIdentityMatrix(&(imp->mat));
}

void LXTransform3DTranslate(LXTransform3DRef r, LXFloat tx, LXFloat ty, LXFloat tz)
{
    if ( !r) return;
    LX4x4Matrix mm;
    
    setIdentityMatrix(&mm);
    mm.m14 = tx;
    mm.m24 = ty;
    mm.m34 = tz;
    mm.m44 = 1.0;
    
    LXTransform3DConcatMatrix(r, &mm);
}

void LXTransform3DScale(LXTransform3DRef r, LXFloat sx, LXFloat sy, LXFloat sz)
{
    if ( !r) return;
    LX4x4Matrix mm;
    
    setIdentityMatrix(&mm);
    mm.m11 = sx;
    mm.m22 = sy;
    mm.m33 = sz;
    mm.m44 = 1.0;
    
    LXTransform3DConcatMatrix(r, &mm);
}

void LXTransform3DRotate(LXTransform3DRef r, LXFloat angle, LXFloat ax, LXFloat ay, LXFloat az)
{
    if ( !r) return;
    LX4x4Matrix mm;
    setIdentityMatrix(&mm);

    // use doubles internally
    double x = ax;
    double y = ay;
    double z = az;

    // normalize xyz
    const double sumOfSq = x*x + y*y + z*z;
    if (sumOfSq == 0.0)
        return;  // vector length is 0, can't rotate
    
    const double len = sqrt(sumOfSq);
    x /= len;
    y /= len;
    z /= len;

    // algorithm based on glRotate man page
    const double c = cos(angle);
    const double s = sin(angle);
    const double ci = 1.0 - c;

    mm.m11 = x*x*ci + c;
    mm.m21 = y*x*ci + z*s;
    mm.m31 = x*z*ci - y*s;    
    
    mm.m12 = x*y*ci - z*s;
    mm.m22 = y*y*ci + c;
    mm.m32 = y*z*ci + x*s;
    
    mm.m13 = x*z*ci + y*s;
    mm.m23 = y*z*ci - x*s;
    mm.m33 = z*z*ci + c;
    
    LXTransform3DConcatMatrix(r, &mm);
}


void LXTransform3DGetMatrix(LXTransform3DRef t, LX4x4Matrix *outMatrix)
{
    if ( !t || !outMatrix) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    
    *outMatrix = imp->mat;
}

void LXTransform3DSetMatrix(LXTransform3DRef t, LX4x4Matrix *matrix)
{
    if ( !t || !matrix) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    
    imp->mat = *matrix;
}


#pragma mark --- transpose ---

void LX4x4MatrixTranspose(LX4x4Matrix *dst, const LX4x4Matrix *src)
{
   LXFloat * LXRESTRICT to = (LXFloat *) dst;
   LXFloat * LXRESTRICT from;
   LXFloat *orig = (LXFloat *) src;
   LXFloat temp[16];
   
   if (dst == src) {  // for in-place operation, need a copy of the original data
        memcpy(temp, orig, 16*sizeof(LXFloat));
        from = temp;
    } else
        from = orig;
   
   to[0] = from[0];
   to[1] = from[4];
   to[2] = from[8];
   to[3] = from[12];
   to[4] = from[1];
   to[5] = from[5];
   to[6] = from[9];
   to[7] = from[13];
   to[8] = from[2];
   to[9] = from[6];
   to[10] = from[10];
   to[11] = from[14];
   to[12] = from[3];
   to[13] = from[7];
   to[14] = from[11];
   to[15] = from[15];
}

char *LX4x4MatrixCreateString(const LX4x4Matrix *m, LXBool insertLineBreaks)
{
    if ( !m) return NULL;
    
    LXInteger bufSize = 511;
    char *str = _lx_calloc(bufSize+1, 1);
    LXInteger n = 0;
    
    snprintf(str+n, bufSize-n, "{ ");
    n = strlen(str);
    
    LXFloat *mf = (LXFloat *)m;
    
    LXInteger x, y;
    for (y = 0; y < 4; y++) {
        snprintf(str+n, MAX(0, bufSize-n), "[ ");
        n = strlen(str);
    
        for (x = 0; x < 4; x++) {
            LXFloat f = mf[y*4 + x];
            if (f == 0.0) {
                snprintf(str+n, MAX(0, bufSize-n), "0.0");
            } else if (f == 1.0) {
                snprintf(str+n, MAX(0, bufSize-n), "1.0");
            } else {
                snprintf(str+n, MAX(0, bufSize-n), "%.8f", f);
            }
            n = strlen(str);
            
            if (x < 3) {
                snprintf(str+n, MAX(0, bufSize-n), ", ");
            } else {
                if (y < 3) {
                    snprintf(str+n, MAX(0, bufSize-n), " ], ");
                } else {
                    snprintf(str+n, MAX(0, bufSize-n), " ] ");
                }
            }
            n = strlen(str);
        }
        if (insertLineBreaks) {
            snprintf(str+n, MAX(0, bufSize-n), "\n");
            n = strlen(str);
            if (y < 3) { // indent
                snprintf(str+n, MAX(0, bufSize-n), "  ");
                n = strlen(str);
            }
        }
    }
    
    snprintf(str+n, MAX(0, bufSize-n), "} ");
    
    return str;
}


#pragma mark --- concat ---

void LXTransform3DConcat(LXTransform3DRef a, LXTransform3DRef b)
{
    if ( !a || !b) return;
    if (a == b) {
        LXPrintf("*** %s: can't concat matrix with itself (would violate restrict rule)\n", __func__);
        return;
    }
    LXTransform3DImpl *bimp = (LXTransform3DImpl *)b;
    
    LXTransform3DConcatMatrix(a, &(bimp->mat));
}


void LXTransform3DConcatMatrix(LXTransform3DRef r, LX4x4Matrix * LXRESTRICT A)
{
    if (!r || !A) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)r;
    LX4x4Matrix * LXRESTRICT B = &(imp->mat);
    LX4x4Matrix dm;
    
    dm.m11 = A->m11 * B->m11  +  A->m12 * B->m21  +  A->m13 * B->m31  +  A->m14 * B->m41;
    dm.m12 = A->m11 * B->m12  +  A->m12 * B->m22  +  A->m13 * B->m32  +  A->m14 * B->m42;
    dm.m13 = A->m11 * B->m13  +  A->m12 * B->m23  +  A->m13 * B->m33  +  A->m14 * B->m43;
    dm.m14 = A->m11 * B->m14  +  A->m12 * B->m24  +  A->m13 * B->m34  +  A->m14 * B->m44;
    
    dm.m21 = A->m21 * B->m11  +  A->m22 * B->m21  +  A->m23 * B->m31  +  A->m24 * B->m41;
    dm.m22 = A->m21 * B->m12  +  A->m22 * B->m22  +  A->m23 * B->m32  +  A->m24 * B->m42;
    dm.m23 = A->m21 * B->m13  +  A->m22 * B->m23  +  A->m23 * B->m33  +  A->m24 * B->m43;
    dm.m24 = A->m21 * B->m14  +  A->m22 * B->m24  +  A->m23 * B->m34  +  A->m24 * B->m44;

    dm.m31 = A->m31 * B->m11  +  A->m32 * B->m21  +  A->m33 * B->m31  +  A->m34 * B->m41;
    dm.m32 = A->m31 * B->m12  +  A->m32 * B->m22  +  A->m33 * B->m32  +  A->m34 * B->m42;
    dm.m33 = A->m31 * B->m13  +  A->m32 * B->m23  +  A->m33 * B->m33  +  A->m34 * B->m43;
    dm.m34 = A->m31 * B->m14  +  A->m32 * B->m24  +  A->m33 * B->m34  +  A->m34 * B->m44;

    dm.m41 = A->m41 * B->m11  +  A->m42 * B->m21  +  A->m43 * B->m31  +  A->m44 * B->m41;
    dm.m42 = A->m41 * B->m12  +  A->m42 * B->m22  +  A->m43 * B->m32  +  A->m44 * B->m42;
    dm.m43 = A->m41 * B->m13  +  A->m42 * B->m23  +  A->m43 * B->m33  +  A->m44 * B->m43;
    dm.m44 = A->m41 * B->m14  +  A->m42 * B->m24  +  A->m43 * B->m34  +  A->m44 * B->m44;
    
    memcpy(&(imp->mat), &dm, sizeof(LX4x4Matrix));
}


#pragma mark --- transform vec ---


#define MAT(m_, col_, row_)  (m_)[(row_)*4 + (col_)]


LXINLINE void transformVector(LXFloat * LXRESTRICT u, const LXFloat * LXRESTRICT v, const LXFloat * LXRESTRICT m)
{
   LXFloat v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
   u[0] = v0 * MAT(m,0,0) + v1 * MAT(m,1,0) + v2 * MAT(m,2,0) + v3 * MAT(m,3,0);
   
   u[1] = v0 * MAT(m,0,1) + v1 * MAT(m,1,1) + v2 * MAT(m,2,1) + v3 * MAT(m,3,1);
   
   u[2] = v0 * MAT(m,0,2) + v1 * MAT(m,1,2) + v2 * MAT(m,2,2) + v3 * MAT(m,3,2);
   
   u[3] = v0 * MAT(m,0,3) + v1 * MAT(m,1,3) + v2 * MAT(m,2,3) + v3 * MAT(m,3,3);
}

LXINLINE void transformVector_f(float * LXRESTRICT u, const float * LXRESTRICT v, const float * LXRESTRICT m)
{
   float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
   u[0] = v0 * (float)MAT(m,0,0) + v1 * (float)MAT(m,1,0) + v2 * (float)MAT(m,2,0) + v3 * (float)MAT(m,3,0);
   
   u[1] = v0 * (float)MAT(m,0,1) + v1 * (float)MAT(m,1,1) + v2 * (float)MAT(m,2,1) + v3 * (float)MAT(m,3,1);
   
   u[2] = v0 * (float)MAT(m,0,2) + v1 * (float)MAT(m,1,2) + v2 * (float)MAT(m,2,2) + v3 * (float)MAT(m,3,2);
   
   u[3] = v0 * (float)MAT(m,0,3) + v1 * (float)MAT(m,1,3) + v2 * (float)MAT(m,2,3) + v3 * (float)MAT(m,3,3);
}

LXINLINE void transformVectorTransposed(LXFloat * LXRESTRICT u, const LXFloat * LXRESTRICT v, const LXFloat * LXRESTRICT m)
{
   LXFloat v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
   u[0] = v0 * MAT(m,0,0) + v1 * MAT(m,0,1) + v2 * MAT(m,0,2) + v3 * MAT(m,0,3);
   
   u[1] = v0 * MAT(m,1,0) + v1 * MAT(m,1,1) + v2 * MAT(m,1,2) + v3 * MAT(m,1,3);
   
   u[2] = v0 * MAT(m,2,0) + v1 * MAT(m,2,1) + v2 * MAT(m,2,2) + v3 * MAT(m,2,3);
      
   u[3] = v0 * MAT(m,3,0) + v1 * MAT(m,3,1) + v2 * MAT(m,3,2) + v3 * MAT(m,3,3);
}


void LXTransform3DTransformVector(LXTransform3DRef t, LXFloat *x, LXFloat *y, LXFloat *z)
{
    if ( !t) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);

    LXFloat out[4] = { 0.0, 0.0, 0.0, 1.0 };
    LXFloat in[4] = { (x) ? *x : 0.0, (y) ? *y : 0.0, (z) ? *z : 0.0, 1.0 };
    
    transformVector(out, in, (const LXFloat *)mm);
    
    if (x) *x = out[0];
    if (y) *y = out[1];
    if (z) *z = out[2];
}

void LXTransform3DTransformVector4Ptr(LXTransform3DRef t, LXFloat *vf)
{
    if ( !t) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);

    LXFloat out[4] = { 0.0, 0.0, 0.0, 1.0 };
    
    transformVector(out, vf, (const LXFloat *)mm);
    
    vf[0] = out[0];
    vf[1] = out[1];
    vf[2] = out[2];
    vf[3] = out[3];
}

void LXTransform3DTransformDistance(LXTransform3DRef t, LXFloat *x, LXFloat *y, LXFloat *z)
{
    if ( !t) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);

    // first transform origin
    LXFloat outOrigin[4] = { 0.0, 0.0, 0.0, 1.0 };
    LXFloat inOrigin[4] = { 0.0, 0.0, 0.0, 1.0 };
    
    transformVector(outOrigin, inOrigin, (const LXFloat *)mm);
    
    // then the actual vector
    LXFloat out[4] = { 0.0, 0.0, 0.0, 1.0 };
    LXFloat in[4] = { (x) ? *x : 0.0, (y) ? *y : 0.0, (z) ? *z : 0.0, 1.0 };
    
    transformVector(out, in, (const LXFloat *)mm);
    
    if (x) *x = out[0] - outOrigin[0];
    if (y) *y = out[1] - outOrigin[1];
    if (z) *z = out[2] - outOrigin[2];
}

void LXTransform3DTransformVector4ArrayInPlace(LXTransform3DRef t, LXFloat *varr, size_t vecCount)
{
    if ( !t) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);

    LXFloat out[4] = { 0.0, 0.0, 0.0, 1.0 };

    LXUInteger i;
    for (i = 0; i < vecCount; i++) {
        LXFloat * LXRESTRICT vf = varr + i*4;
        
        transformVector(out, vf, (const LXFloat *)mm);
    
        vf[0] = out[0];
        vf[1] = out[1];
        vf[2] = out[2];
        vf[3] = out[3];
    }
}

void LXTransform3DTransformVector4ArrayInPlace_f(LXTransform3DRef t, float *varr, size_t vecCount)
{
    if ( !t) return;
    LXUInteger i;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);
    float mm_f[16];
    for (i = 0; i < 16; i++) {
        mm_f[i] = (float)(((LXFloat *)mm)[i]);
    }

    float out[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    for (i = 0; i < vecCount; i++) {
        float * LXRESTRICT vf = varr + i*4;
        
        transformVector_f(out, vf, mm_f);
    
        vf[0] = out[0];
        vf[1] = out[1];
        vf[2] = out[2];
        vf[3] = out[3];
    }
}

void LXTransform3DTransformVector4Array(LXTransform3DRef t, LXFloat * LXRESTRICT src, LXFloat * LXRESTRICT dst, size_t vecCount)
{
    if ( !t) return;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);

    LXFloat * LXRESTRICT sf = src;
    LXFloat * LXRESTRICT df = dst;
    LXUInteger i;
    for (i = 0; i < vecCount; i++) {
        transformVector(df, sf, (const LXFloat *)mm);    
        sf += 4;
        df += 4;
    }
}

void LXTransform3DTransformVector4Array_f(LXTransform3DRef t, float * LXRESTRICT src, float * LXRESTRICT dst, size_t vecCount)
{
    if ( !t) return;
    LXUInteger i;
    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);
    float mm_f[16];
    for (i = 0; i < 16; i++) {
        mm_f[i] = (float)(((LXFloat *)mm)[i]);
    }
    
#if defined(__SSE2__)
    lx_matmul_sse3_aligned(mm_f, src, dst, vecCount);
    
#else
    float * LXRESTRICT sf = src;
    float * LXRESTRICT df = dst;
    for (i = 0; i < vecCount; i++) {
        transformVector_f(df, sf, mm_f);
        sf += 4;
        df += 4;
    }
#endif
}


#pragma mark --- invert ---


// this function uses OpenGL-style row/col ordering
//
static LXBool glstyle_invert_matrix_3d_general( LXFloat *mOut, LXFloat *mOrig )
{
   LXFloat pos, neg, t;
   LXFloat det;
   LXFloat mIn[16];
   memcpy(mIn, mOrig, 16*sizeof(LXFloat));
   
   memset(mOut, 0, 16*sizeof(LXFloat));

   // calculate the determinant of upper left 3x3 submatrix and determine if the matrix is singular

   pos = neg = 0.0;
   t =  MAT(mIn,0,0) * MAT(mIn,1,1) * MAT(mIn,2,2);
   if (t >= 0.0) pos += t; else neg += t;

   t =  MAT(mIn,1,0) * MAT(mIn,2,1) * MAT(mIn,0,2);
   if (t >= 0.0) pos += t; else neg += t;

   t =  MAT(mIn,2,0) * MAT(mIn,0,1) * MAT(mIn,1,2);
   if (t >= 0.0) pos += t; else neg += t;

   t = -MAT(mIn,2,0) * MAT(mIn,1,1) * MAT(mIn,0,2);
   if (t >= 0.0) pos += t; else neg += t;

   t = -MAT(mIn,1,0) * MAT(mIn,0,1) * MAT(mIn,2,2);
   if (t >= 0.0) pos += t; else neg += t;

   t = -MAT(mIn,0,0) * MAT(mIn,2,1) * MAT(mIn,1,2);
   if (t >= 0.0) pos += t; else neg += t;

   det = pos + neg;

   if (det*det < 1e-25)
      return NO;

   det = 1.0f / det;
   MAT(mOut,0,0) = (  (MAT(mIn,1,1)*MAT(mIn,2,2) - MAT(mIn,2,1)*MAT(mIn,1,2) )*det);
   MAT(mOut,0,1) = (- (MAT(mIn,0,1)*MAT(mIn,2,2) - MAT(mIn,2,1)*MAT(mIn,0,2) )*det);
   MAT(mOut,0,2) = (  (MAT(mIn,0,1)*MAT(mIn,1,2) - MAT(mIn,1,1)*MAT(mIn,0,2) )*det);
   MAT(mOut,1,0) = (- (MAT(mIn,1,0)*MAT(mIn,2,2) - MAT(mIn,2,0)*MAT(mIn,1,2) )*det);
   MAT(mOut,1,1) = (  (MAT(mIn,0,0)*MAT(mIn,2,2) - MAT(mIn,2,0)*MAT(mIn,0,2) )*det);
   MAT(mOut,1,2) = (- (MAT(mIn,0,0)*MAT(mIn,1,2) - MAT(mIn,1,0)*MAT(mIn,0,2) )*det);
   MAT(mOut,2,0) = (  (MAT(mIn,1,0)*MAT(mIn,2,1) - MAT(mIn,2,0)*MAT(mIn,1,1) )*det);
   MAT(mOut,2,1) = (- (MAT(mIn,0,0)*MAT(mIn,2,1) - MAT(mIn,2,0)*MAT(mIn,0,1) )*det);
   MAT(mOut,2,2) = (  (MAT(mIn,0,0)*MAT(mIn,1,1) - MAT(mIn,1,0)*MAT(mIn,0,1) )*det);

   // translation
   MAT(mOut,0,3) = - (MAT(mIn,0,3) * MAT(mOut,0,0) +
                      MAT(mIn,1,3) * MAT(mOut,0,1) +
                      MAT(mIn,2,3) * MAT(mOut,0,2) );
   MAT(mOut,1,3) = - (MAT(mIn,0,3) * MAT(mOut,1,0) +
                      MAT(mIn,1,3) * MAT(mOut,1,1) +
                      MAT(mIn,2,3) * MAT(mOut,1,2) );
   MAT(mOut,2,3) = - (MAT(mIn,0,3) * MAT(mOut,2,0) +
                      MAT(mIn,1,3) * MAT(mOut,2,1) +
                      MAT(mIn,2,3) * MAT(mOut,2,2) );
   return YES;
}


LXBool LXTransform3DInvert(LXTransform3DRef t)
{
    if ( !t) return NO;

    LXTransform3DImpl *imp = (LXTransform3DImpl *)t;
    LX4x4Matrix *mm = &(imp->mat);
    LX4x4Matrix tempM;
    LX4x4Matrix newM;
    
    LX4x4MatrixTranspose(&tempM, mm);
    
    if (YES == glstyle_invert_matrix_3d_general((LXFloat *)(&newM), (LXFloat *)(&tempM))) {
        LX4x4MatrixTranspose(mm, &newM);
        return YES;
    } else
        return NO;
}


#pragma mark --- utils ---

void LXTransform3DConcatPerspective(LXTransform3DRef a, double fovYInRad, double aspect, double zNear, double zFar)
{
    if (zNear == zFar) {
        LXPrintf("** %s: invalid near/far values (%f, %f)\n", __func__, zNear, zFar);
        return;
    }

    // algorithm based on gluPerspective man page
    double zdm = 1.0 / (zNear - zFar);
    
    double f = 1.0 / tan(fovYInRad / 2.0);  // f = cotangent(fovy/2)
    
    LX4x4Matrix m;
    memset(&m, 0, sizeof(m));
    
    m.m11 = f / aspect;
    m.m22 = f;
    m.m33 = (zFar + zNear) * zdm;
    m.m34 = (2.0 * zFar * zNear) * zdm;
    m.m43 = -1.0;
    
    LXTransform3DConcatMatrix(a, &m);
}


LXINLINE void crossProduct3D(double aX, double aY, double aZ,
                                  double bX, double bY, double bZ,
                                  double *outX, double *outY, double *outZ)
{
    double x, y, z;
    x = aY * bZ - aZ * bY;
    y = aZ * bX - aX * bZ;
    z = aX * bY - aY * bX;
    *outX = x;
    *outY = y;
    *outZ = z;
}

void LXTransform3DConcatLookAt(LXTransform3DRef a,          double eyeX, double eyeY, double eyeZ,
                                                            double centerX, double centerY, double centerZ,
                                                            double upX, double upY, double upZ)
{
    // algorithm based on gluLookAt man page
    double Fx = centerX - eyeX;
    double Fy = centerY - eyeY;
    double Fz = centerZ - eyeZ;
    
    // normalize F and up vecs
    double sumOfSq, len;
    
    sumOfSq = Fx*Fx + Fy*Fy + Fz*Fz;
    if (sumOfSq == 0.0) return;  // vector length is 0
    
    len = sqrt(sumOfSq);
    Fx /= len;
    Fy /= len;
    Fz /= len;

    sumOfSq = upX*upX + upY*upY + upZ*upZ;
    if (sumOfSq == 0.0) return;  // vector length is 0
    
    len = sqrt(sumOfSq);
    upX /= len;
    upY /= len;
    upZ /= len;
    
    double sx, sy, sz;
    crossProduct3D(Fx, Fy, Fz,  upX, upY, upZ,  &sx, &sy, &sz);
    
    double ux, uy, uz;
    crossProduct3D(sx, sy, sz,  Fx, Fy, Fz,  &ux, &uy, &uz);
    
    
    LX4x4Matrix m;
    memset(&m, 0, sizeof(m));
    
    m.m11 = sx;
    m.m12 = sy;
    m.m13 = sz;
    
    m.m21 = ux;
    m.m22 = uy;
    m.m23 = uz;
    
    m.m31 = -Fx;
    m.m32 = -Fy;
    m.m33 = -Fz;
    
    m.m44 = 1.0;
    
    LXTransform3DConcatMatrix(a, &m);
    LXTransform3DTranslate(a, -eyeX, -eyeY, -eyeZ);
}


void LXTransform3DConcatExactPixelsTransformForSurfaceSize(LXTransform3DRef a, double w, double h, LXBool flip)
{
    if (w == 0.0 || h == 0.0) {
        LXPrintf("*** %s: zero size given\n", __func__);
        return;
    }

    // from LQGLPbuffer.m: 
    //glScalef (2.0f / width, -2.0f /  height, 1.0f);
    //glTranslatef (-width / 2.0f, -height / 2.0f, 0.0f);
    
    // ... these need to be applied in inverse order to get the transform that we expect
    if (flip) {
        LXTransform3DScale(a,  1, -1, 1);
        LXTransform3DTranslate(a,  0, h, 0);
    }
    
    LXTransform3DTranslate(a,  w * -0.5,  h * -0.5,  0.0);
    LXTransform3DScale(a,  2.0/w,  2.0/h,  1.0);
}

