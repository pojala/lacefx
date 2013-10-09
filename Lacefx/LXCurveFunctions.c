/*
 *  LXCurveFunctions.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 27.10.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXCurveFunctions.h"
#include "LXStringUtils.h"
#include <math.h>


#pragma mark --- beziers ---

// c argument is coefficient array (4 LXPoints)
LXINLINE LXPoint getBezierCurvePoint(const LXPoint *c, const double u)
{
    LXPoint p;
    p.x = c[0].x + u * (c[1].x + u * (c[2].x + u * c[3].x));
    p.y = c[0].y + u * (c[1].y + u * (c[2].y + u * c[3].y));
    return p;
}

// these four coefficients are equivalent to the ABCD parameters in some source material.
// (however the coeffs are in opposite order: coeff[0] is "D", coeff[3] is "A")
static void parametrizeBezierCurve(LXPoint *coefficients,
                                   LXPoint startPoint, LXPoint endPoint, LXPoint controlPoint1, LXPoint controlPoint2)
{
    LXPoint tangent2;
    tangent2.x = 3.0 * (endPoint.x - controlPoint2.x);
    tangent2.y = 3.0 * (endPoint.y - controlPoint2.y);

    coefficients[0] = startPoint;
    coefficients[1].x = 3.0 * (controlPoint1.x - startPoint.x);  // 1st tangent
    coefficients[1].y = 3.0 * (controlPoint1.y - startPoint.y);  // 1st tangent
    coefficients[2].x = 3.0 * (endPoint.x - startPoint.x) - 2.0 * coefficients[1].x - tangent2.x;
    coefficients[2].y = 3.0 * (endPoint.y - startPoint.y) - 2.0 * coefficients[1].y - tangent2.y;
    coefficients[3].x = 2.0 * (startPoint.x - endPoint.x) + coefficients[1].x + tangent2.x;
    coefficients[3].y = 2.0 * (startPoint.y - endPoint.y) + coefficients[1].y + tangent2.y;
}

LXINLINE void setControlPointForInOrOutBezSegType(LXCurveSegment *seg)
{
    if ( !seg)
        return;
    else if (LXCurveSegmentTypeNoFlags(seg->type) == kLXInBezierSegment) {
        seg->controlPoint1 = LXLinearSegmentPointAtU(*seg, 1.0/3.0);
    }
    else if (LXCurveSegmentTypeNoFlags(seg->type) == kLXOutBezierSegment) {
        seg->controlPoint1 = LXLinearSegmentPointAtU(*seg, 2.0/3.0);
    }
}

void LXCalcBezierCurve(LXCurveSegment seg, const LXInteger steps, LXPoint *outArray)
{
    if ( !outArray || steps < 1) return;
    
    setControlPointForInOrOutBezSegType(&seg);

    LXPoint coeffs[4];
    parametrizeBezierCurve(coeffs,  seg.startPoint, seg.endPoint, seg.controlPoint1, seg.controlPoint2);
    
    LXInteger i;
    for (i = 0; i < steps; i++) {
        const double u = (double)i / steps;
        outArray[i] = getBezierCurvePoint((const LXPoint *)coeffs, u);
    }
}


#pragma mark --- hermite/cardinal ---

LXINLINE LXFloat segmentLength(LXPoint a, LXPoint b)
{
    LXFloat x = (b.x - a.x);
    LXFloat y = (b.y - a.y);

    return sqrtf(x*x + y*y);
}

LXINLINE LXFloat autoTightnessValueLimitedForSegments(LXFloat segLen, LXFloat cpLen)
{
    const LXFloat base = (LXFloat)2.0;
    const LXFloat ln_base = (LXFloat)0.693147180559945;
    ///LXPrintf("... seglen %.2f, cplen %.2f\n", segLen, cpLen);
    
    if (cpLen < segLen*base) {
        return 0.5f;  // ordinary Catmull-Rom spline
    } else {
        LXFloat l = cpLen / segLen;
        LXFloat v = logf(l) / ln_base;
        
        return 0.5f * (1.0f / v);
    }
}

LXINLINE void setTangentPointsForHermiteOrCardinalSeg(LXCurveSegment *seg)
{
    const LXUInteger type = LXCurveSegmentTypeNoFlags(seg->type);
    const LXBool isCatmull = (type == kLXCatmullRomSegment);
    const LXBool isAutoTangents = (isCatmull || type == kLXCardinalSegment);    
    
    if (isAutoTangents) {
        // cardinal spline tangent points are computed from the previous and next points.
        // these need to be passed as cp1/cp2.
        // formula for the tangents is: T[i] = a * ( P[i+1] - P[i-1] )
        LXPoint p0 = seg->controlPoint1;
        LXPoint p1 = seg->startPoint;
        LXPoint p2 = seg->endPoint;
        LXPoint p3 = seg->controlPoint2;

        // "tightness constant" is 0.5 for catmull-rom splines
        /*
        const LXFloat cardinalA1 = (isCatmull) ? 0.5 : seg->tangentInfo.x;
        const LXFloat cardinalA2 = (isCatmull) ? 0.5 : seg->tangentInfo.y;
        */
        LXFloat cardinalA1;
        LXFloat cardinalA2;
        if ( !isCatmull) {
            cardinalA1 = seg->tangentInfo.x;
            cardinalA2 = seg->tangentInfo.y;
        } else {
            // 2010.04.11 -- added this "limiter" mechanism to keep the points tighter when the control points are far away from the segment.
            // (technically these are not pure catmull-rom curves anymore, but I don't think anyone cares...? these curves are used for purely visual purposes in Conduit.)
            LXFloat segLen = segmentLength(seg->startPoint, seg->endPoint);
            LXFloat cp1Len = segmentLength(seg->controlPoint1, seg->endPoint);
            LXFloat cp2Len = segmentLength(seg->startPoint, seg->controlPoint2);
            
            cardinalA1 = autoTightnessValueLimitedForSegments(segLen, cp1Len);
            cardinalA2 = autoTightnessValueLimitedForSegments(segLen, cp2Len);
        }
        
        LXFloat t1x = cardinalA1 * (p2.x - p0.x);
        LXFloat t1y = cardinalA1 * (p2.y - p0.y);
        
        LXFloat t2x = cardinalA2 * (p3.x - p1.x);
        LXFloat t2y = cardinalA2 * (p3.y - p1.y);
        
        /*printf("... %s, tightness is %.2f, %.2f; computed cardinal points: (%.2f, %.2f) - %.2f, %.2f - %.2f, %.2f - (%.2f, %.2f) --> (%.3f, %.3f), (%.3f, %.3f)\n",
                    (isCatmull) ? "catmull" : "cardinal",
                    cardinalA1, cardinalA2,
                    seg->controlPoint1.x, seg->controlPoint1.y,
                    seg->startPoint.x, seg->startPoint.y,
                    seg->endPoint.x, seg->endPoint.y,
                    seg->controlPoint2.x, seg->controlPoint2.y,
                    t1x, t1y,  t2x, t2y);*/
        
        seg->controlPoint1 = LXMakePoint(t1x, t1y);
        seg->controlPoint2 = LXMakePoint(t2x, t2y);
    }    
}

