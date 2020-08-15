#include <Windows.h>
#include "..\DetourSharedBase\DetourShared.h"
#include "crtdbg.h"
#include "stdio.h"

#include <atlbase.h>
#include <atlcom.h>

#include "DetourClientMain.h"


using namespace std;




DWORD g_dwMainThread;


#define NUMTHREADS 35

void CreateComObject();

int MyTlsData::g_tlsIndex;
MyCriticalSectionNoDebugInfo MyTlsData::g_tlsCritSect;
bool volatile MyTlsData::g_IsCreatingTlsData;
long MyTlsData::g_numTlsInstances;

#if _DEBUG
int MyTlsData::_tlsSerialNo;
#endif _DEBUG


mapThreadIdToTls* g_pmapThreadIdToTls;

//static 
bool MyTlsData::DllMain(ULONG ulReason)
{
	switch (ulReason)
	{
	case DLL_PROCESS_ATTACH:
		g_tlsIndex = TlsAlloc();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
	{
		UninitCollectStacks();
		TlsFree(g_tlsIndex);
		g_tlsIndex = 0;
	}
	break;
	case DLL_THREAD_DETACH:
		if (g_pmapThreadIdToTls != nullptr)
		{
			CComCritSecLock<decltype(g_tlsCritSect)> lock(g_tlsCritSect);
			auto res = g_pmapThreadIdToTls->find(GetCurrentThreadId());

			if (res == g_pmapThreadIdToTls->end())
			{
				// a thread is exiting for which we never created a Tls struct
				VSASSERT(TlsGetValue(g_tlsIndex) == 0, "how can there be TLS value with no TLS data?");
			}
			else
			{
				res->second->~MyTlsData();
				// create an allocator type for BYTE from the same allocator at the map
				using myByteAllocator = typename allocator_traits<mapThreadIdToTls::allocator_type>::rebind_alloc<BYTE>;
				// create an instance of the allocator type
				mapThreadIdToTls::allocator_type alloc_type;

				myByteAllocator allocatorByte(alloc_type);
				allocatorByte.deallocate((BYTE*)res->second, sizeof(*res->second));
				g_pmapThreadIdToTls->erase(res);
			}
		}
		break;
	}
	return true;
}

//static 
MyTlsData* MyTlsData::GetTlsData()
{
	auto pMyTlsData = (MyTlsData*)TlsGetValue(g_tlsIndex);
	if (pMyTlsData == nullptr && g_pmapThreadIdToTls != nullptr)
	{
		CComCritSecLock<decltype(g_tlsCritSect)> lock(g_tlsCritSect);
		if (!g_IsCreatingTlsData)
		{/*
		 Because new thread creation can grow TLS structures, which call heapalloc internally, and those
		 heapalloc calls can internally grow internal heap blocks by calling heapalloc, we cannot
		 recur and we cannot create our TLS data in the same heap at this time.
		 thus we could potentially miss an allocation detour because detour prevention is done on all threads (g_IsCreatingTlsData is a static)

		 >	DetourClient.dll!MyRtlAllocateHeap(void *, unsigned long, unsigned long)	C++
		 ntdll.dll!RtlpAllocateUserBlockFromHeap(_HEAP *, unsigned char, unsigned long, unsigned char)	C
		 ntdll.dll!RtlpAllocateUserBlock(_LFH_HEAP *, unsigned char, unsigned long, unsigned char)	C
		 ntdll.dll!RtlpLowFragHeapAllocFromContext(void *, unsigned short, unsigned long, unsigned long)	C
		 ntdll.dll!RtlpAllocateHeapInternal(void *, unsigned long, unsigned long, unsigned short)	C
		 [Inline Frame] ntdll.dll!RtlpHpHeapAllocateRedirectLayer(void *, unsigned long, unsigned long)	C
		 ntdll.dll!RtlAllocateHeap(void *, unsigned long, unsigned long)	C
		 DetourClient.dll!MyTlsData::GetTlsData()	C++
		 DetourClient.dll!MyRtlAllocateHeap(void *, unsigned long, unsigned long)	C++
		 ntdll.dll!LdrpGetNewTlsVector(unsigned long)	C
		 ntdll.dll!LdrpAllocateTls()	C

		 */

			g_IsCreatingTlsData = true;
			auto res = g_pmapThreadIdToTls->find(GetCurrentThreadId());
			if (res != g_pmapThreadIdToTls->end())
			{
				VSASSERT(((MyTlsData*)TlsGetValue(g_tlsIndex))->_dwThreadId == GetCurrentThreadId(), "tls already created?");
			}
			else
			{
				// create an allocator type for BYTE from the same allocator at the map
				using myTlsDataAllocator = typename allocator_traits<mapThreadIdToTls::allocator_type>::rebind_alloc<MyTlsData>;
				// create an instance of the allocator type
				mapThreadIdToTls::allocator_type alloc_type;

				myTlsDataAllocator allocatorByte(alloc_type);
				auto pmem = allocatorByte.allocate(1);
				pMyTlsData = new (pmem) MyTlsData();
				if (TlsGetValue(g_tlsIndex) != nullptr)
				{
					VSASSERT(TlsGetValue(g_tlsIndex) == nullptr, "tlsvalue should be 0");
				}
				(*g_pmapThreadIdToTls)[GetCurrentThreadId()] = pmem;
				auto ret = TlsSetValue(g_tlsIndex, pMyTlsData);
				VSASSERT(ret, "Failed to set tls value");

			}
			g_IsCreatingTlsData = false;
		}
	}
	return pMyTlsData;
}

