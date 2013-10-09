/*
 *  LXCList.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 17.3.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXCList.h"
#include "LXRef_Impl.h"


typedef struct LXCList {
    size_t count;
    size_t capacity;
    void **array;
} LXCList;


LXCListPtr LXCListCreateWithCapacity(size_t capacity)
{
    LXCList *list = _lx_malloc(sizeof(LXCList));
    list->count = 0;
    list->capacity = capacity;
    list->array = _lx_malloc(list->capacity * sizeof(void *));
    
    return list;
}

LXCListPtr LXCListCreate()
{
    return LXCListCreateWithCapacity(16);  // default
}

void LXCListDestroy(LXCListPtr list)
{
    LXInteger i;
    if ( !list) return;
    
    for (i = 0; i < list->count; i++) {
        LXRefRelease(list->array[i]);
    }
    _lx_free(list->array);
    _lx_free(list);
}



LXUInteger LXCListCount(LXCListPtr list)
{
    if ( !list) return 0;
    return list->count;
}

LXRef LXCListAt(LXCListPtr list, LXUInteger index)
{
    if ( !list) return NULL;
    if (index >= list->count) return NULL;
    return list->array[index];
}

LXBool LXCListContains(LXCListPtr list, LXRef ref)
{
    LXInteger i;
    if ( !list || !ref) return NO;
    
    for (i = 0; i < list->count; i++) {
        if (list->array[i] == ref)
            return YES;
    }
    return NO;
}


LXSuccess _LXCListGrowCapacityTo(LXCListPtr list, size_t newCapacity)
{
    if (newCapacity > list->capacity) {
        LXInteger i;
        size_t oldCapacity = list->capacity;
        void *newArray;
        
        list->capacity = newCapacity;
    
        newArray = realloc(list->array, newCapacity * sizeof(void *));
        if ( !newArray)
            return NO;  // realloc failed for whatever reason
        
        list->array = newArray;
        
        for (i = oldCapacity; i < newCapacity; i++) {
            list->array[i] = NULL;
        }
    }
    return YES;
}

void LXCListPut(LXCListPtr list, LXRef ref)
{
    if ( !list || !ref) return;
    
    if (_LXCListGrowCapacityTo(list, list->count + 1)) {
        list->array[list->count] = ref;
        list->count++;
    }
}

void _LXCListMoveContentsFromTo(LXCListPtr list, LXUInteger fromIndex, LXUInteger toIndex)
{
    // move array contents through a temp buffer
    LXInteger amountToMove = list->count - fromIndex;
    if (amountToMove <= 32) {
        void *tempBuf[32];
        
        memcpy(tempBuf,  list->array + fromIndex,  amountToMove * sizeof(void *));
        memcpy(list->array + toIndex,  tempBuf,  amountToMove * sizeof(void *));
    }
    else {
        void *tempBuf = _lx_malloc(amountToMove * sizeof(void *));

        memcpy(tempBuf,  list->array + fromIndex,  amountToMove * sizeof(void *));
        memcpy(list->array + toIndex,  tempBuf,  amountToMove * sizeof(void *));
    }
}


void LXCListPutAt(LXCListPtr list, LXRef ref, LXUInteger index)
{
    if ( !list || !ref) return;
    
    if (index >= list->count) {
        LXCListPut(list, ref);
        return;
    }
    
    if (_LXCListGrowCapacityTo(list, list->count + 1)) {
        _LXCListMoveContentsFromTo(list, index, index + 1);
    
        list->array[index] = ref;
        list->count++;
    }
}

void LXCListRemoveAt(LXCListPtr list, LXUInteger index)
{
    if ( !list) return;
    if (index >= list->count) return;
    
    LXRefRelease(list->array[index]);
    
    _LXCListMoveContentsFromTo(list, index + 1, index);
    list->count--;
}


LXInteger LXCListIndexOf(LXCListPtr list, LXRef wantedObj)
{
    LXInteger i;
    if ( !list) return kLXNotFound;
    if ( !wantedObj) return kLXNotFound;
    
    for (i = 0; i < list->count; i++) {
        if (list->array[i] == wantedObj)
            return i;
    }
    
    return kLXNotFound;    
}

LXInteger LXCListIndexOfElementNamed(LXCListPtr list, const char *wantedName)
{
    LXInteger i;
    if ( !list) return kLXNotFound;
    if ( !wantedName) return kLXNotFound;
    
    for (i = 0; i < list->count; i++) {
        LXRefAbstract *ref = (LXRefAbstract *)(list->array[i]);
        //printf(" -- lxclist elem %i: %p (%s, %p)\n", i, ref, ref->typeID, ref->copyNameFunc);
        
        if (ref && ref->copyNameFunc) {
            char *name = ref->copyNameFunc((LXRef)ref);
            LXBool found = (0 == strcmp(wantedName, name));
            
            _lx_free(name);
            if (found) {
                return i;
            }
        }
    }
    
    return kLXNotFound;
}



#pragma mark --- list combiners and other useful utilities ---

void LXCListRetainAll(LXCListPtr list)
{
    if ( !list) return;
    
    LXInteger i;
    for (i = 0; i < list->count; i++) {
        LXRefRetain(list->array[i]);
    }    
}

void LXCListPerform(LXCListPtr list, LXRefGenericVoidFuncPtr actionFunc)
{
    if ( !list) return;
    
    LXInteger i;
    for (i = 0; i < list->count; i++) {
        LXRef ref = list->array[i];
        actionFunc(ref);
    }
}


LXFloat LXCFloatAdd(LXFloat v1, LXFloat v2)
{
    return v1 + v2;
}

LXFloat LXCFloatMax(LXFloat v1, LXFloat v2)
{
    return (v1 > v2) ? v1 : v2;
}

LXFloat LXCListCombineByFloatFunc(LXCListPtr list, LXRefFloatGetterFuncPtr accessorFunc, LXFloatCombinerFuncPtr combinerFunc)
{
    if ( !list || !accessorFunc || !combinerFunc) return 0.0;
    
    LXFloat sum = 0.0;
    LXInteger i;
    for (i = 0; i < list->count; i++) {
        LXRef ref = list->array[i];
        
        //printf("   ..comb %i: %f -> %f\n", i, accessorFunc(ref), sum);
        
        sum = combinerFunc(sum, accessorFunc(ref));
    }
    
    return sum;
}



LXCListPtr LXCListSelectByFlagsFunc(LXCListPtr list, LXRefIntegerGetterFuncPtr accessorFunc, LXUInteger flagCriterion)
{
    if ( !list || !accessorFunc) return NULL;
    
    LXCListPtr newList = LXCListCreate();
    
    LXInteger i;
    for (i = 0; i < list->count; i++) {
        LXRef ref = list->array[i];
        
        LXUInteger flags = (LXUInteger) accessorFunc(ref);
        
        if (flags & flagCriterion)
            LXCListPut(newList, ref);
    }
    
    return newList;
}


LXCListPtr LXCListSelectByBoolFunc(LXCListPtr list, LXRefBoolGetterFuncPtr accessorFunc)
{
    if ( !list || !accessorFunc) return NULL;
    
    LXCListPtr newList = LXCListCreate();
    
    LXInteger i;
    for (i = 0; i < list->count; i++) {
        LXRef ref = list->array[i];
        
        if (accessorFunc(ref))
            LXCListPut(newList, ref);
    }
    
    return newList;
}


LXCListPtr LXCListSelectInverseOfList(LXCListPtr list, LXCListPtr exclusionList)
{
    if ( !list) return NULL;
    
    LXCListPtr newList = LXCListCreate();
    
    LXInteger i;
    for (i = 0; i < list->count; i++) {
        LXRef ref = list->array[i];
        
        if ( !LXCListContains(exclusionList, ref))
            LXCListPut(newList, ref);
    }
    
    return newList;
}


#pragma mark --- working with lists in LXRefs ---


LXRef LXRefGetListElementByNameWithRecurseFunc(LXRef topObj, const char *wantedName, LXRefCListGetterFuncPtr getterFunc)
{
    if ( !topObj || !wantedName || !getterFunc) return NULL;
    
    LXCListPtr list = getterFunc(topObj);
    
    if ( !list) return NULL;
    
    LXInteger index = LXCListIndexOfElementNamed(list, wantedName);
    
    if (index != kLXNotFound) {
        return LXCListAt(list, index);
    }
    else {  // didn't find in top-level list, so look recursively inside the list
        LXInteger i;
        LXInteger count = LXCListCount(list);
        for (i = 0; i < count; i++) {
            LXRef obj = LXCListAt(list, i);
            LXRef found = LXRefGetListElementByNameWithRecurseFunc(obj, wantedName, getterFunc);
            if (found)
                return found;
        }
    }
    return NULL;
}


