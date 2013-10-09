/*
 *  dllmain.m
 *  Lacefx
 *
 *  Created by Pauli Ojala on 28.7.2008.
 *  Copyright 2008 Lacquer oy/ltd.
 *
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 */


#import <windows.h>
#import "LXRandomGen.h"


__declspec(dllimport) int OBJCRegisterDLL(HINSTANCE handle);


int APIENTRY DllMain(HINSTANCE handle, DWORD reason, LPVOID _reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        LXRdSeed();
        
        return OBJCRegisterDLL(handle);
    }
    return TRUE;
}
