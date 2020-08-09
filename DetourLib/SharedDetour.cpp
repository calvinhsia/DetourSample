#include <Windows.h>
#include "..\Detours\detours.h"
#include "..\DetourSharedBase\DetourShared.h"
#include "stdio.h"


DetourTableEntry g_arrDetourTableEntry[DTF_MAX];

// using declsepc(naked) means we don't have to push/pop the params multiple times

#ifndef _WIN64
/*
STUBMACRO defines ASM code that takes the parameter as a DTF enum and looks up the DetourTableEntry in the array
g_arrDetourTableEntry. Hotpath has minimal branching
It then gets the redirected entry, and if it's 0, gets the real entry and does a JMP to that entry
*/
#define STUBMACRO(DTF)   \
{                        \
	_asm mov edx, (SIZE DetourTableEntry) * DTF /* calculate the index into the array*/\
	_asm add edx, offset g_arrDetourTableEntry /*add the base table address.*/ \
	_asm mov eax, [edx+4]  /* look at the 2nd entry in the table (the redirected value)*/ \
	_asm cmp eax, 0         /* cmp to 0*/ \
	_asm je b1             /* if it's zero, jump to b1*/ \
	_asm jmp eax           /* now jump to the redirected*/ \
	_asm b1:               /* jmp label */ \
    _asm jmp DWORD PTR [edx] /*jmp to the real */\
}

/*
DETOURFUNC macro defines a function with no prolog/epilog (naked)
*/

// the parameters and the return type of the declspace(naked) don't matter
#define DETOURFUNC(FuncName) _declspec(naked) void MyStub##FuncName() {	STUBMACRO(DTF_##FuncName) }

// these macros create the detour stubs for x86, e.g. MyStubMessageBoxA
DETOURFUNC(MessageBoxA);

DETOURFUNC(GetModuleHandleA);
DETOURFUNC(GetModuleFileNameA);
DETOURFUNC(RtlAllocateHeap);
DETOURFUNC(HeapReAlloc);
DETOURFUNC(RtlFreeHeap);

DETOURFUNC(NdrClientCall2);

// the detoured functions for Registry
DETOURFUNC(RegCreateKeyExW);
DETOURFUNC(RegOpenKeyExW);
DETOURFUNC(RegOpenKeyExA);

// detoured functions for Themed Scrollbar
DETOURFUNC(EnableScrollbar);

// detoured functions for ResponsiveNess
DETOURFUNC(PeekMessageA);

#else _WIN64
char* g_szArch = "x64 64 bit";
// these are the stubs for x64. (naked and inline asm not supported)
int WINAPI MyStubMessageBoxA(
	_In_opt_ HWND hWnd,
	_In_opt_ LPCSTR lpText,
	_In_opt_ LPCSTR lpCaption,
	_In_ UINT uType)
{
	auto redir = g_arrDetourTableEntry[DTF_MessageBoxA].GetMethod();
	return (reinterpret_cast<decltype(&MessageBoxA)>(redir))(hWnd, lpText, lpCaption, uType);
}

HMODULE WINAPI MyStubGetModuleHandleA(
	_In_opt_ LPCSTR lpModuleName)
{
	auto redir = g_arrDetourTableEntry[DTF_GetModuleHandleA].GetMethod();
	return (reinterpret_cast<decltype(&GetModuleHandleA)>(redir))(lpModuleName);
}

DWORD WINAPI MyStubGetModuleFileNameA(
	_In_opt_ HMODULE hModule,
	_Out_writes_to_(nSize, ((return < nSize) ? (return +1) : nSize)) LPSTR lpFilename,
	_In_ DWORD nSize
)
{
	auto redir = g_arrDetourTableEntry[DTF_GetModuleFileNameA].GetMethod();
	return (reinterpret_cast<decltype(&GetModuleFileNameA)>(redir))(hModule, lpFilename, nSize);
}

PVOID WINAPI MyStubRtlAllocateHeap(
	_In_opt_ HANDLE hHeapHandle,
	_In_ ULONG dwFlags,
	_In_ DWORD nSize
)
{
	auto redir = g_arrDetourTableEntry[DTF_RtlAllocateHeap].GetMethod();
	return (reinterpret_cast<pfnRtlAllocateHeap>(redir))(hHeapHandle, dwFlags, nSize);
}

PVOID WINAPI MyStubHeapReAlloc(
	HANDLE hHeap,
	DWORD dwFlags,
	LPVOID lpMem,
	SIZE_T dwBytes
)
{
	auto redir = g_arrDetourTableEntry[DTF_HeapReAlloc].GetMethod();
	return (reinterpret_cast<pfnHeapReAlloc>(redir))(hHeap, dwFlags, lpMem, dwBytes);
}

