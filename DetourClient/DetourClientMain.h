#pragma once
#include <string>
#include <stack>
#include <vector>
#include <functional>
#include <algorithm>
using namespace std;

typedef enum {
    StackTypeHeapAlloc = 1,
    StackTypeRpc = 2,
} StackType;

struct HeapSizeData
{
    HeapSizeData(int nSize, int nThresh)
    {
        _nSize = nSize;
        _nThreshold = nThresh;
        _nStacksCollected = 0;
    }
    LONG _nSize; // the size of the allocation
    LONG _nThreshold; // if this is 0, then we collect stacks. Starts at e.g. 1000. Will count down. 
    LONG _nStacksCollected; // # of stacks collected (total stacks, not unique stacks)
};

//  Contains HeapSizeData for exact match
//once initialized, this vector is const except for the counts, which are interlocked when changed. Thus it's thread safe
extern vector<HeapSizeData> g_heapAllocSizes;

struct StlAllocStats
{
    LONG g_MyStlAllocTotalAlloc = 0;
    LONG g_MyStlAllocLimit = 65536*1;
    LONG g_NumUniqueStacks = 0;
    bool g_fReachedMemLimit = false;
    long g_nTotMyStlAllocBytes = 0;
    long g_nTotMyStlFreedBytes = 0;
    long g_nTotFramesCollected = 0;
};
extern StlAllocStats g_MyStlAllocStats;


extern DWORD g_dwMainThread;
extern int g_nTotalAllocs;
extern LONGLONG g_TotalAllocSize;

extern WCHAR * g_strHeapAllocSizesToCollect;
extern WCHAR * g_strHeapAllocThresholds;
extern int g_NumFramesTocapture;
extern SIZE_T g_HeapAllocSizeMinValue;
extern HANDLE g_hHeap;


bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraData, int numFramesToSkip);
bool UnCollectStack(StackType stackType, DWORD stackParam);

void DoSomeManagedCode();
void DoSomeThreadingModelExperiments();

LONGLONG GetNumStacksCollected();

