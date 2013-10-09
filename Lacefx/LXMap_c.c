/*
 *  LXMap_c.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 10.2.2010.
 *  Copyright 2010 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXMap.h"
#include "LXBasicTypes.h"
#include "LXStringUtils.h"
#include "LXMutex.h"
#include "hashmap.h"
#include <math.h>


extern LXMutexPtr g_lxAtomicLock;



#pragma mark --- extremely basic integer map ---

LXIntegerMapPtr LXIntegerMapCreateMutable()
{
    return (LXIntegerMapPtr) hashmap_new();
}

void LXIntegerMapDestroy(LXIntegerMapPtr r)
{
    if ( !r) return;
    hashmap_free((map_t)r);
}

void LXIntegerMapInsert(LXMapPtr r, const char *key, LXInteger v)
{
    if ( !r || !key) return;
    LXMutexLock(g_lxAtomicLock);
    
    hashmap_put((map_t)r, key, (any_t)v);
    
    LXMutexUnlock(g_lxAtomicLock);
}

LXInteger LXIntegerMapGet(LXMapPtr r, const char *key)
{
    if ( !r || !key) return 0;
    any_t v = 0;
    LXMutexLock(g_lxAtomicLock);
    
    hashmap_get((map_t)r, key, &v);
    
    LXMutexUnlock(g_lxAtomicLock);
    return (LXInteger)v;
}

LXSuccess LXIntegerMapPop(LXMapPtr r, const char *key, LXInteger *outValue)
{
    if ( !r || !key) return NO;
    any_t v = 0;
    LXSuccess hadValue = NO;
    LXMutexLock(g_lxAtomicLock);
    
    if ((hadValue = (MAP_OK == hashmap_get((map_t)r, key, &v)))) {
        hashmap_remove((map_t)r, key);
    }
    
    LXMutexUnlock(g_lxAtomicLock);
    
    if (outValue) *outValue = (LXInteger)v;
    return hadValue;
}



typedef struct LXMapValue {
    LXUInteger type;
    char *key;          // hashmap doesn't copy keys, so we must copy the key string here and free it when value is deleted
    void *data;
} LXMapValue;

#define LXMapValueTypeGetType(t_)       (LXPropertyType) ((t_) & 0xff)
#define LXMapValueTypeGetFlags(t_)      (((t_) >> 8) & 0xff)
#define LXMapValueTypeSetFlags(t_, f_)  ((t_) | ((f_) << 8))

#define kLXMapValueIsInt64              1
#define kLXMapValueIsBigEndianData      (1<<1)
#define kLXMapValueIsLittleEndianData   (1<<2)


LXMapPtr LXMapCreateMutable()
{
    return (LXMapPtr) hashmap_new();
}

static int destroyMapValue(any_t userData, any_t value)
{
    LXMapValue *mapValue = (LXMapValue *)value;
    LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
    
    ///printf("%s: proptype %i, key %s\n", __func__, propType, mapValue->key);
    
    _lx_free(mapValue->key);  // was created with _lx_strdup()
    
    switch (propType) {
        case kLXBoolProperty:
            break;  // 'data' is the actual value

        case kLXIntegerProperty:
            if (LXMapValueTypeGetFlags(mapValue->type) & kLXMapValueIsInt64) {
                _lx_free(mapValue->data);
            }  // for non-int64 type, 'data' is the actual value
            break;
            
        default:
            _lx_free(mapValue->data);
            break;
            
        case kLXStringProperty:
            LXStrUnibufferDestroy((LXUnibuffer *)mapValue->data);
            _lx_free(mapValue->data);
            break;
            
        case kLXRefProperty:
            LXRefRelease((LXRef)mapValue->data);
            break;
            
        case kLXMapProperty:
            LXMapDestroy((LXMapPtr)mapValue->data);
            break;
    }
    
    _lx_free(mapValue);
    return MAP_OK;
}

void LXMapDestroy(LXMapPtr r)
{
    if ( !r) return;
    map_t map = (map_t)r;
    
    hashmap_iterate(map, destroyMapValue, NULL);
    hashmap_free(map);
}

void LXMapSetBool(LXMapPtr r, const char *key, LXBool v)
{
    if ( !r || !key) return;
    map_t map = (map_t)r;
    LXInteger integer = v;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXBoolProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = (void *)integer;
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetDouble(LXMapPtr r, const char *key, double v)
{
    if ( !r || !key) return;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXFloatProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = _lx_malloc(sizeof(double));
    
    memcpy(mapValue->data, &v, sizeof(double));
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetInteger(LXMapPtr r, const char *key, LXInteger v)
{
    if ( !r || !key) return;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXIntegerProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = (void *)v;
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetInt32(LXMapPtr r, const char *key, int32_t v)
{
    LXMapSetInteger(r, key, v);
}

void LXMapSetInt64(LXMapPtr r, const char *key, int64_t v)
{
    if ( !r || !key) return;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = LXMapValueTypeSetFlags(kLXIntegerProperty, kLXMapValueIsInt64);
    mapValue->key = _lx_strdup(key);
    mapValue->data = _lx_malloc(sizeof(int64_t));
    
    memcpy(mapValue->data, &v, sizeof(int64_t));
        
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetUTF16(LXMapPtr r, const char *key, LXUnibuffer uni)
{
    if ( !r || !key || !uni.unistr) return;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXStringProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = _lx_malloc(sizeof(LXUnibuffer));
    
    LXUnibuffer *copiedUni = (LXUnibuffer *)mapValue->data;
    *copiedUni = LXStrUnibufferCopy(&uni);
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetUTF8(LXMapPtr r, const char *key, const char *utf8Str)
{
    if ( !r || !key || !utf8Str) return;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXStringProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = _lx_malloc(sizeof(LXUnibuffer));

    LXUnibuffer *copiedUni = (LXUnibuffer *)mapValue->data;
    size_t utf16Len = 0;
    copiedUni->unistr = LXStrCreateUTF16_from_UTF8(utf8Str, strlen(utf8Str), &utf16Len);
    copiedUni->numOfChar16 = utf16Len;
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetBinaryData(LXMapPtr r, const char *key, const uint8_t *data, size_t dataLen, LXEndianness endianness)
{
    if ( !r || !key || !data || dataLen < 1) return;
    
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXBinaryDataProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = _lx_malloc(sizeof(uint32_t) + dataLen);
    
    uint32_t *plen = (uint32_t *)mapValue->data;
    uint8_t *pdata = (uint8_t *)mapValue->data + sizeof(uint32_t);
    
    *plen = dataLen;
    memcpy(pdata, data, dataLen);
    
    switch (endianness) {
        default: break;
        case kLXLittleEndian:
            mapValue->type = LXMapValueTypeSetFlags(kLXIntegerProperty, kLXMapValueIsLittleEndianData);
            break;
        case kLXBigEndian:
            mapValue->type = LXMapValueTypeSetFlags(kLXIntegerProperty, kLXMapValueIsBigEndianData);
            break;
    }
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapSetObjectRef(LXMapPtr r, const char *key, LXRef ref)
{
    if ( !r || !key || !ref) return;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXRefProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = LXRefRetain(ref);
    
    hashmap_put(map, mapValue->key, mapValue);
}

void LXMapCopyEntriesFromMap(LXMapPtr dstMap, LXMapPtr obj)
{
    if ( !dstMap || !obj) return;
    map_t sourceMap = (map_t)obj;
    
    int len = hashmap_length(sourceMap);
    if (len > 0) {
        const char **keys = _lx_malloc(len * sizeof(char *));
        hashmap_get_keys_array(sourceMap, keys, len);
        
        int i;
        for (i = 0; i < len; i++) {
            const char *key = keys[i];
            LXMapValue *srcVal = NULL;
            hashmap_get(sourceMap, key, (any_t *)&srcVal);
            if (srcVal) {
                LXPropertyType propType = LXMapValueTypeGetType(srcVal->type);
                switch (propType) {
                    case kLXBoolProperty: {
                        LXInteger integer = (LXInteger)(srcVal->data);
                        LXMapSetBool(dstMap, key, (integer) ? YES : NO);
                        break;
                    }
                    case kLXFloatProperty:
                        LXMapSetDouble(dstMap, key, *((double *)srcVal->data));
                        break;
                    case kLXIntegerProperty:
                        if (LXMapValueTypeGetFlags(srcVal->type) & kLXMapValueIsInt64) {
                            LXMapSetInt64(dstMap, key, *((int64_t *)srcVal->data));
                        } else {
                            LXMapSetInteger(dstMap, key, (LXInteger)(srcVal->data));
                        }
                        break;
                    case kLXStringProperty:
                        LXMapSetUTF16(dstMap, key, *((LXUnibuffer *)srcVal->data));
                        break;
                    case kLXRefProperty:
                        LXMapSetObjectRef(dstMap, key, (LXRef)srcVal->data);
                        break;
                    case kLXMapProperty:
                        LXMapSetMap(dstMap, key, (LXMapPtr)srcVal->data);
                        break;
                }
            }
        }
        
        _lx_free(keys);
    }
}

void LXMapSetMap(LXMapPtr r, const char *key, LXMapPtr obj)
{
    if ( !r || !key || !obj) return;
    map_t map = (map_t)r;

    LXMapValue *mapValue = _lx_malloc(sizeof(LXMapValue));
    mapValue->type = kLXMapProperty;
    mapValue->key = _lx_strdup(key);
    mapValue->data = LXMapCreateMutable();
    
    LXMapCopyEntriesFromMap((LXMapPtr)mapValue->data, obj);
    
    hashmap_put(map, mapValue->key, mapValue);
}


LXSuccess LXMapGetBool(LXMapPtr r, const char *key, LXBool *outValue)
{
    if ( !r || !key || !outValue) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXBoolProperty: {
                LXInteger integer = (LXInteger)(mapValue->data);
                *outValue = (integer) ? YES : NO;
                return YES;
            }
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetDouble(LXMapPtr r, const char *key, double *outValue)
{
    if ( !r || !key || !outValue) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXFloatProperty:
                *outValue = *((double *)mapValue->data);
                return YES;
            case kLXIntegerProperty:
                if (LXMapValueTypeGetFlags(mapValue->type) & kLXMapValueIsInt64) {
                    *outValue = *((int64_t *)mapValue->data);
                } else {
                    *outValue = (LXInteger)(mapValue->data);
                }
                return YES;
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetInteger(LXMapPtr r, const char *key, LXInteger *outValue)
{
    if ( !r || !key || !outValue) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXFloatProperty:
                *outValue = lround(*((double *)mapValue->data));
                return YES;
            case kLXIntegerProperty:
                if (LXMapValueTypeGetFlags(mapValue->type) & kLXMapValueIsInt64) {
                    *outValue = (LXInteger)(*((int64_t *)mapValue->data));
                } else {
                    *outValue = (LXInteger)(mapValue->data);
                }
                return YES;
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetInt32(LXMapPtr r, const char *key, int32_t *outValue)
{
    LXInteger integerValue = 0;
    if (LXMapGetInteger(r, key, &integerValue)) {
        *outValue = integerValue;
        return YES;
    } else {
        return NO;
    }
}

LXSuccess LXMapGetInt64(LXMapPtr r, const char *key, int64_t *outValue)
{
    if ( !r || !key || !outValue) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXFloatProperty:
                *outValue = lround(*((double *)mapValue->data));
                return YES;
            case kLXIntegerProperty:
                ///printf("%s: int, type value %i, flags %i\n", __func__, mapValue->type, LXMapValueTypeGetFlags(mapValue->type));
                if (LXMapValueTypeGetFlags(mapValue->type) & kLXMapValueIsInt64) {
                    *outValue = *((int64_t *)mapValue->data);
                } else {
                    *outValue = (LXInteger)(mapValue->data);
                }
                return YES;
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetUTF16(LXMapPtr r, const char *key, LXUnibuffer *outUni)
{
    if ( !r || !key || !outUni) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXStringProperty: {
                LXUnibuffer *srcUni = (LXUnibuffer *)mapValue->data;
                *outUni = LXStrUnibufferCopy(srcUni);
                return YES;
            }
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetUTF8(LXMapPtr r, const char *key, char **outUTF8Str)
{
    if ( !r || !key || !outUTF8Str) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXStringProperty: {
                LXUnibuffer *srcUni = (LXUnibuffer *)mapValue->data;
                *outUTF8Str = LXStrCreateUTF8_from_UTF16(srcUni->unistr, srcUni->numOfChar16, NULL);
                return YES;
            }
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetBinaryData(LXMapPtr r, const char *key, uint8_t **outData, size_t *outDataLen, LXEndianness *outEndianness)
{
    if ( !r || !key || !outData || !outDataLen) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXBinaryDataProperty: {
                LXEndianness endian = kLXUnknownEndian;
                if (LXMapValueTypeGetFlags(mapValue->type) & kLXMapValueIsLittleEndianData) {
                    endian = kLXLittleEndian;
                } else if (LXMapValueTypeGetFlags(mapValue->type) & kLXMapValueIsBigEndianData) {
                    endian = kLXBigEndian;
                }
                
                uint32_t *plen = (uint32_t *)mapValue->data;
                uint8_t *pdata = (uint8_t *)mapValue->data + sizeof(uint32_t);
                
                *outDataLen = *plen;
                *outData = _lx_malloc(*plen);
                memcpy(*outData, pdata, *plen);
                
                if (outEndianness) *outEndianness = endian;
                return YES;
            }
            default: 
                return NO;
        }
    }    
}

LXSuccess LXMapGetObjectRef(LXMapPtr r, const char *key, LXRef *outRef)
{
    if ( !r || !key || !outRef) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXRefProperty:
                *outRef = (LXRef)(mapValue->data);
                return YES;
            default: 
                return NO;
        }
    }
}

LXSuccess LXMapGetMap(LXMapPtr r, const char *key, LXMapPtr *outMap)
{
    if ( !r || !key || !outMap) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        LXPropertyType propType = LXMapValueTypeGetType(mapValue->type);
        switch (propType) {
            case kLXMapProperty:
                *outMap = (LXMapPtr)(mapValue->data);
                return YES;
            default: 
                return NO;
        }
    }    
}


LXSuccess LXMapRemoveValueForKey(LXMapPtr r, const char *key)
{
    if ( !r || !key) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        hashmap_remove(map, key);
        destroyMapValue(NULL, mapValue);
        return YES;
    }
}

LXBool LXMapContainsValueForKey(LXMapPtr r, const char *key, LXPropertyType *outPropType)
{
    if ( !r || !key) return NO;
    map_t map = (map_t)r;
    
    LXMapValue *mapValue = NULL;
    if (MAP_OK != hashmap_get(map, key, (any_t *)&mapValue) || !mapValue) {
        return NO;
    } else {
        if (outPropType) {
            *outPropType = LXMapValueTypeGetType(mapValue->type);
        }
        return YES;
    }
}


LXUInteger LXMapCount(LXMapPtr r)
{
    if ( !r) return 0;
    map_t map = (map_t)r;
    
    return hashmap_length(map);
}

LXBool LXMapGetKeysArray(LXMapPtr r, const char **keys, size_t arrayLen)
{
    if ( !r) return NO;
    map_t map = (map_t)r;
    
    hashmap_get_keys_array(map, keys, arrayLen);
    return YES;
}