LXCurveSegment LXCalcAutoTangentsForCurveSegment(LXCurveSegment seg)
{
    setTangentPointsForHermiteOrCardinalSeg(&seg);
    return seg;
}

void LXCalcHermiteCurve(LXCurveSegment seg, const LXInteger steps, LXPoint *outArray)
{
/* pseudocode from http://www.cubic.org/docs/hermite.htm :
 moveto (P1);                            // move pen to startpoint
 for (int t=0; t < steps; t++)
 {
  float s = (float)t / (float)steps;    // scale s to go from 0 to 1
  float h1 =  2s^3 - 3s^2 + 1;          // calculate basis function 1
  float h2 = -2s^3 + 3s^2;              // calculate basis function 2
  float h3 =   s^3 - 2*s^2 + s;         // calculate basis function 3
  float h4 =   s^3 -  s^2;              // calculate basis function 4
  vector p = h1*P1 +                    // multiply and sum all funtions
             h2*P2 +                    // together to build the interpolated
             h3*T1 +                    // point along the curve.
             h4*T2;
  lineto (p)                            // draw to calculated point on the curve
 }
*/
    if ( !outArray || steps < 1) return;
    
    setTangentPointsForHermiteOrCardinalSeg(&seg);

    // hermite curve parameter matrix
    const LXFloat m11 =  2.0,  m12 = -3.0,  m14 =  1.0;
    const LXFloat m21 = -2.0,  m22 =  3.0;
    const LXFloat m31 =  1.0,  m32 = -2.0,  m33 =  1.0;
    const LXFloat m41 =  1.0,  m42 = -1.0;

    LXInteger i;
    for (i = 0; i < steps; i++) {
        const LXFloat u = (LXFloat)i / steps;
        const LXFloat u2 = u * u;
        const LXFloat u3 = u2 * u;
        const LXFloat h1 =  m11*u3 + m12*u2         + m14 ;
        const LXFloat h2 =  m21*u3 + m22*u2 ;
        const LXFloat h3 =  m31*u3 + m32*u2 + m33*u ;
        const LXFloat h4 =  m41*u3 + m42*u2 ;
        
        LXFloat x = h1*seg.startPoint.x + h2*seg.endPoint.x + h3*seg.controlPoint1.x + h4*seg.controlPoint2.x;
        LXFloat y = h1*seg.startPoint.y + h2*seg.endPoint.y + h3*seg.controlPoint1.y + h4*seg.controlPoint2.y;
        
        outArray[i] = LXMakePoint(x, y);
    }
}