MyTlsData::MyTlsData() //ctor
{
#if _DEBUG
	_dwThreadId = GetCurrentThreadId();
	_nSerialNo = _tlsSerialNo++;
#endif _DEBUG
	_fIsInRtlAllocHeap = false;
	InterlockedIncrement(&g_numTlsInstances);
}

MyTlsData::~MyTlsData()
{
	InterlockedDecrement(&g_numTlsInstances);
}


long g_MyRtlAllocateHeapCount = 0;

PVOID WINAPI MyRtlAllocateHeap(HANDLE hHeapHandle, ULONG dwFlags, SIZE_T size)
{
	InterlockedIncrement(&g_MyRtlAllocateHeapCount);
#if _DEBUG
	static int recurcount = 0;
	if (recurcount++ > NUMTHREADS)
	{
		//		_asm int 3
	}
#endif _DEBUG
	PVOID pMem = Real_RtlAllocateHeap(hHeapHandle, dwFlags, size);
	auto pMyTlsData = MyTlsData::GetTlsData();
	if (pMyTlsData != nullptr)
	{
		/*
		// heapalloc is called when initializing a new thread, so we can't use TLS until it's been initialized, so we bailout

		>	DetourClient.dll!MyRtlAllocateHeap(void * hHeap, unsigned long dwFlags, unsigned long size) Line 72	C++	Symbols loaded.
		DetourSharedBase.exe!MyStubRtlAllocateHeap(void * hHeapHandle, unsigned long dwFlags, unsigned long nSize) Line 77	C++	Symbols loaded.
		ntdll.dll!LdrpGetNewTlsVector(unsigned long EntryCount) Line 674	C	Symbols loaded.
		ntdll.dll!LdrpAllocateTls() Line 810	C	Symbols loaded.
		ntdll.dll!LdrpInitializeThread(_CONTEXT * Context) Line 6244	C	Symbols loaded.
		ntdll.dll!_LdrpInitialize(_CONTEXT * UserContext, void * NtdllBaseAddress) Line 1754	C	Symbols loaded.
		ntdll.dll!LdrpInitialize(_CONTEXT * UserContext, void * NtdllBaseAddress) Line 1403	C	Symbols loaded.
		ntdll.dll!LdrInitializeThunk(_CONTEXT * UserContext, void * NtdllBaseAddress) Line 75	C	Symbols loaded.
		*/
		if (!pMyTlsData->_fIsInRtlAllocHeap)
		{
			pMyTlsData->_fIsInRtlAllocHeap = true;
			bool fDidCollectStack = false;
			if (g_heapAllocSizes.size() > 0)
			{
				// we search through all but the last entry (end() -1) which is used to store any stacks of allocations > g_HeapAllocSizeMinValue
				auto it = find_if(g_heapAllocSizes.begin(), g_heapAllocSizes.end() - 1, [size](HeapSizeData data)
					{
						if (data._nSize == size)
						{
							return true;
						}
						return false;
					});
				if (it != g_heapAllocSizes.end())
				{
					if (it->_nSize == size ||
						size >= (SIZE_T)g_HeapAllocSizeMinValue) // these are huge allocations larger than e.g. 10M. We always try to collect stack
					{
						if (it->_nThreshold == 0)
						{
							fDidCollectStack = CollectStack(pMem, StackTypeHeapAlloc,/*stackSubType*/ (DWORD)size, /*extra data*/NULL, /*numFramesToSkip*/1);
							if (fDidCollectStack)
							{
								InterlockedIncrement(&it->_nStacksCollected);
							}
						}
						else
						{
							InterlockedDecrement(&it->_nThreshold);
						}
					}
				}
			}
			pMyTlsData->_fIsInRtlAllocHeap = false;
		}
	}
#if _DEBUG
	recurcount--;
#endif _DEBUG
	return pMem;
}


