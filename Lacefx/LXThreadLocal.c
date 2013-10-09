/*
 *  LXThreadLocal.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 29.6.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */

#include "LXThreadLocal.h"


#if defined(__APPLE__) || defined(_HAVE_PTHREAD_H)

LXThreadLocalVar LXThreadLocalCreate()
{
    pthread_key_t tls = 0;
    
    pthread_key_create(&tls, NULL);
    
    return tls;
}

void LXThreadLocalDestroy(LXThreadLocalVar tls)
{
    pthread_key_delete(tls);
}

#endif