LXINLINE LXPoint hermiteOrCardinalCurvePointAtU(LXCurveSegment seg, LXFloat u)
{
    setTangentPointsForHermiteOrCardinalSeg(&seg);

    // hermite curve parameter matrix
    const LXFloat m11 =  2.0,  m12 = -3.0,  m14 =  1.0;
    const LXFloat m21 = -2.0,  m22 =  3.0;
    const LXFloat m31 =  1.0,  m32 = -2.0,  m33 =  1.0;
    const LXFloat m41 =  1.0,  m42 = -1.0;

    const LXFloat u2 = u * u;
    const LXFloat u3 = u2 * u;    
    const LXFloat h1 =  m11*u3 + m12*u2         + m14 ;
    const LXFloat h2 =  m21*u3 + m22*u2 ;
    const LXFloat h3 =  m31*u3 + m32*u2 + m33*u ;
    const LXFloat h4 =  m41*u3 + m42*u2 ;
    
    LXFloat x = h1*seg.startPoint.x + h2*seg.endPoint.x + h3*seg.controlPoint1.x + h4*seg.controlPoint2.x;
    LXFloat y = h1*seg.startPoint.y + h2*seg.endPoint.y + h3*seg.controlPoint1.y + h4*seg.controlPoint2.y;
        
    return LXMakePoint(x, y);
}


LXPoint LXCalcCurvePointAtU(LXCurveSegment seg, LXFloat u)
{
    const LXUInteger type = LXCurveSegmentTypeNoFlags(seg.type);
    switch (type) {
        case kLXLinearSegment:
            return LXLinearSegmentPointAtU(seg, u);
        
        case kLXInBezierSegment:
        case kLXOutBezierSegment:
        case kLXInOutBezierSegment: {
            LXPoint coeffs[4];
            setControlPointForInOrOutBezSegType(&seg);
            parametrizeBezierCurve(coeffs,  seg.startPoint, seg.endPoint, seg.controlPoint1, seg.controlPoint2);
            return getBezierCurvePoint((const LXPoint *)coeffs, u);
        }
        
        case kLXHermiteSegment:
        case kLXCardinalSegment:
        case kLXCatmullRomSegment:
            return hermiteOrCardinalCurvePointAtU(seg, u);
        
        default:
            printf("** %s: unknown curve type (%i)\n", __func__, (int)seg.type);
            return LXZeroPoint;            
    }
}

void LXCalcCurve(LXCurveSegment seg, const LXInteger steps, LXPoint *outArray)
{
    const LXUInteger type = LXCurveSegmentTypeNoFlags(seg.type);
    switch (type) {
        case kLXLinearSegment: {
            LXInteger i;
            for (i = 0; i < steps; i++) {
                const LXFloat u = (LXFloat)i / steps;
                outArray[i] = LXLinearSegmentPointAtU(seg, u);
            }
            return;
        }
            
        case kLXInBezierSegment:
        case kLXOutBezierSegment:
        case kLXInOutBezierSegment:
            LXCalcBezierCurve(seg, steps, outArray);
            return;
            
        case kLXHermiteSegment:
        case kLXCardinalSegment:
        case kLXCatmullRomSegment:
            LXCalcHermiteCurve(seg, steps, outArray);
            return;
    }
}


