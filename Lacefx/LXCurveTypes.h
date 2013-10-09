/*
 *  LXCurveTypes.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 27.10.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXCURVETYPES_H_
#define _LXCURVETYPES_H_

#include "LXBasicTypes.h"


enum {
    kLXLinearSegment = 0,
    
    kLXInBezierSegment,
    kLXOutBezierSegment,
    kLXInOutBezierSegment,
    
    kLXHermiteSegment,      // requires two tangent control points
    kLXCardinalSegment,     // requires a tightness constant in seg.tangentInfo, and previous/next points in cp1/cp2
    kLXCatmullRomSegment,   // same as cardinal segment, but tightness is always 0.5
};
typedef LXUInteger LXCurveSegmentType;


typedef struct _LXCurveSegment {
    LXCurveSegmentType type;
    
    LXPoint endPoint;
	LXPoint controlPoint1;
	LXPoint controlPoint2;
    LXPoint tangentInfo;
    
	LXPoint startPoint;    
} LXCurveSegment;


// LXCurveSegmentType internal flags for implementations that want to optimize storage internally.
// any API that returns individual LXCurveSegment structs must not return these flags, but only the proper values.
enum {
    kLXCurveSegmentFlag_HasNoStartPoint = 1 << 16,  // implementations that maintain curve lists can use this to specify that the startPoint isn't stored
};

#define LXCurveSegmentNoFlagsMask         ((1 << 16) - 1)
#define LXCurveSegmentTypeNoFlags(t_)     ((t_) & LXCurveSegmentNoFlagsMask)


#endif  // _LXCURVETYPES_H_
