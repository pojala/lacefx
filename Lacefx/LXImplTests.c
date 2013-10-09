/*
 *  LXImplTests.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "Lacefx.h"
#include <math.h>


#if 0

#include "LXShape.h"


#ifdef __APPLE__
// implemented in LXImplTests_Cocoa.m
extern void RunLXViewCocoaTest(LXViewRef topView);
#endif


static LXShapeRef createTestShape_circle()
{
    LXShapeRef shape = LXShapeCreateMutable();
    LXShapeSetName(shape, "testCircle");
    
    LXShapeAddPoint2f(shape, -1,  0);
    LXShapeAddPoint2f(shape,  0,  1);
    LXShapeAddPoint2f(shape,  1,  0);
    LXShapeAddPoint2f(shape,  0, -1);

    
    LXShapeSetTypeForPointAtIndex(shape, kLXPointBezierInOut, 0);
    LXShapeSetTypeForPointAtIndex(shape, kLXPointBezierInOut, 1);
    LXShapeSetTypeForPointAtIndex(shape, kLXPointBezierInOut, 2);
    LXShapeSetTypeForPointAtIndex(shape, kLXPointBezierInOut, 3);
    
    const LXFloat kappaX = 0.5522847498;
    const LXFloat kappaY = kappaX;
    
    const LXPoint cp_1[2] = { { -1, -kappaY },  { -1, kappaY  } };
    const LXPoint cp_2[2] = { { -kappaX, 1  },  { kappaX, 1   } };
    const LXPoint cp_3[2] = { { 1,  kappaY  },  { 1, -kappaY  } };
    const LXPoint cp_4[2] = { {  kappaX, -1 },  { -kappaX, -1 } };
    
    LXShapeSetAuxDataForPointAtIndex(shape, cp_1, 0);
    LXShapeSetAuxDataForPointAtIndex(shape, cp_2, 1);
    LXShapeSetAuxDataForPointAtIndex(shape, cp_3, 2);
    LXShapeSetAuxDataForPointAtIndex(shape, cp_4, 3);
    
    LXShapeSetClosed(shape, YES);
    
    return shape;
}
#endif

static LXShaderRef createTestPxProg_twoPointGradient()
{
    static const char pxProgStr[] =
        "!!LCQfp1.0"
        "MOV v2, { 0.3, 0.3, 0.3, 1.0};"
        "MOV v3, { 0.905, 0.9, 0.91, 1.0};"
        "SUB r1, v3, v2;"
        "MAD r0, s1, r1, v2;"  // r0 = S1 * (v3 - v2) + v2
        "END";

    LXError err;
    LXShaderRef pxProg = LXShaderCreateWithString(pxProgStr, strlen(pxProgStr),
                                                  kLXShaderFormat_FPClosure,
                                                  kLXDefaultStorage, &err);
    return pxProg;
}


void LXImplRunTests()
{
    LXSuccess ok;
    LXError err;

    LXDEBUGLOG("----- starting Lacefx tests -----");

	/* --- autorelease pool --- */
    LXPoolRef pool = LXPoolCreateForThread();
    LXPoolEnableDebug(pool);
    
    LXPoolRef obj1 = LXPoolCreate();
    LXPoolRef obj2 = LXPoolCreate();    
    LXAutorelease(obj1);
    LXAutorelease(obj2);
    
    LXPoolRef pool2 = LXPoolCreateForThread();  // test nesting
    LXPoolEnableDebug(pool2);

    LXTransform3DRef obj3 = LXTransform3DCreateIdentity();    
    LXAutorelease(obj3);
    
    char *cString = malloc(512);
    LXAutoreleaseCPtr(cString);
    
    LXPoolRelease(pool2);  // test pool release
    LXPoolRelease(pool);
    
    
    // new pool for rest of our autoreleased objects
    pool = LXPoolCreateForThread();
    LXPoolEnableDebug(pool);
    

    /* --- map --- */    
   {
    LXMapPtr map = LXMapCreateMutable();
    LXBool origB = YES;
    double origD = 3.75343;
    int32_t origI = 12312312;
    int64_t origI64 = 123123LL * 12312LL;
    char *origStr = "this string contains öäå...";
	LXMapSetBool(map, "eka", origB);
	LXMapSetDouble(map, "toka", origD);
	LXMapSetInt32(map, "kolmas", origI);
    LXMapSetInt64(map, "isoluku_ja_pitka_kopioitu_key", origI64);
    LXMapSetUTF8(map, "scandic utf8", origStr);
    LXMapSetBinaryData(map, "test binary", (uint8_t *)origStr, strlen(origStr) + 1, kLXUnknownEndian);
	
	LXBool b = 0;
	double d = 0;
	int32_t i = 0;
    int64_t i64 = 0;
    char *str = NULL;
    uint8_t *data = NULL;
    size_t dataLen = 0;
	LXMapGetBool(map, "eka", &b);
	LXMapGetDouble(map, "toka", &d);
	LXMapGetInt32(map, _lx_strdup("kolmas"), &i);  // using strdup here just to ensure that keys don't need to be pointer-equivalent
    LXMapGetInt64(map, strdup("isoluku_ja_pitka_kopioitu_key"), &i64);
	LXMapGetUTF8(map, "scandic utf8", &str);
    LXMapGetBinaryData(map, "test binary", &data, &dataLen, NULL);
    
	if (origB != b) printf("*** map getBool fails\n");
	if (origD != d) printf("*** map getDouble fails\n");
	if (origI != i) printf("*** map getInt32 fails\n");
    if (origI64 != i64) printf("*** map getInt64 fails (has key: %i)\n", LXMapContainsValueForKey(map, "isoluku_ja_pitka_kopioitu_key", NULL));
    if (0 != strcmp(origStr, str)) printf("*** map getUTF8 fails (result: %s)\n", str);
    if (dataLen != strlen(origStr)+1) printf("*** map getBinaryData fails, incorrect length (%i)\n", (int)dataLen);
    if (0 != strcmp(origStr, (char *)data)) printf("*** map getBinaryData fails, data is incorrect\n");
       
    _lx_free(str);
	
	LXMapDestroy(map);
   }
        
    /* --- vector transforms --- */
   {
    LXTransform3DRef t = LXTransform3DCreateIdentity();
    LXTransform3DTranslate(t, 3, 2, 1);
    
    {
    LXFloat vec[4] = { 10, 10, 10, 1 };
    LXTransform3DTransformVector4Ptr(t, vec);
    LXDEBUGLOG("transformed vec: %f, %f, %f, %f (exp 13, 12, 11)", vec[0], vec[1], vec[2], vec[3]);
    }
    
    LXTransform3DSetIdentity(t);
    LXTransform3DScale(t, 2, 3, 4);
    
    {
    LXFloat vec[4] = { 10, 10, 10, 1 };
    LXTransform3DTransformVector4Ptr(t, vec);
    LXDEBUGLOG("transformed vec: %f, %f, %f, %f (exp 20, 30, 40)", vec[0], vec[1], vec[2], vec[3]);
    }

    LXTransform3DTranslate(t, 6, 7, 8);
    
    {
    LXFloat vec[4] = { 10, 10, 10, 1 };
    LXTransform3DTransformVector4Ptr(t, vec);
    LXDEBUGLOG("transformed vec: %f, %f, %f, %f (exp 26, 37, 48)", vec[0], vec[1], vec[2], vec[3]);
    }
    
    LXTransform3DRotate(t, M_PI/2, 1, 0, 0);
    
    {
    LXFloat vec[4] = { 10, 10, 10, 1 };
    LXTransform3DTransformVector4Ptr(t, vec);
    LXDEBUGLOG("transformed vec: %f, %f, %f, %f (exp 26, 48, 37)", vec[0], vec[1], vec[2], vec[3]);
    }

    LXTransform3DRotate(t, 0.2, 0, 0, 1);
    
    {
    LXFloat vec[4] = { 10, 10, 10, 1 };
    LXTransform3DTransformVector4Ptr(t, vec);
    LXDEBUGLOG("transformed vec: %f, %f, %f, %f (exp 35.017859, -41.87779, 37)", vec[0], vec[1], vec[2], vec[3]);
    }
   }
   
   /* --- pixelprogram evaluation test --- */
   {
    static const char pxProgStr[] =
        "!!LCQfp1.0\n"
        "ADD r0, v1, { 0.3, 0.8, 0.2, 0.1 };\n"
        "END";

    LXShaderRef pxProg = LXShaderCreateWithString(pxProgStr, strlen(pxProgStr), kLXShaderFormat_FPClosure,
                                                              kLXDefaultStorage, &err);
    if ( !pxProg)
        printf("** couldn't create pixel program (%i)\n", err.errorID);
     
    const int arrCount = 4;
    float inArr[arrCount * 4];
    float outArr[arrCount * 4];
    memset(outArr, 0, arrCount * 4 * sizeof(float));
    
    int i;
    for (i = 0; i < arrCount; i++) {
        inArr[i*4+0] = (float)i / arrCount;
        inArr[i*4+1] = (float)i * 10;
        inArr[i*4+2] = 0.0;
        inArr[i*4+3] = 1.0;
    }
        
    ok = LXShaderEvaluate1DArray4f(pxProg, arrCount, inArr, outArr);
    if ( !ok) {
        printf("** pixel program evaluation failed\n");
    } else {
        LXDEBUGLOG("got values: firstvec %.3f / %.3f (expected 0.3 / 0.8)\n  .. middle %.3f / %.3f (expected 0.8 / 20.8)",
                       outArr[0], outArr[1], //outArr[2], outArr[3],
                       outArr[arrCount/2*4], outArr[arrCount/2*4+1]  //, outArr[arrCount/2*4+2], outArr[arrCount/2*4+3]);
                       );
    }
   }
   

#if 0   
   /* --- list and shape test --- */
   {
    LXCListPtr list = LXCListCreate();
    LXShapeRef shape;
    
    shape = LXShapeCreateMutable();
    LXShapeSetName(shape, "a1");
    LXCListPut(list, shape);   // note: LXCList does not retain objects, but does release them when destroyed!
    
    shape = LXShapeCreateMutable();
    LXShapeSetName(shape, "a2");
    LXCListPut(list, shape);
    
    shape = LXShapeCreateMutable();
    LXShapeSetName(shape, "a3");
    LXCListPut(list, shape);
    
    shape = LXShapeCreateMutable();
    LXShapeSetName(shape, "z1");
    LXCListPutAt(list, shape, 2);
    
    if (LXCListCount(list) != 4)
        printf("invalid list count: %i\n", LXCListCount(list));
        
    if (LXCListIndexOfElementNamed(list, "a3") != 3)
        printf("can't find wanted list element\n");
    
    LXCListDestroy(list);
   }
#endif

   
   LXDEBUGLOG("----- Lacefx tests done -----");
}

