/*
 *  LXMap.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 30.8.2007.
 *  Copyright 2007 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXMAP_H_
#define _LXMAP_H_

#include "LXBasicTypes.h"

typedef void *LXMapPtr;
typedef void *LXIntegerMapPtr;


enum {
    kLXIntegerProperty = 1,
    kLXBoolProperty,
    kLXFloatProperty,
    kLXStringProperty,
    kLXBinaryDataProperty,
    kLXMapProperty,             // LXMap object
    kLXRefProperty,             // LXRef object
    kLXPtrProperty,             // generic pointer
    kLXVector4Property,         // 4-element LXFloat vector (e.g. LXRect, LXRGBA or a vertex)
};
typedef LXUInteger LXPropertyType;



#ifdef __cplusplus
extern "C" {
#endif

#pragma mark --- LXMap public API ---

LXEXPORT LXMapPtr LXMapCreateMutable(void);
LXEXPORT void LXMapDestroy(LXMapPtr r);

LXEXPORT void LXMapSetBool(LXMapPtr map, const char *key, LXBool v);
LXEXPORT void LXMapSetDouble(LXMapPtr map, const char *key, double v);
LXEXPORT void LXMapSetInteger(LXMapPtr map, const char *key, LXInteger v);
LXEXPORT void LXMapSetInt32(LXMapPtr map, const char *key, int32_t v);
LXEXPORT void LXMapSetInt64(LXMapPtr map, const char *key, int64_t v);
LXEXPORT void LXMapSetUTF16(LXMapPtr map, const char *key, LXUnibuffer uni);
LXEXPORT void LXMapSetUTF8(LXMapPtr map, const char *key, const char *utf8Str);
LXEXPORT void LXMapSetBinaryData(LXMapPtr map, const char *key, const uint8_t *data, size_t dataLen, LXEndianness endianness);
LXEXPORT void LXMapSetObjectRef(LXMapPtr map, const char *key, LXRef ref);
LXEXPORT void LXMapSetMap(LXMapPtr map, const char *key, LXMapPtr obj);

LXEXPORT LXSuccess LXMapGetBool(LXMapPtr map, const char *key, LXBool *outValue);
LXEXPORT LXSuccess LXMapGetDouble(LXMapPtr map, const char *key, double *outValue);
LXEXPORT LXSuccess LXMapGetInteger(LXMapPtr map, const char *key, LXInteger *outValue);
LXEXPORT LXSuccess LXMapGetInt32(LXMapPtr map, const char *key, int32_t *outValue);
LXEXPORT LXSuccess LXMapGetInt64(LXMapPtr map, const char *key, int64_t *outValue);
LXEXPORT LXSuccess LXMapGetUTF16(LXMapPtr map, const char *key, LXUnibuffer *uni);
LXEXPORT LXSuccess LXMapGetUTF8(LXMapPtr map, const char *key, char **outUTF8Str);
LXEXPORT LXSuccess LXMapGetBinaryData(LXMapPtr map, const char *key, uint8_t **outData, size_t *outDataLen, LXEndianness *outEndianness);
LXEXPORT LXSuccess LXMapGetObjectRef(LXMapPtr map, const char *key, LXRef *outRef);
LXEXPORT LXSuccess LXMapGetMap(LXMapPtr map, const char *key, LXMapPtr *outMap);

LXEXPORT LXBool LXMapContainsValueForKey(LXMapPtr map, const char *key, LXPropertyType *outPropType);  // outPropType can be NULL if this information is not needed
LXEXPORT LXSuccess LXMapRemoveValueForKey(LXMapPtr map, const char *key);

LXEXPORT LXUInteger LXMapCount(LXMapPtr map);
LXEXPORT LXBool LXMapGetKeysArray(LXMapPtr map, const char **keys, size_t arrayLen);  // keys are owned by the map; must be copied if caller needs to retain them

LXEXPORT void LXMapCopyEntriesFromMap(LXMapPtr map, LXMapPtr otherMap);


// extremely basic map type that only supports string keys and integer values.
//
// * LXInteger is guaranteed to be pointer-sized, so this can be used for weak [non-retained] pointer values as well.
// 
// * the insert / get / pop operations are atomic, so an integer map can be shared between threads.
//
LXEXPORT LXIntegerMapPtr LXIntegerMapCreateMutable(void);
LXEXPORT void LXIntegerMapDestroy(LXIntegerMapPtr r);

LXEXPORT void LXIntegerMapInsert(LXMapPtr map, const char *key, LXInteger v);
LXEXPORT LXInteger LXIntegerMapGet(LXMapPtr map, const char *key);

LXEXPORT LXSuccess LXIntegerMapPop(LXMapPtr r, const char *key, LXInteger *outValue);  // removes the value and returns it in outValue (can be NULL if not interested in the value)

#ifdef __cplusplus
}
#endif

#endif
