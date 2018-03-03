#pragma once
#include <Windows.h>

#define CLINKAGE extern "C"
#define EXPORT __declspec(dllexport) __stdcall


/*
We want to detour all the functions that we detour in the process as early as possible. The longer we wait,
the more likely there are more threads, some of which may be calling the detours.
If the detour is partially implemented, (e.g. the address or trampoline is partially completed on one thread) another thread calling the function 
will call random code, leading to failure. So we have to create a toolhelp snapshot to enumerate the threads
and suspend them. (this in itself is a race condition: a thread can be created while enumerating the threads)
We also have to HeapLock, because the Detours require heap allocation, and if a thread is suspended while doing a heap alloc, deadlock occurs.
Also, doing all the Detours from a singlee module means the detours can share the same VirtualAlloc'd 64K memory block, rather
than one 64k block for each module that detours.

VS detours in 3 different places: the sequence before this change is:
	1. detour in devenv for privateregistry settings
	2. detour in msenv for themed scrollbars
	3. detour in vslog for responsiveness monitoring
The CLR starts in between, causing lots of threads to be created, doing memory allocations, reading registry, etc.


Instructions to add a new detour:
1. Add an Enum (like DTF_MessageBoxA) and prototype (like pfnMessageBoxA). Indicate where the redirect code is in a comment e.g. ThemedScroolBar(msenv) or VSResponsiveness(VsLog)
(note: multiple different modules might need to detour the same function: if one client doesn't need it any more, it might remove it not realizing somebody else needs it)
	typedef int (WINAPI *pfnMessageBoxA)(
		_In_opt_ HWND hWnd,
		_In_opt_ LPCSTR lpText,
		_In_opt_ LPCSTR lpCaption,
		_In_ UINT uType);

2. Add a stub function (for both 64 and 32 bit) which is the version that would be called if any code calls the function before your module even gets loaded, so it just forwards to the real version
	For 32 bit: (a macro does it for you, using a Declspec naked so parameteres don't have to be pushed/popped multiple times..)
		DETOURFUNC(MessageBoxA)

	For 64 bit:
	int WINAPI MyStubMessageBoxA(
		_In_opt_ HWND hWnd,
		_In_opt_ LPCSTR lpText,
		_In_opt_ LPCSTR lpCaption,
		_In_ UINT uType)
		{
			auto redir = g_arrDetourTableEntry[DTF_MessageBoxA].GetMethod();
			return (reinterpret_cast<pfnMessageBoxA>(redir))(hWnd, lpText, lpCaption, uType);
		}

3. Get the real address of the function and put it in table (you might have to call LoadLibraryGetModuelHandle/GetProcAddress to get the module loaded first.
(loading it earlier will change DLL load order, but because we're very early in the process, perf tests won't trip on it.). Currently, all modules detoured are already loaded (kernelbase, combase, user32)

	g_arrDetourTableEntry[DTF_MessageBoxA].RealFunction = &MessageBoxA;


4. Add a DetourAttach line (gets executed very early: (and DetourDetach)

	DetourAttach((PVOID *)&g_arrDetourTableEntry[DTF_MessageBoxA].RealFunction, MyStubMessageBoxA);

5. When your code loads, call RedirectDetour, to redirect the stub to call your version of the detoured function
(and get a pointer to the Real Function, so your version can call the real one



The code does a detour of all functions to the stubs very early in the process. The stubs just call the Real functions.
By the time your code gets loaded and you want to detour, call RedirecDetour to redirect from the stub to your implementation
At this point there is no thread suspension necessary. (we still have to lock, but just for the address pointer for the redirect)
This architecture lends itself to:
-- hooks: client code called before/after the real function
-- chaining

todo: obfuscate for security (xor DTF with pid?)

see bug https://devdiv.visualstudio.com/DevDiv/_workitems/edit/280248

*/

typedef HRESULT(WINAPI *pfnRedirectDetour)(int DTF_Id, PVOID pvNew, PVOID *ppReal);