BOOL WINAPI MyStubRtlFreeHeap(
	HANDLE hHeap,
	DWORD dwFlags,
	LPVOID lpMem
)
{
	auto redir = g_arrDetourTableEntry[DTF_RtlFreeHeap].GetMethod();
	return (reinterpret_cast<pfnRtlFreeHeap>(redir))(hHeap, dwFlags, lpMem);
}

LSTATUS APIENTRY MyStubRegCreateKeyExW(
	__in        HKEY hKey,
	__in        LPCWSTR lpSubKey,
	__reserved  DWORD Reserved,
	__in_opt  LPWSTR lpClass,
	__in        DWORD dwOptions,
	__in        REGSAM samDesired,
	__in_opt  CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	__out       PHKEY phkResult,
	__out_opt LPDWORD lpdwDisposition
)
{
	auto redir = g_arrDetourTableEntry[DTF_RegCreateKeyExW].GetMethod();
	return (reinterpret_cast<decltype(&RegCreateKeyExW)>(redir))(hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpSecurityAttributes, phkResult, lpdwDisposition);
}

LSTATUS APIENTRY MyStubRegOpenKeyExW(
	__in HKEY hKey,
	__in_opt LPCWSTR lpSubKey,
	__in_opt DWORD dwOptions,
	__in REGSAM samDesired,
	__out PHKEY phkResult
)
{
	auto redir = g_arrDetourTableEntry[DTF_RegOpenKeyExW].GetMethod();
	return (reinterpret_cast<decltype(&RegOpenKeyExW)>(redir))(hKey, lpSubKey, dwOptions, samDesired, phkResult);
}

LSTATUS APIENTRY MyStubRegOpenKeyExA(
	__in HKEY hKey,
	__in_opt LPCSTR lpSubKey,
	__in_opt DWORD dwOptions,
	__in REGSAM samDesired,
	__out PHKEY phkResult
)
{
	auto redir = g_arrDetourTableEntry[DTF_RegOpenKeyExA].GetMethod();
	return (reinterpret_cast<decltype(&RegOpenKeyExA)>(redir))(hKey, lpSubKey, dwOptions, samDesired, phkResult);
}



BOOL WINAPI MyStubEnableScrollbar(
	_In_ HWND hWnd,
	_In_ UINT wSBflags,
	_In_ UINT wArrows)
{
	auto redir = g_arrDetourTableEntry[DTF_EnableScrollbar].GetMethod();
	return (reinterpret_cast<decltype(&EnableScrollBar)>(redir))(hWnd, wSBflags, wArrows);
}


