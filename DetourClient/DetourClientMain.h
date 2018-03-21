#pragma once
#include "vector"
using namespace std;

typedef enum {
    StackTypeHeapAlloc = 1,
    StackTypeRpc = 2,
} StackType;


extern DWORD g_dwMainThread;
extern int g_nTotalAllocs;
extern LONGLONG g_TotalAllocSize;
extern LONG g_MyStlAllocTotalAlloc;
extern vector<int> g_heapAllocSizesToCollect;
extern vector<int> g_heapAllocSizeThreshholds;
bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraData, int numFramesToSkip);
bool UnCollectStack(StackType stackType, DWORD stackParam);

void DoSomeManagedCode();
void DoSomeThreadingModelExperiments();

LONGLONG GetNumStacksCollected();

