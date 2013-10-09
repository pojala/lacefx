/*
 *  LXCurveFunctions.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 27.10.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXCURVEFUNCTIONS_H_
#define _LXCURVEFUNCTIONS_H_

#include "LXCurveTypes.h"
#include "LXBasicTypeFunctions.h"

#ifdef __cplusplus
extern "C" {
#endif


LXEXPORT void LXCalcCurve(LXCurveSegment seg, const LXInteger steps, LXPoint *outArray);

LXEXPORT LXFUNCATTR_PURE LXPoint LXCalcCurvePointAtU(LXCurveSegment seg, LXFloat u);

LXEXPORT LXFUNCATTR_PURE LXFloat LXEstimateUForCurveAtX(LXCurveSegment seg, LXFloat x);


// called by LXCalcCurve for curves of type kLXInOutBezierSegment
LXEXPORT void LXCalcBezierCurve(LXCurveSegment seg, const LXInteger steps, LXPoint *outArray);  

// called by LXCalcCurve for curves of type kLXHermiteSegment and kLXCatmullRomSegment
LXEXPORT void LXCalcHermiteCurve(LXCurveSegment seg, const LXInteger steps, LXPoint *outArray);

LXINLINE LXFUNCATTR_PURE LXPoint LXLinearSegmentPointAtU(const LXCurveSegment seg, const LXFloat u) {
    LXPoint p;
    p.x = seg.startPoint.x + (seg.endPoint.x - seg.startPoint.x) * u;
    p.y = seg.startPoint.y + (seg.endPoint.y - seg.startPoint.y) * u;
    return p;
}

// this produces the canonical tangent points for "kLXCatmullRomSegment" and "kLXCardinalSegment" curves
// 
LXEXPORT LXCurveSegment LXCalcAutoTangentsForCurveSegment(LXCurveSegment seg);


// generating arcs
//
LXEXPORT void LXCreateBezierCurvesForArc(LXFloat xc, LXFloat yc, LXFloat radius,
                                         LXFloat angleMin, LXFloat angleMax,
                                         LXBool isForward,
                                         LXCurveSegment **outSegs, size_t *outSegCount);

// binary serialisation utility
//
LXEXPORT size_t LXCurveArrayGetSerializedDataSize(const LXCurveSegment *curves, LXUInteger curveCount);
LXEXPORT LXSuccess LXCurveArraySerialize(const LXCurveSegment *curves, LXUInteger curveCount, uint8_t *buf, size_t bufLen);

LXEXPORT LXSuccess LXCurveArrayCreateFromSerializedData(const uint8_t *buf, size_t dataLen,
                                                        LXCurveSegment **curves, LXUInteger *curveCount,
                                                        LXError *outError);

// plotting lines to pixels (Bresenham line algorithm)
//
LXEXPORT LXSuccess LXPlotLinearSegment(LXPoint p0, LXPoint p1, LXPoint **outPixels, LXUInteger *outPixelCount);

#ifdef __cplusplus
}
#endif


#endif // LXCURVEFUNCTIONS_H