#pragma mark --- estimation of U ---

LXINLINE LXFloat calcUForLinearCurveAtX(LXCurveSegment seg, LXFloat x)
{
    LXFloat lenX = seg.endPoint.x - seg.startPoint.x;
    x -= seg.startPoint.x;

    if (lenX != 0.0) {
        return (x / lenX);
    } else
        return 0.0;
}

LXINLINE LXFloat estimateUForBezierCurveAtX(LXCurveSegment seg, LXFloat x)
{
    setControlPointForInOrOutBezSegType(&seg);

    LXPoint coeffs[4];
    parametrizeBezierCurve(coeffs,  seg.startPoint, seg.endPoint, seg.controlPoint1, seg.controlPoint2);

    // start guessing at u == linf(x)
    double u = calcUForLinearCurveAtX(seg, x);
    
    // do N iterations of newton approximation
    LXInteger i;
    for (i = 0; i < 3; i++) {
        double guess = ((coeffs[3].x*u + coeffs[2].x)*u + coeffs[1].x)*u + coeffs[0].x;

        double deriv = (3.0*coeffs[3].x*u + 2.0*coeffs[2].x)*u + coeffs[1].x;
    
        if (deriv == 0.0)  break;
        u = u - (guess - x) / deriv;
    }
    
    return u;
}

LXINLINE LXFloat estimateUForHermiteCurveAtX(LXCurveSegment seg, LXFloat x)
{
    setTangentPointsForHermiteOrCardinalSeg(&seg);

    // hermite curve parameter matrix
    const LXFloat m11 =  2.0,  m12 = -3.0,  m14 =  1.0;
    const LXFloat m21 = -2.0,  m22 =  3.0;
    const LXFloat m31 =  1.0,  m32 = -2.0,  m33 =  1.0;
    const LXFloat m41 =  1.0,  m42 = -1.0;

    // start guessing at u == linf(x)
    double u = calcUForLinearCurveAtX(seg, x);
    
    // do N iterations of newton approximation
    LXInteger i;
    for (i = 0; i < 3; i++) {
        const LXFloat u2 = u * u;
        const LXFloat u3 = u * u2;
        const LXFloat h1 =  m11*u3 + m12*u2         + m14 ;
        const LXFloat h2 =  m21*u3 + m22*u2 ;
        const LXFloat h3 =  m31*u3 + m32*u2 + m33*u ;
        const LXFloat h4 =  m41*u3 + m42*u2 ;
    
        LXFloat guess = h1*seg.startPoint.x + h2*seg.endPoint.x + h3*seg.controlPoint1.x + h4*seg.controlPoint2.x;
        
        LXFloat d_h1 = (m11*3.0)*u2 + (m12*2.0)*u;
        LXFloat d_h2 = (m21*3.0)*u2 + (m22*2.0)*u;
        LXFloat d_h3 = (m31*3.0)*u2 + (m32*2.0)*u + m33;
        LXFloat d_h4 = (m41*3.0)*u2 + (m42*2.0)*u;
        
        LXFloat deriv = d_h1*seg.startPoint.x + d_h2*seg.endPoint.x + d_h3*seg.controlPoint1.x + d_h4*seg.controlPoint2.x;

        ///if (x > 0.9) {
        ///    printf("x is %.3f:  guess %i, u was %.3f --> %.3f\n", x, i, u, u-(guess-x)/deriv);
        ///}
        
        if (deriv == 0.0)  break;
        u = u - (guess - x) / deriv;        
    }
    
    return u;
}

LXFloat LXEstimateUForCurveAtX(LXCurveSegment seg, LXFloat x)
{
    const LXUInteger type = LXCurveSegmentTypeNoFlags(seg.type);
    switch (type) {
        case kLXLinearSegment:      return calcUForLinearCurveAtX(seg, x);
        
        case kLXInBezierSegment:
        case kLXOutBezierSegment:
        case kLXInOutBezierSegment:
                                    return estimateUForBezierCurveAtX(seg, x);
        
        case kLXHermiteSegment:
        case kLXCardinalSegment:
        case kLXCatmullRomSegment:
                                    return estimateUForHermiteCurveAtX(seg, x);
        
        default:
            printf("** %s: unknown curve type (%i)\n", __func__, (int)seg.type);
            return 0.0;
    }
}


