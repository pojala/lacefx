/*
 *  LXCList.h
 *  Lacefx
 *
 *  Created by Pauli Ojala on 17.3.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#ifndef _LXCLIST_H_
#define _LXCLIST_H_

#include "LXBasicTypes.h"
#include "LXRefTypes.h"


typedef struct LXCList *LXCListPtr;


// function pointer types
typedef LXCListPtr(*LXRefCListGetterFuncPtr)(LXRef ref);


// the LXCList type is a simple C array for LXRef types.
// it's mainly intended to be used by LX* API classes internally.

// IMPORTANT NOTE ABOUT MEMORY POLICY:
// LXCList doesn't retain added objects, but does release them.
// This makes it easier to add newly created objects directly into the array.


#ifdef __cplusplus
extern "C" {
#endif

LXEXPORT LXCListPtr LXCListCreate();
LXEXPORT LXCListPtr LXCListCreateWithCapacity(size_t capacity);
LXEXPORT void LXCListDestroy(LXCListPtr list);

LXEXPORT LXUInteger LXCListCount(LXCListPtr list);
LXEXPORT LXRef LXCListAt(LXCListPtr list, LXUInteger index);

LXEXPORT void LXCListPut(LXCListPtr list, LXRef ref);
LXEXPORT void LXCListPutAt(LXCListPtr list, LXRef ref, LXUInteger index);
LXEXPORT void LXCListRemoveAt(LXCListPtr list, LXUInteger index);

LXEXPORT LXInteger LXCListIndexOf(LXCListPtr list, LXRef obj);
LXEXPORT LXInteger LXCListIndexOfElementNamed(LXCListPtr list, const char *name);

LXEXPORT LXBool LXCListContains(LXCListPtr list, LXRef ref);

// operations on list contents
LXEXPORT void LXCListRetainAll(LXCListPtr list);

LXEXPORT void LXCListPerform(LXCListPtr list, LXRefGenericVoidFuncPtr actionFunc);

LXEXPORT LXFloat LXCListCombineByFloatFunc(LXCListPtr list, LXRefFloatGetterFuncPtr accessorFunc, LXFloatCombinerFuncPtr combinerFunc);

LXEXPORT LXCListPtr LXCListSelectByBoolFunc(LXCListPtr list, LXRefBoolGetterFuncPtr accessorFunc);
LXEXPORT LXCListPtr LXCListSelectByFlagsFunc(LXCListPtr list, LXRefIntegerGetterFuncPtr accessorFunc, LXUInteger flagCriterion);

LXEXPORT LXCListPtr LXCListSelectInverseOfList(LXCListPtr list, LXCListPtr exclusionList);


// -- combiner helper functions --
LXEXPORT LXFloat LXCFloatAdd(LXFloat v1, LXFloat v2);
LXEXPORT LXFloat LXCFloatMax(LXFloat v1, LXFloat v2);

// -- working with lists in LXRefs --

// this function is useful for implementing accessors like LXViewGetSubviewByName()
LXEXPORT LXRef LXRefGetListElementByNameWithRecurseFunc(LXRef topObj, const char *wantedName, LXRefCListGetterFuncPtr getterFunc);


#ifdef __cplusplus
}
#endif

#endif
