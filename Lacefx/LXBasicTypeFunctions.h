/*
 *  LXBasicTypeFunctions.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 27.10.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXBASICTYPEFUNC_H_
#define _LXBASICTYPEFUNC_H_

#include "LXBasicTypes.h"


LXINLINE LXFUNCATTR_PURE LXPoint LXMakePoint(LXFloat x, LXFloat y) {
    LXPoint p = { x, y };
    return p;
}

LXINLINE LXFUNCATTR_PURE LXSize LXMakeSize(LXFloat w, LXFloat h) {
    LXSize s = { w, h };
    return s;
}

LXINLINE LXFUNCATTR_PURE LXRect LXMakeRect(LXFloat x, LXFloat y, LXFloat w, LXFloat h) {
    LXRect r = { x, y, w, h };
    return r;
}

LXINLINE LXFUNCATTR_PURE LXRGBA LXMakeRGBA(LXFloat r, LXFloat g, LXFloat b, LXFloat a) {
    LXRGBA c = { r, g, b, a };
    return c;
}

LXINLINE LXFUNCATTR_PURE LXRGBA LXMakeRGBAFromScalar(LXFloat v) {
    LXRGBA c = { v, v, v, v };
    return c;
}

LXINLINE LXFUNCATTR_PURE LXRGBA LXMakeRGBAFromWhiteAndAlpha(LXFloat w, LXFloat a) {
    LXRGBA c = { w, w, w, a };
    return c;
}

LXINLINE LXFUNCATTR_PURE LXRGBA LXMakeRGBAAndPremultiply(LXFloat r, LXFloat g, LXFloat b, LXFloat a) {
    LXRGBA c = { r*a, g*a, b*a, a };
    return c;
}


LXEXPORT_CONSTVAR LXPoint LXZeroPoint;
LXEXPORT_CONSTVAR LXPoint LXUnitPoint;

LXEXPORT_CONSTVAR LXSize LXZeroSize;
LXEXPORT_CONSTVAR LXSize LXUnitSize;

LXEXPORT_CONSTVAR LXRect LXZeroRect;
LXEXPORT_CONSTVAR LXRect LXUnitRect;

LXEXPORT_CONSTVAR LXRGBA LXZeroRGBA;
LXEXPORT_CONSTVAR LXRGBA LXWhiteRGBA;
LXEXPORT_CONSTVAR LXRGBA LXBlackOpaqueRGBA;


LXINLINE LXFUNCATTR_PURE LXBool LXRGBAIsEqual(LXRGBA x, LXRGBA y) {
    return (x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a) ? NO : YES;
}


LXINLINE LXFUNCATTR_PURE LXBool LXColorFloatIsVisuallyEquivalent(LXFloat a, LXFloat b)
{
#ifdef LXFLOAT_IS_DOUBLE
    return (LXFABS(a - b) < 0.001) ? YES : NO;
#else
    return (FABSF(a - b) < 0.001f) ? YES : NO;
#endif
}

LXINLINE LXFUNCATTR_PURE LXBool LXRGBAIsVisuallyEquivalent(LXRGBA x, LXRGBA y)
{
    return ( !LXColorFloatIsVisuallyEquivalent(x.r, y.r) || !LXColorFloatIsVisuallyEquivalent(x.g, y.g) ||
             !LXColorFloatIsVisuallyEquivalent(x.b, y.b) || !LXColorFloatIsVisuallyEquivalent(x.a, y.a))
                ? NO : YES;
}


#endif