#pragma mark --- serialization ---

#pragma pack(push, 1)

typedef struct {
    uint32_t cookie;
    uint32_t flags;
    uint64_t curveCount;
    
    uint32_t metadataSizeInBytes;  // this is effectively the offset to the actual curve data
    uint32_t _reserved[32-5];
    
    char buf[256];
} LXCurveArrayFlat;

#pragma pack(pop)


#define FLATHEADERSIZE (32 * sizeof(uint32_t))

#define FLATCOOKIE              0x3401abbf
#define FLATCOOKIE_FLIPPED      0xbfab0134

#define SERPOINT_SIZE           (2*sizeof(double))


LXINLINE size_t serializedSizeForCurveSegmentType(uint32_t type)
{
    LXUInteger realType = LXCurveSegmentTypeNoFlags(type);
    LXBool hasNoStartPoint = (type & kLXCurveSegmentFlag_HasNoStartPoint) ? YES : NO;
    
    size_t segSerSize = 4;  // for type value
    
    segSerSize += (hasNoStartPoint) ? 1 * SERPOINT_SIZE : 2 * SERPOINT_SIZE;
                                         
    if (realType != kLXLinearSegment) {
        segSerSize += 3 * SERPOINT_SIZE;
    }
    return segSerSize;
}

LXINLINE size_t decodedSizeForCurveSegmentType(uint32_t type)
{
    LXBool hasNoStartPoint = (type & kLXCurveSegmentFlag_HasNoStartPoint) ? YES : NO;
    size_t ss = sizeof(LXCurveSegment);
    
    return (hasNoStartPoint) ? (ss - sizeof(LXPoint)) : ss;
}


size_t LXCurveArrayGetSerializedDataSize(const LXCurveSegment *curves, LXUInteger curveCount)
{
    size_t dataSize = FLATHEADERSIZE;
    uint8_t *curvesPtr = (uint8_t *)curves;
    
    LXUInteger i;
    for (i = 0; i < curveCount; i++) {
        const LXCurveSegment *seg = (LXCurveSegment *)curvesPtr;
        ///LXPrintf(".. seg %i / type %i: data size %i\n", i, seg->type, serializedSizeForCurveSegmentType(seg->type));

        dataSize += serializedSizeForCurveSegmentType(seg->type);
        curvesPtr += decodedSizeForCurveSegmentType(seg->type);
    }
    return dataSize;
}

LXSuccess LXCurveArraySerialize(const LXCurveSegment *curves, LXUInteger curveCount, uint8_t *buf, size_t bufLen)
{
    if ( !buf || bufLen < FLATHEADERSIZE)
        return NO;

    LXCurveArrayFlat *flat = (LXCurveArrayFlat *)buf;
    memset(flat, 0, FLATHEADERSIZE);
    flat->cookie = 0x3401abbf;  // determines endianness
    flat->flags = 0;
    flat->curveCount = curveCount;
    flat->metadataSizeInBytes = 0;
    
    size_t dataSize = FLATHEADERSIZE;
    
    // if metadata were written, it should come here before the actual curve data
    
    uint8_t *curvesPtr = (uint8_t *)curves;
    
    LXUInteger i;
    for (i = 0; i < curveCount; i++) {
        const LXCurveSegment *seg = (LXCurveSegment *)curvesPtr;
        LXUInteger realType = LXCurveSegmentTypeNoFlags(seg->type);
        LXBool hasNoStartPoint = (seg->type & kLXCurveSegmentFlag_HasNoStartPoint) ? YES : NO;
        
        size_t segSerSize = serializedSizeForCurveSegmentType(seg->type);
        curvesPtr += decodedSizeForCurveSegmentType(seg->type);
        ///LXPrintf(" serializing %i: realtype is %i; nostartp %i; sersize %i; dataOffset %i\n", i, realType, hasNoStartPoint, segSerSize, (int)dataSize);

        if (dataSize+segSerSize > bufLen) {
            printf("*** %s: out of buffer size (curveCount %ld, index %ld, len %ld)\n", __func__, (long)curveCount, (long)i, (long)bufLen);
            return NO;
        }
        
        uint8_t *segData = buf + dataSize;
        dataSize += segSerSize;
        
        *(uint32_t *)(segData) = (uint32_t)(seg->type & UINT32_MAX);
        segData += 4;
        
        LXPoint pp;
        
        if ( !hasNoStartPoint) {
            pp = seg->startPoint;
            *(double *)(segData + 0) = (double)pp.x;
            *(double *)(segData + 8) = (double)pp.y;
            segData += 16;
        }
        pp = seg->endPoint;
        *(double *)(segData + 0) = (double)pp.x;
        *(double *)(segData + 8) = (double)pp.y;
        segData += 16;
        
        if (realType != kLXLinearSegment) {
            pp = seg->controlPoint1;
            *(double *)(segData + 0) = (double)pp.x;
            *(double *)(segData + 8) = (double)pp.y;
            segData += 16;
            pp = seg->controlPoint2;
            *(double *)(segData + 0) = (double)pp.x;
            *(double *)(segData + 8) = (double)pp.y;
            segData += 16;
            pp = seg->tangentInfo;
            *(double *)(segData + 0) = (double)pp.x;
            *(double *)(segData + 8) = (double)pp.y;
            //segData += 16;
        }
    }
    return YES;
}