PVOID WINAPI MyHeapReAlloc( // no re-new
	HANDLE hHeap,
	DWORD dwFlags,
	LPVOID lpMem,
	SIZE_T dwBytes
)
{
	bool IsTracked = false;
	PVOID pNewMem = {};

	//LPVOID pBlock = (PBYTE)lpMem - nExtraBytes;
	//if (((PDWORD)pBlock)[0] == MySignature)
	//{
	//    DWORD dwSizeAlloc = ((PDWORD)pBlock)[1];
	//    if (UnCollectStack(dwSizeAlloc))
	//    {
	//        IsTracked = true;

	//        pNewMem = MyRtlAllocateHeap(hHeap, dwFlags, dwBytes);
	//        memmove(pNewMem, lpMem, dwBytes < dwSizeAlloc ? dwBytes : dwSizeAlloc);
	//    }
	//}
	if (!IsTracked)
	{
		pNewMem = Real_HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
	}
	return pNewMem;
}


BOOL WINAPI MyRtlFreeHeap(
	HANDLE hHeap,
	DWORD dwFlags,
	LPVOID lpMem
)
{
	auto res = Real_RtlFreeHeap(hHeap, dwFlags, lpMem);
	return res;
}



#ifndef _WIN64

////_declspec (thread) // note: when this code runs in VS, random failures when initializing the std::stack in a TLS: the stack would be empty even when the prior line says push(). workaround: make the TLS point to a struct containing the stl::stack
//std::stack<std::pair<LPVOID, DWORD>> _stackRetAddresses; // return address, TickCount
//
//void _stdcall pushvalue(LPVOID val)
//{
//	_stackRetAddresses.push(pair<LPVOID, DWORD>(val, GetTickCount()));
//}
//
//LPVOID _stdcall popvalue()
//{
//	auto top = _stackRetAddresses.top();
//	auto retAddr = top.first;
//	auto elapsed = GetTickCount() - top.second;
//	_stackRetAddresses.pop();
//	DWORD stackSubType = 0;
//	if (GetCurrentThreadId() == g_dwMainThread)
//	{
//		stackSubType = 0;
//	}
//	else
//	{
//		stackSubType = 1;
//	}
//	CollectStack(StackTypeRpc, stackSubType, elapsed, /*numFramesToSkip*/3);
//	return retAddr;
//}
//
//PVOID Real_NdrClientCall2;
//_declspec(naked) void DetourNdrClientCall2()
//{
//	/*
//	detouring NdrClientCall2 is a little complicated because it's CDECL. Wrapping (running code before/after the call to get elapsed time) is thus more complex
//	https://msdn.microsoft.com/en-us/library/windows/desktop/aa374215(v=vs.85).aspx
//	Network Data Representation (NDR) Engine
//	A caller looks like:
//
//	CLIENT_CALL_RETURN RPC_VAR_ENTRY
//	NdrClientCall4(
//	PMIDL_STUB_DESC     pStubDescriptor,
//	PFORMAT_STRING      pFormat,
//	...
//	)
//	{
//	RPCP_PAGED_CODE();
//
//	va_list                     ArgList;
//
//	//
//	// On x86 we pass in a pointer to the first parameter, and expect that
//	// the rest of the parameters are directly behind it on the stack. The
//	// only way this silly assumption is true is if we disable optimization
//	// in the stub. We will switch to passing in all the parameters the same
//	// as in AMD64 and ARM. Due to back compat, we can't change NdrClientCall2,
//	// so this is a copy of the AMD/ARM version of NdrClientCall2 only called
//	// on x86 running Win8.1 or newer.
//	//
//	INIT_ARG( ArgList, pFormat);
//	uchar *StartofStack = (uchar*)GET_STACK_START(ArgList);
//
//	return NdrClientCall2( pStubDescriptor, pFormat, StartofStack );
//	}
//
//
//	The asm looks like:
//	lea         eax,[ebp+10h]
//	push        eax
//	push        dword ptr [ebp+0Ch]
//	push        dword ptr [ebp+8]
//	call        NdrClientCall2 (76C57590h)
//	add         esp,0Ch
//
//	this is the prototype:
//	CLIENT_CALL_RETURN RPC_VAR_ENTRY
//	NdrClientCall2(
//	PMIDL_STUB_DESC     pStubDescriptor,
//	PFORMAT_STRING      pFormat,
//	...
//	);
//	Upon entry here, ESP points to the caller return address (where the caller cleans the stack by adding 4 * the # of parms it pushed to ESP)
//	*/
//	_asm call pushvalue // call psuhvalue (which expects 1 param, but we don't push one, so it uses the curvalue on the stack which is the ret addr) to put return addr in our threadlocal std::stack
//	_asm call Real_NdrClientCall2   // call the real code (it's cdecl with variable # parms)
//
//	_asm push eax       // We're just Pushing here to decrement the stack by 4 to save space to put the orig return addr. This slot will be replaced by the orig ret addr
//	_asm push eax       // save real function's return value
//	_asm call popvalue  // get the original return address in eax
//	_asm mov[esp + 4], eax       // put the original ret addr on stack where the "ret" below will return correctly
//
//	_asm pop eax      // restore the real function's return value
//	_asm ret          // ret to caller
//}
#endif _WIN64

