#pragma once
#include <string>
#include <stack>
#include <vector>
#include "unordered_map"
#include "memory"
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
extern DWORD g_dwMainThread;

extern WCHAR * g_strHeapAllocSizesToCollect;
extern WCHAR * g_strHeapAllocThresholds;
extern int g_NumFramesTocapture;
extern SIZE_T g_HeapAllocSizeMinValue;
extern HANDLE g_hHeap;

struct StlAllocStats
{
    LONG _MyStlAllocCurrentTotalAlloc = 0;
    LONG _MyStlAllocLimit = 65536 * 1;
    LONG _NumUniqueStacks = 0;
    bool _fReachedMemLimit = false;
    long _MyStlAllocBytesEverAlloc = 0;
    long _MyStlTotBytesEverFreed = 0;
    long _nTotFramesCollected = 0;
    int _nTotNumHeapAllocs;// total # of allocations by AllocHeap
    LONGLONG _TotNumBytesHeapAlloc; // total # bytes alloc'd by AllocHeap
};
extern StlAllocStats g_MyStlAllocStats;




template <class T>
struct MySTLAlloc // https://blogs.msdn.microsoft.com/calvin_hsia/2010/03/16/use-a-custom-allocator-for-your-stl-container/
{
    typedef T value_type;
    MySTLAlloc()
    {
    }
    // A converting copy constructor:
    template<class U> MySTLAlloc(const MySTLAlloc<U>& other)
    {
    }
    template<class U> bool operator==(const MySTLAlloc<U>&) const
    {
        return true;
    }
    template<class U> bool operator!=(const MySTLAlloc<U>&) const
    {
        return false;
    }
    T* allocate(const size_t n) const
    {
        if (n == 0)
        {
            return nullptr;
        }
        if (n > static_cast<size_t>(-1) / sizeof(T))
        {
            throw std::bad_array_new_length();
        }
        unsigned nSize = (UINT)n * sizeof(T);
        InterlockedAdd(&g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc, nSize);
        g_MyStlAllocStats._MyStlAllocBytesEverAlloc += nSize;
        void *pv;
        pv = HeapAlloc(g_hHeap, 0, nSize);
        if (pv == 0)
        {
            throw std::bad_alloc();
            //			_ASSERT_EXPR(false, L"MyStlAlloc failed to allocate:");// out of memmory allocating %d(%x).\n Try reducing stack size limit.For 32 bit proc, try http://blogs.msdn.com/b/calvin_hsia/archive/2010/09/27/10068359.aspx ", nSize, nSize));
        }
        return static_cast<T*>(pv);
    }
    void deallocate(T* const p, size_t n) const
    {
        unsigned nSize = (UINT)n * sizeof(T);
        InterlockedAdd(&g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc, -((int)nSize));
        g_MyStlAllocStats._MyStlTotBytesEverFreed += nSize;
        // upon ininitialize, g_hHeap is null, ebcause the heap has already been deleted, deleting all our objects
        if (g_hHeap != nullptr)
        {
            HeapFree(g_hHeap, 0, p);
        }
    }
    ~MySTLAlloc() {

    }
};

template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// need a hash function for pair<int,int>
template<typename S, typename T> struct hash<pair<S, T>>
{
    inline size_t operator()(const pair<S, T> & v) const
    {
        size_t seed = 0;
        ::hash_combine(seed, v.first);
        ::hash_combine(seed, v.second);
        return seed;
    }
};


typedef vector<PVOID
    , MySTLAlloc<PVOID>
> vecFrames;

// Collects the callstack and calculates the stack hash
// represents a single call stack and how often the identical stack occurs
struct CallStack
{
    CallStack(int NumFramesToSkip) : cnt(1)
    {
        vecFrames.resize(g_NumFramesTocapture);
        int nFrames = RtlCaptureStackBackTrace(
            /*FramesToSkip*/ NumFramesToSkip,
            /*FramesToCapture*/ g_NumFramesTocapture,
            &vecFrames[0],
            &stackHash
        );
        vecFrames.resize(nFrames);
    }
    ULONG stackHash; // hash of stack 4 bytes in both x86 and amd64
    int cnt;   // # of occurrences of this particular stack
    vecFrames vecFrames; // the stack frames
};

typedef unordered_map<UINT, CallStack // can't use unique_ptr because can't override it's allocator and thus can cause deadlock
    ,
    hash<UINT>,
    equal_to<UINT>,
    MySTLAlloc<pair<const UINT, CallStack> >
> mapStackHashToStack; // stackhash=>CallStack

                       // represents the stacks for a particular stack type : e.g. the 100k allocations
                       // if the stacks are identical, the count is bumped.
struct StacksForStackType
{
    StacksForStackType(int numFramesToSkip)
    {
        AddNewStack(numFramesToSkip);
    }
    bool AddNewStack(int numFramesToSkip)
    {
        bool fDidAdd = false;
        if (!g_MyStlAllocStats._fReachedMemLimit)
        {
            CallStack stack(numFramesToSkip);
            auto hash = stack.stackHash;
            auto res = _stacks.find(hash);
            if (res == _stacks.end())
            {
                if (!g_MyStlAllocStats._fReachedMemLimit)
                {
                    g_MyStlAllocStats._NumUniqueStacks++;
                    g_MyStlAllocStats._nTotFramesCollected += stack.vecFrames.size();
                    _stacks.insert(mapStackHashToStack::value_type(hash, move(stack)));
                    fDidAdd = true;
                }
            }
            else
            {
                res->second.cnt++;
                fDidAdd = true;
            }
        }
        return fDidAdd;
    }

    LONGLONG GetTotalNumStacks()
    {
        auto tot = 0l;
        for (auto &stack : _stacks)
        {
            tot += stack.second.cnt;
        }
        return tot;
    }
    // map of stack hash to CalLStack
    mapStackHashToStack _stacks;

};

typedef pair<StackType, UINT> mapKey;

typedef unordered_map<mapKey, StacksForStackType
    ,
    hash<mapKey>,
    equal_to<mapKey>,
    MySTLAlloc<pair<const mapKey, StacksForStackType>>
> mapStacksByStackType;

// map the Size of an alloc to all the stacks that allocated that size.
// note: if we're looking for all allocs of a specific size (e.g. 1Mb), then no need for a map by size (because all keys will be the same): more efficient to just use a mapStacks
extern mapStacksByStackType *g_pmapStacksByStackType;





void InitCollectStacks();
void UninitCollectStacks();


bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraData, int numFramesToSkip);
bool UnCollectStack(StackType stackType, DWORD stackParam);

void DoSomeManagedCode();
void DoSomeThreadingModelExperiments();

LONGLONG GetNumStacksCollected();

