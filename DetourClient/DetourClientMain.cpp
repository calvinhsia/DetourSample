#include <Windows.h>
#include "..\DetourSharedBase\DetourShared.h"
#include "crtdbg.h"
#include "stdio.h"

void CollectStacks(int size);
LONGLONG GetNumStacksCollected();
int g_tlsIndex;
pfnRtlAllocateHeap Real_RtlAllocateHeap;
SIZE_T g_AllocSizeThresholdForStackCollection = 0;// 1024 * 1024;
extern int g_nTotalAllocs;
extern LONGLONG g_TotalAllocSize;

void DoSomeManagedCode();


#define USETLSINDEX 1

#if !USETLSINDEX

	#define ISRECUR tlRecursionHeapAlloc
	#define SETRECUR tlRecursionHeapAlloc=true;
	#define SETRECURDONE tlRecursionHeapAlloc=false;
#else


/*
Using TlsIndex is a little complicated with heap alloc: When a thread is intialized, another DLL can call TlsAlloc, 
which tries to allocate more memory, but our Tls data hasn't been initialized yet.

		>	DetourClient.dll!MyRtlAllocateHeap(void * hHeap, unsigned long dwFlags, unsigned long size) Line 36	C++
		ntdll.dll!LdrpGetNewTlsVector(unsigned long EntryCount) Line 674	C
		ntdll.dll!LdrpAllocateTls() Line 810	C
		ntdll.dll!LdrpInitializeThread(_CONTEXT * Context) Line 6244	C
		ntdll.dll!_LdrpInitialize(_CONTEXT * UserContext, void * NtdllBaseAddress) Line 1754	C
		ntdll.dll!LdrpInitialize(_CONTEXT * UserContext, void * NtdllBaseAddress) Line 1403	C
		ntdll.dll!LdrInitializeThunk(_CONTEXT * UserContext, void * NtdllBaseAddress) Line 75	C

So we'll use 0 for Uninitialized. 1 for IsRecur, 2 for RecurDone
*/

#define TLSNotRecurring (PVOID)2 
#define TLSIsInitialized (TlsGetValue(g_tlsIndex) != 0)
#define TLSISRecur 1
#define ISRECUR (TlsGetValue(g_tlsIndex) == (PVOID)TLSISRecur)
#define SETRECUR TlsSetValue(g_tlsIndex,(PVOID)TLSISRecur);
#define SETRECURDONE TlsSetValue(g_tlsIndex,TLSNotRecurring);


#endif

PVOID WINAPI MyRtlAllocateHeap(HANDLE hHeap, ULONG dwFlags, SIZE_T size)
{
	if (size > g_AllocSizeThresholdForStackCollection)
	{
#if !USETLSINDEX
		thread_local 
		static bool tlRecursionHeapAlloc = false;
#endif
		/*
		tlsindex is valid, but the tls data is 0, dereferencing ecx:

		0F434A6E A1 F8 5B 44 0F       mov         eax,dword ptr [_tls_index (0F445BF8h)]
		0F434A73 64 8B 0D 2C 00 00 00 mov         ecx,dword ptr fs:[2Ch]
		->0F434A7A 8B 14 81             mov         edx,dword ptr [ecx+eax*4]

		see https://blogs.msdn.microsoft.com/vcblog/2014/12/08/visual-studio-2015-preview-work-in-progress-security-feature/
		Unhandled exception thrown: read access violation.
		**___guard_check_icall_fptr** was 0x111012E. occurred
		*/
		if (!ISRECUR) // this is the more expensive thread local check
		{
			SETRECUR;
			CollectStacks((int)size);
			SETRECURDONE;
		}
	}
	return Real_RtlAllocateHeap(hHeap, dwFlags, size);
}


pfnMessageBoxA g_real_MessageBoxA;
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

pfnGetModuleHandle g_real_GetModuleHandleA;
HMODULE WINAPI MyGetModuleHandleA(LPCSTR lpModuleName)
{
	return g_real_GetModuleHandleA(lpModuleName);
}

pfnGetModuleFileNameA g_real_GetModuleFileNameA;
DWORD WINAPI MyGetModuleFileNameA(
	_In_opt_ HMODULE hModule,
	_Out_writes_to_(nSize, ((return < nSize) ? (return +1) : nSize)) LPSTR lpFilename,
	_In_ DWORD nSize
)
{
	strcpy_s(lpFilename, nSize, "InDetouredGetModuleFileName");
	return (DWORD)strnlen_s(lpFilename, nSize);
}