LXSuccess LXCurveArrayCreateFromSerializedData(const uint8_t *buf, size_t dataLen,
                                               LXCurveSegment **outCurves, LXUInteger *outCurveCount,
                                               LXError *outError)
{
    if ( !buf || dataLen < FLATHEADERSIZE) {
	    LXErrorSet(outError, 1010, "no input buffer provided or data size is too short");
	    return NO;
    }

    const LXCurveArrayFlat *flat = (const LXCurveArrayFlat *)buf;
    
    if (flat->cookie != FLATCOOKIE && flat->cookie != FLATCOOKIE_FLIPPED) {
        printf("*** %s: invalid cookie in flat data (size %i)\n", __func__, (int)dataLen);
	    LXErrorSet(outError, 1011, "provider data doesn't have the correct identifier, file may be corrupted");
        return NO;
    }
    
    const LXBool dataIsFlipped = (flat->cookie == FLATCOOKIE_FLIPPED);
    const LXUInteger flags = (dataIsFlipped) ? LXEndianSwap_uint32(flat->flags) : flat->flags;
    #pragma unused(flags)
    
    uint64_t decCurveCount = (dataIsFlipped) ? LXEndianSwap_uint64(flat->curveCount) : flat->curveCount;
#if !defined(LX64BIT)
    decCurveCount = MIN(decCurveCount, UINT32_MAX);
#endif
    const LXUInteger curveCount = (LXUInteger)decCurveCount;
    
    const LXUInteger mdSize = (dataIsFlipped) ? LXEndianSwap_uint32(flat->metadataSizeInBytes) : flat->metadataSizeInBytes;
    if (mdSize > 64*1024*1024) {  // sanity check
        printf("*** %s: invalid metadata size (%i)\n", __func__, (int)mdSize);
	    LXErrorSet(outError, 1012, "serialized data has excessive metadata size, may indicate corruption");
        return NO;
    }
    
    uint8_t *mdBuffer = (uint8_t *)flat->buf;
    #pragma unused(mdBuffer)
    uint8_t *curveSerDataBuffer = (uint8_t *)flat->buf + mdSize;

    // we must first calculate the decoded data size (because sizeof(LXCurveSegment) may vary according to flags)
    size_t decodedDataSize = 0;
    
    LXUInteger i;
    for (i = 0; i < curveCount; i++) {
        uint32_t type = *(uint32_t *)(curveSerDataBuffer);
        if (dataIsFlipped)
            type = LXEndianSwap_uint32(type);
        
        decodedDataSize += decodedSizeForCurveSegmentType(type);
        curveSerDataBuffer += serializedSizeForCurveSegmentType(type);
    }
    
    if (decodedDataSize == 0) {
        *outCurves = NULL;
        *outCurveCount = 0;
        return NO;
    }
    
    *outCurves = _lx_malloc(decodedDataSize);
    *outCurveCount = curveCount;
    
    curveSerDataBuffer = (uint8_t *)flat->buf + mdSize;
    uint8_t *decBuffer = (uint8_t *)(*outCurves);
    
    for (i = 0; i < curveCount; i++) {
        uint8_t *serSeg = curveSerDataBuffer;
        uint32_t type = *(uint32_t *)(serSeg);
        if (dataIsFlipped)
            type = LXEndianSwap_uint32(type);

        curveSerDataBuffer += serializedSizeForCurveSegmentType(type);
        serSeg += 4;
        
        LXCurveSegment *decSeg = (LXCurveSegment *)decBuffer;
        decBuffer += decodedSizeForCurveSegmentType(type);
        
        LXUInteger realType = LXCurveSegmentTypeNoFlags(type);
        LXBool hasNoStartPoint = (type & kLXCurveSegmentFlag_HasNoStartPoint) ? YES : NO;
        double x, y;
        
        decSeg->type = type;
        
        if ( !hasNoStartPoint) {
            x = *((double *)(serSeg + 0));
            y = *((double *)(serSeg + 8));
            serSeg += 16;
            decSeg->startPoint = LXMakePoint(x, y);
        }
        x = *((double *)(serSeg + 0));
        y = *((double *)(serSeg + 8));
        serSeg += 16;
        decSeg->endPoint = LXMakePoint(x, y);
        
        if (realType != kLXLinearSegment) {
            x = *((double *)(serSeg + 0));
            y = *((double *)(serSeg + 8));
            serSeg += 16;
            decSeg->controlPoint1 = LXMakePoint(x, y);

            x = *((double *)(serSeg + 0));
            y = *((double *)(serSeg + 8));
            serSeg += 16;
            decSeg->controlPoint2 = LXMakePoint(x, y);

            x = *((double *)(serSeg + 0));
            y = *((double *)(serSeg + 8));
            //serSeg += 16;
            decSeg->tangentInfo = LXMakePoint(x, y);
        } else {
            decSeg->controlPoint1 = LXZeroPoint;
            decSeg->controlPoint2 = LXZeroPoint;
            decSeg->tangentInfo = LXZeroPoint;
        }
    }
    return YES;
}