decltype(&MessageBoxA) g_real_MessageBoxA;
int WINAPI MyMessageBoxA(
	_In_opt_ HWND hWnd,
	_In_opt_ LPCSTR lpText,
	_In_opt_ LPCSTR lpCaption,
	_In_ UINT uType)
{
	lpCaption = "The detoured version of MessageboxA";
	//static int stackCnt = 0;
	//CollectStacks(stackCnt++);
	return g_real_MessageBoxA(hWnd, lpText, lpCaption, uType);
}

decltype(&GetModuleHandleA) g_real_GetModuleHandleA;
HMODULE WINAPI MyGetModuleHandleA(LPCSTR lpModuleName)
{
	return g_real_GetModuleHandleA(lpModuleName);
}

decltype(&GetModuleFileNameA) g_real_GetModuleFileNameA;
DWORD WINAPI MyGetModuleFileNameA(
	_In_opt_ HMODULE hModule,
	_Out_writes_to_(nSize, ((return < nSize) ? (return +1) : nSize)) LPSTR lpFilename,
	_In_ DWORD nSize
)
{
	strcpy_s(lpFilename, nSize, "InDetouredGetModuleFileName");
	return (DWORD)strnlen_s(lpFilename, nSize);
}


//class myclass
//{
//public:
//	myclass()
//	{
//		_dwThreadId = GetCurrentThreadId();
//		_pMem = malloc(100);
//	}
//	DWORD _dwThreadId;
//	LPVOID _pMem;
//public:
//	~myclass()
//	{
//		delete _pMem;
//	}
//};
//
//thread_local myclass g_myclass;


