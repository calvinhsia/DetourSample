// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "..\DetourSharedBase\DetourShared.h"

class temp
{
public:
    int mem;
};
CLINKAGE HRESULT EXPORT StartDetouring(PVOID *pDetours)
{
    auto xx = new temp();
    xx->mem = 1;
    *pDetours = xx;
    return S_OK;
}
#pragma comment(linker, "/EXPORT:StartDetouring=_StartDetouring@4")

CLINKAGE HRESULT EXPORT StopDetouring(PVOID oDetours)
{
    delete oDetours;
    return S_OK;
}
#pragma comment(linker, "/EXPORT:StopDetouring=_StopDetouring@4")



BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

