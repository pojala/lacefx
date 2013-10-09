/*
 *  LXStdout_win.c
 *  Lacefx
 *
 *  Created by Pauli Ojala on 31.3.2009.
 *  Copyright 2009 Lacquer oy/ltd. 
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#include "LXStringUtils.h"
#import "LXMutex.h"


// --- Win32 thread-safe printf ---

// platform lock defined and initialized in LacqitInit.m
extern LXMutexPtr g_lxStdoutLock;


#define USE_WIN_CONSOLE 1


int LXPrintf(const char * LXRESTRICT formatStr, ...)
{
    va_list arguments;
    va_start(arguments, formatStr);

    char msgBuf[512];

    EnterCriticalSection(g_lxStdoutLock);

    vsnprintf(msgBuf, 512, formatStr, arguments);
    
#if (USE_WIN_CONSOLE)
    HANDLE hnd = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD ignore;
    
    WriteFile(hnd, msgBuf, strlen(msgBuf), &ignore, NULL);
#else
    //OSSpinLockLock(&g_winPrintfLock);
    
    printf(msgBuf);

    //OSSpinLockUnlock(&g_winPrintfLock);
#endif

    LeaveCriticalSection(g_lxStdoutLock);
    
    return 0;
}