// To enable detours, they must already have been detoured much earlier, probably before this module has been loaded,
// and we call RedirectDetour to enable the detour
// note: this is not transacted (all or nothing), so a partial success might
// need to be undone
void HookInMyOwnVersion(BOOL fHook)
{

	HMODULE hmDevenv = GetModuleHandleA("DetourLib.dll");

	//	g_mapStacks = new (malloc(sizeof(mapStacks)) mapStacks(MySTLAlloc < pair<const SIZE_T, vecStacks>(GetProcessHeap());
	if (hmDevenv == 0)
	{
		return;
	}
	auto fnRedirectDetour = reinterpret_cast<pfnRedirectDetour>(GetProcAddress(hmDevenv, REDIRECTDETOUR));
	VSASSERT(fnRedirectDetour != nullptr, "Failed to get RedirectDetour");

	if (fHook)
	{
		if (fnRedirectDetour(DTF_MessageBoxA, MyMessageBoxA, (PVOID*)&g_real_MessageBoxA) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleHandleA, MyGetModuleHandleA, (PVOID*)&g_real_GetModuleHandleA) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleFileNameA, MyGetModuleFileNameA, (PVOID*)&g_real_GetModuleFileNameA) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		auto res = fnRedirectDetour(DTF_RtlAllocateHeap, MyRtlAllocateHeap, (PVOID*)&Real_RtlAllocateHeap);
		VSASSERT(res == S_OK, "Redirecting detour to allocate heap");

		res = fnRedirectDetour(DTF_HeapReAlloc, MyHeapReAlloc, (PVOID*)&Real_HeapReAlloc);
		VSASSERT(res == S_OK, "Redirecting detour to heapReAlloc");


		res = fnRedirectDetour(DTF_RtlFreeHeap, MyRtlFreeHeap, (PVOID*)&Real_RtlFreeHeap);
		VSASSERT(res == S_OK, "Redirecting detour to free heap");

#ifndef _WIN64
		//res = fnRedirectDetour(DTF_NdrClientCall2, DetourNdrClientCall2, (PVOID*)&Real_NdrClientCall2);
		//VSASSERT(res == S_OK, "Redirecting detour to MyNdrClientCall2");
#endif _WIN64

	}
	else
	{
		if (fnRedirectDetour(DTF_MessageBoxA, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleHandleA, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleFileNameA, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}

		// when undetouring heap, be careful because HeapAlloc can call HeapAlloc
		if (fnRedirectDetour(DTF_RtlAllocateHeap, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_HeapReAlloc, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_RtlFreeHeap, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_NdrClientCall2, nullptr, nullptr) != S_OK)
		{
			VSASSERT(false, "Failed to redirect detour");
		}
	}
}

void splitStr(wstring str, char sepChar, function<void(wstring val)> callback)
{
	auto ndxSeparator = str.find(sepChar);
	size_t ndxStart = 0;
	while (ndxSeparator != string::npos)
	{
		callback(str.substr(ndxStart, ndxSeparator - ndxStart));
		ndxStart = ndxSeparator + 1;
		ndxSeparator = str.find_first_of(sepChar, ndxStart);
	}
	auto leftover = str.substr(ndxStart, -1);
	if (leftover.size() > 0)
	{
		callback(leftover);
	}
}

void RecurDownSomeLevels(int nLevel)
{
	if (nLevel > 0)
	{
		RecurDownSomeLevels(nLevel - 1);
		nLevel--; // prevent optimization
	}
	else
	{
		auto x = HeapAlloc(GetProcessHeap(), 0, 1000);
		HeapFree(GetProcessHeap(), 0, x);
		//		CollectStack(StackTypeHeapAlloc, 1, 0, 2);
//		g_MyStlAllocStats.clear();
	}
}

DWORD WINAPI ThreadRoutine(PVOID param)
{
	int threadIndex = (int)(INT_PTR)(param);
	RecurDownSomeLevels(threadIndex);
	for (int i = 0; i < 1000; i++)
	{
		auto x = HeapAlloc(GetProcessHeap(), 0, 1000);
		HeapFree(GetProcessHeap(), 0, x);
	}
	return 0;
}