CLINKAGE HRESULT EXPORT RedirectDetour(int DTF_Id, PVOID pvNew, PVOID *ppReal);


CLINKAGE HRESULT EXPORT LockDetourTable();
CLINKAGE HRESULT EXPORT UnlockDetourTable();


#ifdef _WIN64
#define STARTVISUALSTUDIO "StartVisualStudio"
#define REDIRECTDETOUR "RedirectDetour"
#else
#define STARTVISUALSTUDIO "_StartVisualStudio@0"
#define REDIRECTDETOUR "_RedirectDetour@12"
#endif

/*Start of typedefs for functions that are detoured*/

typedef int (WINAPI *pfnMessageBoxA)(
	_In_opt_ HWND hWnd,
	_In_opt_ LPCSTR lpText,
	_In_opt_ LPCSTR lpCaption,
	_In_ UINT uType);

typedef HMODULE(WINAPI *pfnGetModuleHandle)(_In_opt_ LPCSTR lpModuleName);


typedef DWORD(WINAPI *pfnGetModuleFileNameA)(
	_In_opt_ HMODULE hModule,
	_Out_writes_to_(nSize, ((return < nSize) ? (return +1) : nSize)) LPSTR lpFilename,
	_In_ DWORD nSize
	);

typedef PVOID(WINAPI *pfnRtlAllocateHeap)(
	HANDLE hHeapHandle,
	ULONG dwFlags,
	SIZE_T size
	);

typedef BOOL(WINAPI *pfnRtlFreeHeap)(
	HANDLE hHeap,
	DWORD dwFlags,
	LPVOID lpMem
	);


typedef LSTATUS(APIENTRY *pfnRegCreateKeyExW)(
	__in        HKEY hKey,
	__in        LPCWSTR lpSubKey,
	__reserved  DWORD Reserved,
	__in_opt  LPWSTR lpClass,
	__in        DWORD dwOptions,
	__in        REGSAM samDesired,
	__in_opt  CONST LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	__out       PHKEY phkResult,
	__out_opt LPDWORD lpdwDisposition
	);


typedef LSTATUS(APIENTRY *pfnRegOpenKeyExW)(
	__in HKEY hKey,
	__in_opt LPCWSTR lpSubKey,
	__in_opt DWORD dwOptions,
	__in REGSAM samDesired,
	__out PHKEY phkResult
	);

typedef LSTATUS(APIENTRY *pfnRegOpenKeyExA)(
	__in HKEY hKey,
	__in_opt LPCTSTR lpSubKey,
	__in_opt DWORD dwOptions,
	__in REGSAM samDesired,
	__out PHKEY phkResult
	);



typedef BOOL (WINAPI *pfnEnableScrollBar)(
	_In_ HWND hWnd,
	_In_ UINT wSBflags,
	_In_ UINT wArrows);






typedef BOOL (WINAPI *pfnPeekMessageA)(
	_Out_ LPMSG lpMsg,
	_In_opt_ HWND hWnd,
	_In_ UINT wMsgFilterMin,
	_In_ UINT wMsgFilterMax,
	_In_ UINT wRemoveMsg);





/*end of typedefs for functions that are detoured*/



typedef enum {
	DTF_GetModuleHandleA,   /*sample*/
	DTF_GetModuleFileNameA,	/*sample*/
	DTF_MessageBoxA,		/*sample*/
	DTF_RtlAllocateHeap,    /*sample*/
	DTF_RtlFreeHeap,        /*sample*/

	DTF_RegCreateKeyExW,	/*vscommon\registrydetouring\vsdetour.cpp*/
	DTF_RegOpenKeyExW,		/*vscommon\registrydetouring\vsdetour.cpp*/
	DTF_RegOpenKeyExA,		/*vscommon\registrydetouring\vsdetour.cpp*/

	DTF_EnableScrollbar,    /*env\msenv\core\ThemedScrollbar.cpp */

	DTF_PeekMessageA,       /*vscommon\testtools\vslog\ResponseTime\VSResponsiveness.cpp */


	DTF_MAX
} tagDetouredFunctions;