#pragma mark --- arcs ---

static LXCurveSegment bezierCurveSegmentFromArcSegment(double xc, double yc, double radius, double angle1, double angle2)
{
    LXCurveSegment seg;
    memset(&seg, 0, sizeof(LXCurveSegment));
    seg.type = kLXInOutBezierSegment;

    double r_sin1 = radius * sin(angle1);
    double r_cos1 = radius * cos(angle1);
    double r_sin2 = radius * sin(angle2);
    double r_cos2 = radius * cos(angle2);

    double h = (4.0/3.0) * tan((angle2 - angle1) / 4.0);
    
    seg.startPoint = LXMakePoint(xc + r_cos1, yc + r_sin1);
    
    seg.controlPoint1 = LXMakePoint(xc + r_cos1 - h*r_sin1,
                                    yc + r_sin1 + h*r_cos1);
                                    
    seg.controlPoint2 = LXMakePoint(xc + r_cos2 + h*r_sin2,
                                    yc + r_sin2 - h*r_cos2);

    seg.endPoint =   LXMakePoint(xc + r_cos2, yc + r_sin2);

    return seg;
}

static void appendConstrainedArcToCurveArray(const double xc, const double yc, const double radius,
                                             double angleMin, double angleMax,
                                             LXBool isForward,
                                             LXCurveSegment **pSegs, size_t *pSegCount)
{
    LXInteger segCount = 2;
    double angle;
    double stepLen = (angleMax - angleMin) / (double)segCount;
    
    if (isForward) {
        angle = angleMin;
    } else {
        angle = angleMax;
        stepLen = -stepLen;
    }
    
    LXInteger i;
    for (i = 0; i < segCount; i++) {
        LXCurveSegment seg = bezierCurveSegmentFromArcSegment(xc, yc, radius, angle, angle + stepLen);
        angle += stepLen;
        
        size_t newCount = *pSegCount + 1;
        
        *pSegs = (*pSegs) ? _lx_realloc(*pSegs, newCount * sizeof(LXCurveSegment))
                          :          _lx_malloc(newCount * sizeof(LXCurveSegment));
        
        memcpy(*pSegs + newCount - 1, &seg, sizeof(LXCurveSegment));
        *pSegCount = newCount;
    }
}