void DoLotsOfThreads()
{
	DWORD dwThreadIds[NUMTHREADS] = {};
	HANDLE hThreads[NUMTHREADS] = {};
	for (int iter = 0; iter < 100; iter++)
	{
		for (int iThread = 0; iThread < NUMTHREADS; iThread++)
		{
			hThreads[iThread] = CreateThread(/*LPSECURITY_ATTRIBUTES=*/NULL,
				/*dwStackSize=*/ 1024 * 256,
				&ThreadRoutine,
				/* lpThreadParameter*/(PVOID)(INT_PTR)iThread,
				/*dwCreateFlags*/ 0, /// CREATE_SUSPENDED
				&dwThreadIds[iThread]
			);
		}
		WaitForMultipleObjects(NUMTHREADS, hThreads, /*bWaitAll*/ true, /*dwMilliseconds*/ INFINITE);
	}
	auto x = 2;

}

void SetHeapSizesToCollect(wstring Sizes)
{
	splitStr(Sizes, ',', [](wstring valPair)
		{
			auto ndxColon = valPair.find(':');
			auto heapSize = _wtol(valPair.substr(0, ndxColon).c_str());
			auto heapThresh = _wtol(valPair.substr(ndxColon + 1).c_str());
			g_heapAllocSizes.push_back(HeapSizeData(heapSize, heapThresh));

		});
	if (g_HeapAllocSizeMinValue > 0)
	{
		// add one more at the end for any allocation > the min threshold
		g_heapAllocSizes.push_back(HeapSizeData(g_HeapAllocSizeMinValue, 0));
	}
}


