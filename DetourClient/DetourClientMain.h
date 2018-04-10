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
extern HANDLE g_hHeapDetourData;

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
		InterlockedAdd(&g_MyStlAllocStats._MyStlTotBytesEverFreed[stlAllocHeapToUse], +(int) nSize);
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
			hHeap = g_hHeapDetourData;
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
			hHeap = g_hHeapDetourData;
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


typedef std::vector<PVOID
	, MySTLAlloc<PVOID, StlAllocUseCallStackHeap>
> vecFrames;

// Collects the callstack and calculates the stack hash
// represents a single call stack and how often the identical stack occurs
struct CallStack
{
	CallStack(int NumFramesToSkip) : _nOccur(1)
	{
		_vecFrames.resize(g_NumFramesTocapture);
		int nFrames = RtlCaptureStackBackTrace(
			/*FramesToSkip*/ NumFramesToSkip,
			/*FramesToCapture*/ g_NumFramesTocapture,
			&_vecFrames[0],
			&_stackHash
		);
		_vecFrames.resize(nFrames);
	}
	CallStack(CallStack&& other) // move constructor
	{
		_stackHash = other._stackHash;
		_nOccur = other._nOccur;
		_vecFrames = std::move(other._vecFrames);
	}
	CallStack& operator = (CallStack&& other) // move assignment
	{
		if (this != &other)
		{
			_stackHash = other._stackHash;
			_nOccur = other._nOccur;
			_vecFrames = std::move(other._vecFrames);
		}
		return *this;
	}

	ULONG _stackHash; // hash of stack 4 bytes in both x86 and amd64
	int _nOccur;   // # of occurrences of this particular stack
	vecFrames _vecFrames; // the stack frames
};

typedef std::unordered_map<UINT, CallStack, // can't use unique_ptr because can't override it's allocator and thus can cause deadlock
	std::hash<UINT>,
	std::equal_to<UINT>,
	MySTLAlloc<std::pair<const UINT, CallStack>, StlAllocUseCallStackHeap >
> mapStackHashToStack; // stackhash=>CallStack

// represents the stacks for a particular stack type : e.g. the 100k allocations
// if the stacks are identical, the count is bumped.
struct StacksForStackType
{
	StacksForStackType(int numFramesToSkip)
	{
		AddNewStack(numFramesToSkip);
	}
	StacksForStackType(StacksForStackType&& other) // move constructor
	{
		_stacks = std::move(other._stacks);
	}
	StacksForStackType & operator = (StacksForStackType&& other) // move assignment
	{
		if (this != &other)
		{
			this->_stacks = std::move(other._stacks);
		}
		return *this;
	}
	bool AddNewStack(int numFramesToSkip)
	{
		bool fDidAdd = false;
		if (!g_MyStlAllocStats._fReachedMemLimit)
		{
			CallStack stack(numFramesToSkip);
			auto hash = stack._stackHash;
			auto res = _stacks.find(hash);
			if (res == _stacks.end())
			{
				// a new stack we haven't seen before
				// could have reached limit because we used more mem
				if (!g_MyStlAllocStats._fReachedMemLimit)
				{
					g_MyStlAllocStats._NumUniqueStacks++;
					g_MyStlAllocStats._nTotFramesCollected += (long)stack._vecFrames.size();
					_stacks.insert(mapStackHashToStack::value_type(hash, std::move(stack)));
					fDidAdd = true;
				}
			}
			else
			{
				res->second._nOccur++;
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
			tot += stack.second._nOccur;
		}
		return tot;
	}
	// map of stack hash to CalLStack
	mapStackHashToStack _stacks;

};

typedef UINT mapKey;

typedef std::unordered_map<mapKey, StacksForStackType	,
	std::hash<mapKey>,
	std::equal_to<mapKey>,
	MySTLAlloc<std::pair<const mapKey, StacksForStackType>, StlAllocUseCallStackHeap>
> mapStacksByStackType;

// map the Size of an alloc to all the stacks that allocated that size.
// note: if we're looking for all allocs of a specific size (e.g. 1Mb), then no need for a map by size (because all keys will be the same): more efficient to just use a mapStacks
extern mapStacksByStackType *g_pmapStacksByStackType[];


void InitCollectStacks();
void UninitCollectStacks();


bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraData, int numFramesToSkip);// noexcept;

void DoSomeManagedCode();
void DoSomeThreadingModelExperiments();

LONGLONG GetNumStacksCollected();


