/*
 *  LXFPClosureContext.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 23.9.2008.
 *  Copyright 2008 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXFPClosureContext.h"


typedef struct _LXFPClosureContext {

    LXFloat *scalars;
    LXFloat *vectors;
    
    LXInteger scalarCapacity;
    LXInteger vectorCapacity;
    
    LXInteger scalarsInUse;
    LXInteger vectorsInUse;

} FPClosureContext;


LXFPClosureContextPtr LXFPClosureContextCreate()
{
    FPClosureContext *ctx = _lx_calloc(1, sizeof(FPClosureContext));
    
    if (ctx) {
        ctx->scalarCapacity = 64;
        ctx->scalars = _lx_malloc(ctx->scalarCapacity * sizeof(LXFloat));
    
        ctx->vectorCapacity = 32;
        ctx->vectors = _lx_malloc(ctx->vectorCapacity * 4 * sizeof(LXFloat));
    }
    return ctx;
}

void LXFPClosureContextDestroy(LXFPClosureContextPtr ctx)
{
    if ( !ctx) return;
        
    if (ctx->scalars) _lx_free(ctx->scalars);
    if (ctx->vectors) _lx_free(ctx->vectors);
    
    _lx_free(ctx);
}

LXFPClosureContextPtr LXFPClosureContextCopy(LXFPClosureContextPtr ctx)
{
    if ( !ctx) return NULL;
    
    LXFPClosureContextPtr newCtx = LXFPClosureContextCreate();
    if (newCtx) {
        memcpy(newCtx->scalars, ctx->scalars,  ctx->scalarCapacity * sizeof(LXFloat));
        
        memcpy(newCtx->vectors, ctx->vectors,  ctx->vectorCapacity * 4 * sizeof(LXFloat));
        
        newCtx->scalarsInUse = ctx->scalarsInUse;
        newCtx->vectorsInUse = ctx->vectorsInUse;
    }
    return newCtx;
}

void LXFPClosureContextSetScalars(LXFPClosureContextPtr ctx, LXFloat *values, LXInteger scalarCount)
{
    if ( !ctx || !values || scalarCount < 1) return;
    
    LXInteger realCount = MIN(scalarCount, ctx->scalarCapacity);
    
    memcpy(ctx->scalars, values, realCount * sizeof(LXFloat));
    
    ctx->scalarsInUse = realCount;
}

void LXFPClosureContextSetVectors(LXFPClosureContextPtr ctx, LXRGBA *values, LXInteger vectorCount)
{
    if ( !ctx || !values || vectorCount < 1) return;
    
    const LXInteger realCount = MIN(vectorCount, ctx->vectorCapacity);
    LXInteger i;
    for (i = 0; i < realCount; i++) {
        ctx->vectors[i*4 + 0] = values[i].r;
        ctx->vectors[i*4 + 1] = values[i].g;
        ctx->vectors[i*4 + 2] = values[i].b;
        ctx->vectors[i*4 + 3] = values[i].a;
    }
    
    ctx->vectorsInUse = realCount;
}

LXInteger LXFPClosureContextGetScalarCount(LXFPClosureContextPtr ctx)
{
    if ( !ctx) return 0;
    
    return ctx->scalarsInUse;
}

LXInteger LXFPClosureContextGetVectorCount(LXFPClosureContextPtr ctx)
{
    if ( !ctx) return 0;
    
    return ctx->vectorsInUse;
}

LXInteger LXFPClosureContextGetScalarCapacity(LXFPClosureContextPtr ctx)
{
    if ( !ctx) return 0;
    
    return ctx->scalarCapacity;
}

LXInteger LXFPClosureContextGetVectorCapacity(LXFPClosureContextPtr ctx)
{
    if ( !ctx) return 0;
    
    return ctx->vectorCapacity;
}


LXFloat LXFPClosureContextGetScalarAtIndex(LXFPClosureContextPtr ctx, LXInteger index)
{
    if ( !ctx || index >= ctx->scalarCapacity) return (LXFloat)0.0;
    
    return ctx->scalars[index];
}

LXRGBA LXFPClosureContextGetVectorAtIndex(LXFPClosureContextPtr ctx, LXInteger index)
{
    if ( !ctx || index >= ctx->vectorCapacity) return LXZeroRGBA;
    
    LXFloat *vec = ctx->vectors + index*4;
    
    return LXMakeRGBA(vec[0], vec[1], vec[2], vec[3]);
}