CLINKAGE void EXPORT StartVisualStudio()
{
	InitCollectStacks();

	{
		// experiment with shared_ptr. Can use custom allocator, but can't get attach/detach to work for TlsSetValue
		typedef unordered_map < DWORD, shared_ptr<MyTlsData>,
			hash<DWORD>, equal_to<DWORD>, MySTLAlloc<pair<DWORD, shared_ptr<MyTlsData>>, StlAllocUseTlsHeap>> mapThreadIdTls;
		mapThreadIdTls mymap;

		mapThreadIdTls::allocator_type alloc;
		{
			shared_ptr<MyTlsData> pp = allocate_shared<MyTlsData, MySTLAlloc<MyTlsData, StlAllocUseTlsHeap>>(alloc);
		}
		mymap[1] = allocate_shared<MyTlsData, MySTLAlloc<MyTlsData, StlAllocUseTlsHeap>>(alloc);
	}
	{
		/*
		// experiment with shared_ptr<unique_ptr<.  Can't use because no custom allocator for unique_ptr
		typedef unordered_map < DWORD, shared_ptr<MyTlsData>,
			hash<DWORD>, equal_to<DWORD>, MySTLAlloc<pair<DWORD, shared_ptr<MyTlsData>>, StlAllocUseTlsHeap>> mapThreadIdTls;
		mapThreadIdTls mymap;
		mapThreadIdTls::allocator_type alloc;
		{
			shared_ptr<unique_ptr<MyTlsData>> ptr = allocate_shared<unique_ptr<MyTlsData>, MySTLAlloc<unique_ptr<MyTlsData>, StlAllocUseTlsHeap>>(alloc);

		}
		*/
	}
	{

		mapThreadIdToTls mymap;
		// create an allocator type for BYTE from the same allocator at the map
		using myByteAllocator = typename allocator_traits<mapThreadIdToTls::allocator_type>::rebind_alloc<BYTE>;
		// create an instance of the allocator type
		mapThreadIdToTls::allocator_type alloc_type;

		myByteAllocator allocatorByte(alloc_type);
		auto p = new (allocatorByte.allocate(sizeof(MyTlsData))) MyTlsData();
		mymap[(DWORD)1] = (MyTlsData*)p;
		auto q = new (allocatorByte.allocate(sizeof(MyTlsData))) MyTlsData();
		mymap[(DWORD)2] = (MyTlsData*)q;
		auto res = mymap.find(1);
		res->second->~MyTlsData();
		allocatorByte.deallocate((BYTE*)res->second, sizeof(*res->second));
		mymap.erase(res);
		for (auto& x : mymap)
		{
			x.second->~MyTlsData();
			allocatorByte.deallocate((BYTE*)x.second, sizeof(*x.second));
		}
		mymap.clear();
	}
	{
		//mapThreadIdToTls mymap;// = new mapThreadIdToTls();
		//decltype(mymap.get_allocator())::rebind<BYTE>::other alloc;

		//auto p = new (alloc.allocate(sizeof(MyTlsData))) MyTlsData();
		//mymap[(DWORD)1] = (MyTlsData *)p;
		//auto q = new (alloc.allocate(sizeof(MyTlsData))) MyTlsData();
		//mymap[(DWORD)2] = (MyTlsData *)q;
		//auto res = mymap.find(1);
		//res->second->~MyTlsData();
		//alloc.deallocate((BYTE *)res->second, sizeof(*res->second));
		//mymap.erase(res);
		//for (auto &x : mymap)
		//{
		//	x.second->~MyTlsData();
		//	alloc.deallocate((BYTE *)x.second, sizeof(*x.second));
		//}
		//mymap.clear();
	}

	g_dwMainThread = GetCurrentThreadId();

	SetHeapSizesToCollect(L"8:271 , 72:220, 1031:40");

	SIZE_T sizeAlloc = 72;
	auto it = find_if(g_heapAllocSizes.begin(), g_heapAllocSizes.end(), [sizeAlloc](HeapSizeData data)
		{
			if (data._nSize == sizeAlloc)
			{
				return true;
			}
			return false;
		});
	if (it == g_heapAllocSizes.end())
	{
		auto x = 2;
	}
	else
	{
		if ((*it)._nThreshold == 0)
		{

		}
		else
		{
			(*it)._nThreshold--;
		}
	}




	RecurDownSomeLevels(200);

	HookInMyOwnVersion(true);




	auto h = GetModuleHandleA(0);
	string buff(MAX_PATH, '\0');

	GetModuleFileNameA(0, &buff[0], (DWORD)buff.length());
	//buff.resize(strlen(&buff[0]));

	MessageBoxA(0, buff.c_str(), "Calling the WinApi version of MessageboxA", 0);
	for (int i = 0; i < 10000; i++)
	{
		//        void *p = HeapAlloc(GetProcessHeap(), 0, 1000 + i);
				//    HeapFree(GetProcessHeap(), 0, p);
	}


	DoSomeThreadingModelExperiments();

	CreateComObject();

	DoLotsOfThreads();

	DoSomeManagedCode();


	// now undo detour redirection, so detouring to stubs:
	HookInMyOwnVersion(false);

	GetModuleFileNameA(0, &buff[0], (DWORD)buff.length());

	LONGLONG nStacksCollected = GetNumStacksCollected();
	sprintf_s(&buff[0], buff.length(), "Detours unhooked, calling WinApi MessageboxA # allocs = %d   AllocSize = %lld  Stks Collected=%lld    StackSpaceUsed=%d",
		g_MyStlAllocStats._nTotNumHeapAllocs, g_MyStlAllocStats._TotNumBytesHeapAlloc, nStacksCollected, g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[StlAllocUseCallStackHeap]);


	MessageBoxA(0, &buff[0], "Calling the WinApi version of MessageboxA", 0);


	VSASSERT(g_MyStlAllocStats._nTotNumHeapAllocs > 400 && g_MyStlAllocStats._TotNumBytesHeapAlloc > 3000, "#expected > 400 allocations of >3000 bytes");
	VSASSERT(nStacksCollected > 50, "#expected > 50 stacks collected");

}


BOOL WINAPI DllMain
(
	HINSTANCE hInst,
	ULONG     ulReason,
	LPVOID
)
{
	return MyTlsData::DllMain(ulReason);
}