void LXCreateBezierCurvesForArc(LXFloat xc, LXFloat yc, LXFloat radius,
                                LXFloat aAngleMin, LXFloat aAngleMax,
                                LXBool isForward,
                                LXCurveSegment **outSegs, size_t *outSegCount)
{
    if ( !outSegs || !outSegCount) return;
    
    if (radius <= 0.0) {
        *outSegs = NULL;
        *outSegCount = 0;
        return;
    }
    
    // constrain angles within 360 degrees (2*PI)
    double angleMin = aAngleMin;
    double angleMax = aAngleMax;
    
    while (angleMax < angleMin) {
        angleMax += 2.0*M_PI;
    }
    while ((angleMax - angleMin) > 4.0*M_PI) {
        angleMax -= 2.0*M_PI;
    }
    
    if (angleMin == angleMax) {
        *outSegs = NULL;
        *outSegCount = 0;
        return;
    }
    
    LXCurveSegment *segs = NULL;
    size_t segCount = 0;
    
    // for arcs longer than a half-circle, render in two parts
    double angleLen = angleMax - angleMin;
    if (angleLen > M_PI) {
        double mid = angleMin + 0.5 * angleLen;
        
        if (isForward) {
            appendConstrainedArcToCurveArray(xc, yc, radius, angleMin, mid, YES, &segs, &segCount);            
            appendConstrainedArcToCurveArray(xc, yc, radius, mid, angleMax, YES, &segs, &segCount);
        } else {
            appendConstrainedArcToCurveArray(xc, yc, radius, mid, angleMax, NO,  &segs, &segCount);        
            appendConstrainedArcToCurveArray(xc, yc, radius, angleMin, mid, NO,  &segs, &segCount);            
        }
    }
    else {
        appendConstrainedArcToCurveArray(xc, yc, radius, angleMin, angleMax, isForward, &segs, &segCount);
    }
    
    *outSegs = segs;
    *outSegCount = segCount;
}


#pragma mark --- plotting lines ---

LXSuccess LXPlotLinearSegment(LXPoint p0, LXPoint p1, LXPoint **outPixels, LXUInteger *outPixelCount)
{
    if ( !outPixels || !outPixelCount) return NO;
    
    p0.x = FLOORF(p0.x);
    p0.y = FLOORF(p0.y);
    p1.x = FLOORF(p1.x);
    p1.y = FLOORF(p1.y);

    LXBool isSteep = FABSF(p1.y - p0.y) > FABSF(p1.x - p0.x);
    if (isSteep) {  // swap x and y
        LXFloat t;
        t = p0.x;
        p0.x = p0.y;
        p0.y = t;
        
        t = p1.x;
        p1.x = p1.y;
        p1.y = t;
    }
    
    LXBool swapPoints = (p0.x > p1.x);
    if (swapPoints) {
        LXPoint t = p0;
        p0 = p1;
        p1 = t;
    }        

    double deltaX = p1.x - p0.x;
    double deltaY = fabs(p1.y - p0.y);
    double error = 0.0;
    double deltaErr = deltaY / deltaX;

    double x = p0.x;
    double y = p0.y;
    double yStep = (p0.y < p1.y) ? 1 : -1;
    
    LXInteger pointCount = ceil(p1.x - p0.x + 0.000001);
    LXPoint *pArray = _lx_malloc(pointCount * sizeof(LXPoint));
    
    LXInteger i;
    for (i = 0; i < pointCount; i++) {
        pArray[i] = (isSteep) ? LXMakePoint(y, x) : LXMakePoint(x, y);

        x += 1.0;
        
        error += deltaErr;
        if (error >= 0.5) {
            y += yStep;
            error -= 1.0;
        }
    }
    
    if (swapPoints) {
        // swap array's direction... this is stupid, but it doesn't get called often enough to matter
        LXPoint *oldArr = pArray;
        pArray = _lx_malloc(pointCount * sizeof(LXPoint));
        for (i = 0; i < pointCount; i++) {
            pArray[i] = oldArr[pointCount-1-i];
        }
        _lx_free(oldArr);
    }
    
    *outPixels = pArray;
    *outPixelCount = pointCount;
    return YES;
}

