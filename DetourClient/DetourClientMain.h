#pragma once

// When creating an unordered_map a vector is created, calling "explicit vector(const _Alloc& _Al) _NOEXCEPT", but it allocates which can throw:
// https://developercommunity.visualstudio.com/content/problem/140200/c-stl-stdvector-constructor-declared-with-noexcept.html
#if _DEBUG
#define _ITERATOR_DEBUG_LEVEL 0
#endif _DEBUG
#include <string>
#include <stack>
#include <vector>
#include "unordered_map"
#include "memory"
#include <functional>
#include <algorithm>

// the different types of callstacks we collect
typedef enum {
	StackTypeHeapAlloc = 0,
	StackTypeRpc = 1,
	StackTypeMax = 2
} StackType;

// the different types of heap allocators we use
typedef enum {
	StlAllocUseProcessHeap, // limited by process memory space
	StlAllocUseCallStackHeap,  // limited to e.g. 64k. can throw when out of mem.
	StlAllocUseTlsHeap,		// only for TLS structures
	StlAllocMax
} StlAllocHeapToUse;

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
extern std::vector<HeapSizeData> g_heapAllocSizes;
extern DWORD g_dwMainThread;

extern pfnRtlAllocateHeap Real_RtlAllocateHeap;

extern WCHAR * g_strHeapAllocSizesToCollect;
extern int g_NumFramesTocapture;
extern SIZE_T g_HeapAllocSizeMinValue;



struct StlAllocStats
{
	long _MyStlAllocCurrentTotalAlloc[StlAllocMax];
	long _MyStlAllocBytesEverAlloc[StlAllocMax];
	long _MyStlTotBytesEverFreed[StlAllocMax];

	int _nTotNumHeapAllocs;// total # of allocations by AllocHeap for collected stacks
	LONGLONG _TotNumBytesHeapAlloc; // total # bytes alloc'd by AllocHeap for collected stacks

	long _MyStlAllocLimit = 65536 * 1;
	long _NumUniqueStacks = 0;
	long _NumStacksMissed[StackTypeMax]; // missed due to out of memory
	bool _fReachedMemLimit = false;
	long _nTotFramesCollected = 0;
};
extern StlAllocStats g_MyStlAllocStats;
struct HeapHolder
{
	// ensure that this _instance is statically initialized before the users of MyStlAlloc so the right heap is used.
	//    putting this _instance before the consumers of MyStlAlloc (in the same compilation unit) will do this
	static HeapHolder _instance;
	// create a heap that stores our private data: MyTlsData and Call stacks
	HANDLE g_hHeapDetourData;
	HeapHolder()
	{
		g_hHeapDetourData = HeapCreate(/*options*/0, /*dwInitialSize*/65536,/*dwMaxSize*/ 0);
	}
	~HeapHolder()
	{
		for (int i = 0; i < StackTypeHeapAlloc; i++)
		{
			_ASSERT_EXPR(g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[i] == 0, L"Heap leak");
		}
		HeapDestroy(g_hHeapDetourData);
	}
};


struct MyTlsData
{
	static int g_tlsIndex;
	static CComAutoCriticalSection g_tlsCritSect;
	static volatile bool g_IsCreatingTlsData;
	static long g_numTlsInstances;
	static bool DllMain(ULONG ulReason);
	static MyTlsData* GetTlsData();
#if _DEBUG
	static int _tlsSerialNo;
#endif _DEBUG
	MyTlsData(); //ctor
	~MyTlsData(); //dtor

#if _DEBUG
	DWORD _dwThreadId;
	int _nSerialNo;
#endif _DEBUG

	bool _fIsInRtlAllocHeap;
};



template <class T, StlAllocHeapToUse stlAllocHeapToUse>
struct MySTLAlloc // https://blogs.msdn.microsoft.com/calvin_hsia/2010/03/16/use-a-custom-allocator-for-your-stl-container/
{
	typedef T value_type;
	MySTLAlloc()
	{
	}
	// A converting copy constructor:
	template<class U, StlAllocHeapToUse stlAllocHeapToUse> MySTLAlloc(const MySTLAlloc<U, stlAllocHeapToUse>& other)
	{
	}
	template<class U, StlAllocHeapToUse stlAllocHeapToUse> bool operator==(const MySTLAlloc<U, stlAllocHeapToUse>&) const
	{
		return true;
	}
	template<class U, StlAllocHeapToUse stlAllocHeapToUse> bool operator!=(const MySTLAlloc<U, stlAllocHeapToUse>&) const
	{
		return false;
	}
	template <class U>
	struct rebind
	{
		typedef MySTLAlloc<U, stlAllocHeapToUse> other;
	};

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
		T *pv = static_cast<T*>(MyAllocate(nSize));
		if (pv == nullptr)
		{
			if (stlAllocHeapToUse == StlAllocUseCallStackHeap)
			{
				g_MyStlAllocStats._fReachedMemLimit = true;
				throw std::bad_alloc();
			}
		}
		InterlockedAdd(&g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[stlAllocHeapToUse], nSize);
		InterlockedAdd(&g_MyStlAllocStats._MyStlAllocBytesEverAlloc[stlAllocHeapToUse], nSize);
		return pv;
	}
	void deallocate(T* const p, size_t n) const
	{
		unsigned nSize = (UINT)n * sizeof(T);
		InterlockedAdd(&g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[stlAllocHeapToUse], -((int)nSize));
		InterlockedAdd(&g_MyStlAllocStats._MyStlTotBytesEverFreed[stlAllocHeapToUse], +(int)nSize);
		MyFree((PVOID)p);
	}

	PVOID MyAllocate(SIZE_T size) const
	{
		PVOID pmem;
		HANDLE hHeap;
		switch (stlAllocHeapToUse)
		{
		case StlAllocUseProcessHeap:
			hHeap = GetProcessHeap();
			break;
		case StlAllocUseCallStackHeap:
		case StlAllocUseTlsHeap:
			hHeap = HeapHolder::_instance.g_hHeapDetourData;
			_ASSERT_EXPR(hHeap != 0, L"Heap null");
			break;
		default:
			break;
		}
		if (Real_RtlAllocateHeap != nullptr)
		{
			pmem = Real_RtlAllocateHeap(hHeap, 0, size);
		}
		else
		{
			pmem = HeapAlloc(hHeap, 0, size);
		}
		return pmem;
	}

	void MyFree(PVOID pmem) const
	{
		HANDLE hHeap;
		switch (stlAllocHeapToUse)
		{
		case StlAllocUseProcessHeap:
			hHeap = GetProcessHeap();
			break;
		case StlAllocUseCallStackHeap:
		case StlAllocUseTlsHeap:
			hHeap = HeapHolder::_instance.g_hHeapDetourData;
			break;
		default:
			break;
		}
		HeapFree(hHeap, 0, pmem);
	}

	~MySTLAlloc() {

	}
};

//template <class T>
//inline void hash_combine(std::size_t & seed, const T & v)
//{
//    std::hash<T> hasher;
//    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//}
//
//// need a hash function for pair<int,int>
//template<typename S, typename T> struct std::hash<std::pair<S, T>>
//{
//    inline size_t operator()(const std::pair<S, T> & v) const
//    {
//        size_t seed = 0;
//        ::hash_combine(seed, v.first);
//        ::hash_combine(seed, v.second);
//        return seed;
//    }
//};




void InitCollectStacks();
void UninitCollectStacks();


bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraData, int numFramesToSkip);// noexcept;

void DoSomeManagedCode();
void DoSomeThreadingModelExperiments();

LONGLONG GetNumStacksCollected();