// To enable detours, they must already have been detoured much earlier, probably before this module has been loaded,
// and we call RedirectDetour to enable the detour
// note: this is not transacted (all or nothing), so a partial success might
// need to be undone
void HookInMyOwnVersion(BOOL fHook)
{
	HMODULE hmDevenv = GetModuleHandleA(nullptr);

	//	g_mapStacks = new (malloc(sizeof(mapStacks)) mapStacks(MySTLAlloc < pair<const SIZE_T, vecStacks>(GetProcessHeap());

	auto fnRedirectDetour = reinterpret_cast<pfnRedirectDetour>(GetProcAddress(hmDevenv, REDIRECTDETOUR));
	_ASSERT_EXPR(fnRedirectDetour != nullptr, L"Failed to get RedirectDetour");

	if (fHook)
	{
		if (fnRedirectDetour(DTF_MessageBoxA, MyMessageBoxA, (PVOID *)&g_real_MessageBoxA) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleHandleA, MyGetModuleHandleA, (PVOID *)&g_real_GetModuleHandleA) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleFileNameA, MyGetModuleFileNameA, (PVOID *)&g_real_GetModuleFileNameA) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}
		auto res = fnRedirectDetour(DTF_RtlAllocateHeap, MyRtlAllocateHeap, (PVOID *)&Real_RtlAllocateHeap);
		_ASSERT_EXPR(res == S_OK, L"Redirecting detour to allocate heap");
	}
	else
	{
		if (fnRedirectDetour(DTF_MessageBoxA, nullptr, nullptr) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleHandleA, nullptr, nullptr) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}
		if (fnRedirectDetour(DTF_GetModuleFileNameA, nullptr, nullptr) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}

		// when undetouring heap, be careful because HeapAlloc can call HeapAlloc
		if (fnRedirectDetour(DTF_RtlAllocateHeap, nullptr, nullptr) != S_OK)
		{
			_ASSERT_EXPR(false, L"Failed to redirect detour");
		}
	}
}

CLINKAGE void EXPORT StartVisualStudio()
{
	HookInMyOwnVersion(true);
	auto h = GetModuleHandleA(0);
	char buff[MAX_PATH] = { '\0' };
	GetModuleFileNameA(0, buff, sizeof(buff));
	MessageBoxA(0, buff, "Calling the WinApi version of MessageboxA", 0);
	for (int i = 0; i < 1000; i++)
	{
		void *p = HeapAlloc(GetProcessHeap(), 0, 1000 + i);
		HeapFree(GetProcessHeap(), 0, p);
	}
	DoSomeManagedCode();

	// now undo detour redirection, so detouring to stubs:
	HookInMyOwnVersion(false);

	GetModuleFileNameA(0, buff, sizeof(buff));

	LONGLONG nStacksCollected = GetNumStacksCollected();
	sprintf_s(buff, _countof(buff), "Detours unhooked, calling WinApi MessageboxA # allocs = %d   AllocSize = %lld  Stks Collected=%lld", g_nTotalAllocs, g_TotalAllocSize, nStacksCollected);
	MessageBoxA(0, buff, "Calling the WinApi version of MessageboxA", 0);
	_ASSERT_EXPR(g_nTotalAllocs > 400 && g_TotalAllocSize > 70000, L"#expected > 2400 allocations of >700k bytes");
	_ASSERT_EXPR(nStacksCollected > 50, L"#expected > 50 stacks collected");

}


BOOL WINAPI DllMain
(
	HINSTANCE hInst,
	ULONG     ulReason,
	LPVOID
)
{
	auto retval = true;

	switch (ulReason)
	{
	case DLL_PROCESS_ATTACH:
#if USETLSINDEX
		g_tlsIndex = TlsAlloc();
		_ASSERT_EXPR(g_tlsIndex != 0, L"TlsAlloc");
		TlsSetValue(g_tlsIndex, TLSNotRecurring);
#endif USETLSINDEX

		break;

	case DLL_THREAD_ATTACH:
#if USETLSINDEX
		TlsSetValue(g_tlsIndex, TLSNotRecurring);
#endif USETLSINDEX
		break;

	case DLL_PROCESS_DETACH:
		TlsFree(g_tlsIndex);
		break;
	case DLL_THREAD_DETACH:
		break;
	}

	return retval;
}
