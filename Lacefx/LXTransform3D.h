/*
 *  LXTransform3D.h
 *  Lacefx API
 *
 *  Created by Pauli Ojala on 19.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXTRANSFORM3D_H_
#define _LXTRANSFORM3D_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"
#include <math.h>


typedef struct _LX4x4Matrix
{
    LXFloat m11, m12, m13, m14;
    LXFloat m21, m22, m23, m24;
    LXFloat m31, m32, m33, m34;
    LXFloat m41, m42, m43, m44;
} LX4x4Matrix;


#ifndef M_PI
#define M_PI (3.1415926536)
#endif

#define LXRadFromDeg(d_)  (d_ * (M_PI / 180.0))
#define LXDegFromRad(d_)  (d_ * (180.0 / M_PI))


#ifdef __cplusplus
extern "C" {
#endif


LXEXPORT void LX4x4MatrixTranspose(LX4x4Matrix *dst, const LX4x4Matrix *src);  // can be used for in-place operation
LXEXPORT char *LX4x4MatrixCreateString(const LX4x4Matrix *m, LXBool insertLineBreaks);  // created string must be destroyed with _lx_free()

LXEXPORT_CONSTVAR LX4x4Matrix * const LXIdentity4x4Matrix;


#pragma mark --- public API ---

LXEXPORT LXTransform3DRef LXTransform3DRetain(LXTransform3DRef r);
LXEXPORT void LXTransform3DRelease(LXTransform3DRef r);

LXEXPORT LXTransform3DRef LXTransform3DCreateIdentity();
LXEXPORT LXTransform3DRef LXTransform3DCreateWithMatrix(LX4x4Matrix *matrix);

LXEXPORT LXTransform3DRef LXTransform3DCopy(LXTransform3DRef r);

LXEXPORT LXTransform3DRef LXTransform3DCreateWithTranslation(LXFloat tx, LXFloat ty, LXFloat tz);
LXEXPORT LXTransform3DRef LXTransform3DCreateWithScale(LXFloat sx, LXFloat sy, LXFloat sz);
LXEXPORT LXTransform3DRef LXTransform3DCreateWithRotation(LXFloat angle, LXFloat x, LXFloat y, LXFloat z);

LXEXPORT LXTransform3DRef LXTransform3DCreateWith2DTransform(LXFloat m11, LXFloat m12, LXFloat m21, LXFloat m22, LXFloat tX, LXFloat tY);

LXEXPORT void LXTransform3DSetIdentity(LXTransform3DRef r);
LXEXPORT void LXTransform3DTranslate(LXTransform3DRef r, LXFloat tx, LXFloat ty, LXFloat tz);
LXEXPORT void LXTransform3DScale(LXTransform3DRef r, LXFloat sx, LXFloat sy, LXFloat sz);
LXEXPORT void LXTransform3DRotate(LXTransform3DRef r, LXFloat angleInRadians, LXFloat x, LXFloat y, LXFloat z);

LXEXPORT void LXTransform3DGetMatrix(LXTransform3DRef t, LX4x4Matrix *outMatrix);
LXEXPORT void LXTransform3DSetMatrix(LXTransform3DRef t, LX4x4Matrix *matrix);

// apply transform to vector
LXEXPORT void LXTransform3DTransformVector(LXTransform3DRef t, LXFloat *x, LXFloat *y, LXFloat *z);  // accepts NULL for x/y/z
LXEXPORT void LXTransform3DTransformDistance(LXTransform3DRef t, LXFloat *x, LXFloat *y, LXFloat *z);

LXEXPORT void LXTransform3DTransformVector4Ptr(LXTransform3DRef t, LXFloat *vf);
LXEXPORT void LXTransform3DTransformVector4ArrayInPlace(LXTransform3DRef t, LXFloat *vf, size_t vecCount);
LXEXPORT void LXTransform3DTransformVector4Array(LXTransform3DRef t, LXFloat * LXRESTRICT src, LXFloat * LXRESTRICT dst, size_t vecCount);

LXEXPORT void LXTransform3DTransformVector4ArrayInPlace_f(LXTransform3DRef t, float *vf, size_t vecCount);
LXEXPORT void LXTransform3DTransformVector4Array_f(LXTransform3DRef t, float * LXRESTRICT src, float * LXRESTRICT dst, size_t vecCount);

// a = a * b
LXEXPORT void LXTransform3DConcat(LXTransform3DRef a, LXTransform3DRef b);
LXEXPORT void LXTransform3DConcatMatrix(LXTransform3DRef a, LX4x4Matrix * LXRESTRICT matrix);

// inverts the matrix, if possible
LXEXPORT LXBool LXTransform3DInvert(LXTransform3DRef t);


#pragma mark --- utilities ---

// equal to gluPerspective except that fovY is given in radians
LXEXPORT void LXTransform3DConcatPerspective(LXTransform3DRef a, double fovYInRadians, double aspect, double zNear, double zFar);

// equal to gluLookAt
LXEXPORT void LXTransform3DConcatLookAt(LXTransform3DRef a, double eyeX, double eyeY, double eyeZ,
                                                            double centerX, double centerY, double centerZ,
                                                            double upX, double upY, double upZ);

// creates a model view transform that applies a 1:1 pixel mapping for the given surface size.
// creating this matrix explicitly is not generally necessary because the default projection matrix for a surface is equivalent.
// 'flip' parameter is useful because on OpenGL this typically varies (pbuffers use top-left origin, while views use bottom-left).
LXEXPORT void LXTransform3DConcatExactPixelsTransformForSurfaceSize(LXTransform3DRef a, double w, double h, LXBool flip);

#ifdef __cplusplus
}
#endif

#endif