BOOL
WINAPI
MyStubPeekMessageA(
	_Out_ LPMSG lpMsg,
	_In_opt_ HWND hWnd,
	_In_ UINT wMsgFilterMin,
	_In_ UINT wMsgFilterMax,
	_In_ UINT wRemoveMsg)
{
	auto redir = g_arrDetourTableEntry[DTF_PeekMessageA].GetMethod();
	return (reinterpret_cast<decltype(&PeekMessageA)>(redir))(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}


#endif _WIN64

// we want to use barebones critsect because we're very early in process
// because we're so early in the process, no other threads are busy, so unlikely necessary
CRITICAL_SECTION g_csRedirect;



//todo: perhaps export /lock/unlock, perhaps allow batch lock/unlock
// passing in pvNew == null will revert the redirection
// if HRESULT != S_OK, could be because we couldn't find the real function when initializing or the client passed in an invalid ID
CLINKAGE HRESULT EXPORT RedirectDetour(int DTF_Id, PVOID pvNew, PVOID * ppReal)
{
	HRESULT hr = E_FAIL;
	if (DTF_Id >= 0 && DTF_Id < DTF_MAX) // if the client passed in a legitimate value
	{
		if (g_arrDetourTableEntry[DTF_Id].RealFunction != nullptr) // if we successfully found the real function to detour
		{
			_ASSERT_EXPR(pvNew == nullptr || g_arrDetourTableEntry[DTF_Id].RedirectedFunction == nullptr, L"Redirecting a function already redirected");
			// we want to make sure all 4 (or 8) bytes of the address are changed atomically
			// because we're so early in the process, no other threads are busy, so unlikely necessary
#ifdef _WIN64
			InterlockedExchange64((LONGLONG*)&g_arrDetourTableEntry[DTF_Id].RedirectedFunction, (LONGLONG)pvNew);
#else
			InterlockedExchange((LONG*)&g_arrDetourTableEntry[DTF_Id].RedirectedFunction, (LONG)pvNew);
#endif

			if (ppReal != nullptr)
			{
				*ppReal = g_arrDetourTableEntry[DTF_Id].RealFunction;
			}
			hr = S_OK;
		}
	}
	_ASSERT_EXPR(hr == S_OK, L"redirect detour failed?");
	return hr;
};

CLINKAGE HRESULT EXPORT LockDetourTable()
{
	EnterCriticalSection(&g_csRedirect);
	return S_OK;
}

CLINKAGE HRESULT EXPORT UnlockDetourTable()
{
	LeaveCriticalSection(&g_csRedirect);
	return S_OK;
}


class SharedDetours
{
public:
	SharedDetours()
	{
		InitializeCriticalSection(&g_csRedirect);
		InstallTheDetours();
	}
	~SharedDetours()
	{
		UninstallDetours();
		DeleteCriticalSection(&g_csRedirect);
	}
private:
	void InstallTheDetours()
	{
		_ASSERT_EXPR(g_arrDetourTableEntry[DTF_MAX - 1].RealFunction == 0, L"table should have 0 detour real funcrtions");
		_ASSERT_EXPR(g_arrDetourTableEntry[DTF_MAX - 1].RedirectedFunction == 0, L"table should have 0 detour redirected");

		// This is required because on Win7 ole32 directly links to the Kernel32 implementations of these functions
		// and doesn't use Advapi
		HMODULE hmodNtDll = GetModuleHandleA("ntdll");
		OSVERSIONINFO osvi;
		osvi.dwOSVersionInfoSize = sizeof(osvi);
		// Fix for warnings as errors when building against WinBlue build 9444.0.130614-1739
		// warning C4996: 'GetVersionExW': was declared deprecated
		// externalapis\windows\winblue\sdk\inc\sysinfoapi.h(442)
		// Deprecated. Use VerifyVersionInfo* or IsWindows* macros from VersionHelpers.
#pragma warning( disable : 4996 )
		GetVersionEx(&osvi);
#pragma warning( default : 4996 )
		if ((osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 1) || osvi.dwMajorVersion > 6)
		{
			HMODULE hModKernel = GetModuleHandleW(L"kernelbase");

			// First check if the appropriate functions are in kernelbase or kernel32. We want the kernelbase ones
			// if we can help it (win8)
			if (GetProcAddress(hModKernel, "RegCreateKeyExA") == NULL)
			{
				hModKernel = GetModuleHandleW(L"kernel32");
				//CHECKERR(hMod == NULL);
			}
			g_arrDetourTableEntry[DTF_RegCreateKeyExW].RealFunction = &RegCreateKeyExA;
			g_arrDetourTableEntry[DTF_RegOpenKeyExW].RealFunction = &RegOpenKeyExW;

		}

		g_arrDetourTableEntry[DTF_MessageBoxA].RealFunction = &MessageBoxA;
		g_arrDetourTableEntry[DTF_GetModuleHandleA].RealFunction = &GetModuleHandleA;
		g_arrDetourTableEntry[DTF_GetModuleFileNameA].RealFunction = &GetModuleFileNameA;

		g_arrDetourTableEntry[DTF_RtlAllocateHeap].RealFunction = (pfnRtlAllocateHeap)GetProcAddress(hmodNtDll, "RtlAllocateHeap");
		g_arrDetourTableEntry[DTF_HeapReAlloc].RealFunction = HeapReAlloc;// (pfnRtlFreeHeap)GetProcAddress(hmodNtDll, "HeapReAlloc");
		g_arrDetourTableEntry[DTF_RtlFreeHeap].RealFunction = (pfnRtlFreeHeap)GetProcAddress(hmodNtDll, "RtlFreeHeap");

		HMODULE hComBase = GetModuleHandleW(L"rpcrt4.dll");
		// NdrClientCall2 is not a WinAPI so must use _declspec(naked)
		g_arrDetourTableEntry[DTF_NdrClientCall2].RealFunction = (PVOID)GetProcAddress(hComBase, "NdrClientCall2");


		g_arrDetourTableEntry[DTF_EnableScrollbar].RealFunction = &EnableScrollBar;


		g_arrDetourTableEntry[DTF_PeekMessageA].RealFunction = &PeekMessageA;

#ifndef _WIN64
		// the system region of memory changes for various architectures 
		// Address Space Layout Randomization causes Dlls to be loaded at varying addresses
		// we want to tell Detours that on this architecture what the range of system dlls is so 
		// the VirtualAlloc it uses doesn't coincide with e.g. MSCorLib, Rebase costs when collisions occur
		// see bug https://devdiv.visualstudio.com/DevDiv/_workitems/edit/280248

		DetourSetSystemRegionLowerBound((PVOID)0x50000000);
		DetourSetSystemRegionUpperBound((PVOID)0x80000000);
#endif _WIN64

		DetourTransactionBegin();
#define ATTACH(x,y)   DetAttach(x, y,(PCHAR)#x)
#define DETACH(x,y)   DetDetach(x, y,(PCHAR)#x)


		ATTACH(&g_arrDetourTableEntry[DTF_MessageBoxA].RealFunction, MyStubMessageBoxA);
		ATTACH(&g_arrDetourTableEntry[DTF_GetModuleHandleA].RealFunction, MyStubGetModuleHandleA);
		ATTACH(&g_arrDetourTableEntry[DTF_GetModuleFileNameA].RealFunction, MyStubGetModuleFileNameA);
		ATTACH(&g_arrDetourTableEntry[DTF_RtlAllocateHeap].RealFunction, MyStubRtlAllocateHeap);
		ATTACH(&g_arrDetourTableEntry[DTF_RtlFreeHeap].RealFunction, MyStubRtlFreeHeap);
		ATTACH(&g_arrDetourTableEntry[DTF_HeapReAlloc].RealFunction, MyStubHeapReAlloc);

#ifndef _WIN64
//		ATTACH(&g_arrDetourTableEntry[DTF_NdrClientCall2].RealFunction, MyStubNdrClientCall2);
#endif _WIN64


		ATTACH(&g_arrDetourTableEntry[DTF_RegCreateKeyExW].RealFunction, MyStubRegCreateKeyExW);
		ATTACH(&g_arrDetourTableEntry[DTF_RegOpenKeyExW].RealFunction, MyStubRegOpenKeyExW);

		ATTACH(&g_arrDetourTableEntry[DTF_EnableScrollbar].RealFunction, MyStubEnableScrollbar);

		ATTACH(&g_arrDetourTableEntry[DTF_PeekMessageA].RealFunction, MyStubPeekMessageA);

		auto res = DetourTransactionCommit();
		_ASSERT_EXPR(res == S_OK, "Failed to TransactionCommit install");
	}

	void UninstallDetours()
	{
		DetourTransactionBegin();
		DETACH(&g_arrDetourTableEntry[DTF_MessageBoxA].RealFunction, MyStubMessageBoxA);
		DETACH(&g_arrDetourTableEntry[DTF_GetModuleHandleA].RealFunction, MyStubGetModuleHandleA);
		DETACH(&g_arrDetourTableEntry[DTF_GetModuleFileNameA].RealFunction, MyStubGetModuleFileNameA);
		DETACH(&g_arrDetourTableEntry[DTF_RtlAllocateHeap].RealFunction, MyStubRtlAllocateHeap);
		DETACH(&g_arrDetourTableEntry[DTF_HeapReAlloc].RealFunction, MyStubHeapReAlloc);
		DETACH(&g_arrDetourTableEntry[DTF_RtlFreeHeap].RealFunction, MyStubRtlFreeHeap);

#ifndef _WIN64
//		DETACH(&g_arrDetourTableEntry[DTF_NdrClientCall2].RealFunction, MyStubNdrClientCall2);
#endif _WIN64


		DETACH(&g_arrDetourTableEntry[DTF_RegCreateKeyExW].RealFunction, MyStubRegCreateKeyExW);
		DETACH(&g_arrDetourTableEntry[DTF_RegOpenKeyExW].RealFunction, MyStubRegOpenKeyExW);

		DETACH(&g_arrDetourTableEntry[DTF_EnableScrollbar].RealFunction, MyStubEnableScrollbar);

		DETACH(&g_arrDetourTableEntry[DTF_PeekMessageA].RealFunction, MyStubPeekMessageA);


		auto res = DetourTransactionCommit();
		_ASSERT_EXPR(res == S_OK, "Failed to TransactionCommit uninstall");
	}

	VOID DetAttach(PVOID* ppbReal, PVOID pbMine, PCHAR psz)
	{
		LONG res = DetourAttach(ppbReal, pbMine);
		if (res != S_OK) {
			WCHAR szBuf[1024];
			swprintf_s(szBuf, _countof(szBuf), L"Attach detour Failed %S Errcode=%d)", psz, res);
			_ASSERT_EXPR(false, szBuf);
		}
	}

	VOID DetDetach(PVOID* ppbReal, PVOID pbMine, PCHAR psz)
	{
		LONG res = DetourDetach(ppbReal, pbMine);
		if (res != S_OK) {
			WCHAR szBuf[1024];
			swprintf_s(szBuf, _countof(szBuf), L"Detach Failed %S Errcode=%d", psz, res);
			_ASSERT_EXPR(false, szBuf);
		}
	}

};

CLINKAGE HRESULT EXPORT StartDetouring(PVOID* pDetours)
{
//	SharedDetours xx;
	auto xx = new SharedDetours();
	*pDetours = xx;
	return S_OK;
}
#pragma comment(linker, "/EXPORT:StartDetouring=_StartDetouring@4")

CLINKAGE HRESULT EXPORT StopDetouring(PVOID oDetours)
{
	SharedDetours* xx = (SharedDetours*)oDetours;
	delete xx;
	return S_OK;
}
#pragma comment(linker, "/EXPORT:StopDetouring=_StopDetouring@4")

