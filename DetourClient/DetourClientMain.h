#pragma once
typedef enum {
    StackTypeHeapAlloc = 1,
    StackTypeRpc = 2,
} StackType;


extern DWORD g_dwMainThread;
extern int g_nTotalAllocs;
extern LONGLONG g_TotalAllocSize;
extern LONG g_MyStlAllocTotalAlloc;

bool _stdcall CollectStack(StackType stackType, DWORD stackParam, DWORD extraData);
bool UnCollectStack(StackType stackType, DWORD stackParam);

void DoSomeManagedCode();
void DoSomeThreadingModelExperiments();

LONGLONG GetNumStacksCollected();

