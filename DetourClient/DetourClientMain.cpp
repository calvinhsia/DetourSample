#include <Windows.h>
#include "..\DetourSharedBase\DetourShared.h"
#include "crtdbg.h"
#include "stdio.h"

#include <atlbase.h>
#include <atlcom.h>

#include "DetourClientMain.h"


using namespace std;


int g_tlsIndex;
pfnRtlAllocateHeap Real_RtlAllocateHeap;
pfnHeapReAlloc Real_HeapReAlloc;
pfnRtlFreeHeap Real_RtlFreeHeap;


DWORD g_dwMainThread;

CComAutoCriticalSection g_csHeap;



void CreateComObject();

// must define this for 64 bit:
#ifdef _WIN64
#define USETLSINDEX 1
#else
#endif _WIN64

#if !USETLSINDEX

#define ISRECUR tlRecursionHeapAlloc == true
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

*/

#define TLSNotRecurring (PVOID)2 
#define TLSIsInitialized (TlsGetValue(g_tlsIndex) != 0)
#define TLSISRecur 1
#define ISRECUR (TlsGetValue(g_tlsIndex) == (PVOID)TLSISRecur)
#define SETRECUR TlsSetValue(g_tlsIndex,(PVOID)TLSISRecur);
#define SETRECURDONE TlsSetValue(g_tlsIndex,TLSNotRecurring);


#endif

void foobar()
{
    SIZE_T sizeAlloc = 72;
    auto it = find_if(g_heapAllocSizes.begin(), g_heapAllocSizes.end(), [sizeAlloc](HeapSizeData data)
    {
        if (data._nSize == sizeAlloc)
        {
            return true;
        }
        return false;
    });
    GetModuleFileName((HMODULE)(*it)._nSize, nullptr, NULL);

}

bool MyPred(SIZE_T size, HeapSizeData data)
{
    return data._nSize == size;
}

extern "C" DWORD _tls_index;
PVOID WINAPI MyRtlAllocateHeap(HANDLE hHeap, ULONG dwFlags, SIZE_T size)
{
    bool fDidCollectStack = false;
    PVOID pMem = nullptr;

    //    if (size > g_HeapAllocSizeMinValue)
    {
#if !USETLSINDEX
        //thread_local
        //__declspec(thread)
        static bool tlRecursionHeapAlloc = false;
#endif
        {
            //			CComCritSecLock<CComAutoCriticalSection> lock(g_csHeap);
        }


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
#ifndef _WIN64

        _asm mov eax, _tls_index;
        _asm mov ecx, fs:[2ch]
            _asm cmp ecx, 0
        _asm je bailout
#else
#if !USETLSINDEX
        //#Error: can't use thread_local to detour heapalloc in 64 bit
#endif !USETLSINDEX

#endif _WIN64

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
            pMem = Real_RtlAllocateHeap(hHeap, dwFlags, size);
            if (size < g_HeapAllocSizeMinValue)
            {
                //                auto it = find_if(g_heapAllocSizes.begin(), g_heapAllocSizes.end(), [size](HeapSizeData data) {return MyPred(size, data); });
                //HeapSizeData *pdata=nullptr;
                //for (auto item : g_heapAllocSizes)
                //{
                //    if (item._nSize == size)
                //    {
                //        pdata = &item;
                //        break;
                //    }
                //}
                //for_each(g_heapAllocSizes.begin(), g_heapAllocSizes.end(), [size](HeapSizeData data) {
                //    if (data._nSize == size)
                //    {

                //    }
                //});
                auto it = find_if(g_heapAllocSizes.begin(), g_heapAllocSizes.end(), [size](HeapSizeData data)
                {
                    if (data._nSize == size)
                    {
                        return true;
                    }
                    return false;
                });
                if (it != g_heapAllocSizes.end())
                {
                    if ((*it)._nThreshold == 0)
                    {
                        fDidCollectStack = CollectStack(StackTypeHeapAlloc,/*stackSubType*/ (DWORD)size, /*extra data*/NULL, /*numFramesToSkip*/2);
                        if (fDidCollectStack)
                        {
                            InterlockedIncrement(&(*it)._nStacksCollected);
                        }
                    }
                    else
                    {
                        InterlockedDecrement(&(*it)._nThreshold);
                    }
                }
            }
            else
            {
                fDidCollectStack = CollectStack(StackTypeHeapAlloc,/*stackSubType*/ (DWORD)size, /*extra data*/NULL, /*numFramesToSkip*/1);
            }

            SETRECURDONE;
        }
        else
        {
#ifndef _WIN64
            _asm bailout:
#else
#endif _WIN64            
            pMem = Real_RtlAllocateHeap(hHeap, dwFlags, size);
        }
    }
    return pMem;
}


#ifndef _WIN64

_declspec (thread) // note: when this code runs in VS, random failures when initializing the std::stack in a TLS: the stack would be empty even when the prior line says push(). workaround: make the TLS point to a struct containing the stl::stack
std::stack<std::pair<LPVOID, DWORD>> _stackRetAddresses; // return address, TickCount

void _stdcall pushvalue(LPVOID val)
{
    _stackRetAddresses.push(pair<LPVOID, DWORD>(val, GetTickCount()));
}

LPVOID _stdcall popvalue()
{
    auto top = _stackRetAddresses.top();
    auto retAddr = top.first;
    auto elapsed = GetTickCount() - top.second;
    _stackRetAddresses.pop();
    DWORD stackSubType = 0;
    if (GetCurrentThreadId() == g_dwMainThread)
    {
        stackSubType = 0;
    }
    else
    {
        stackSubType = 1;
    }
    CollectStack(StackTypeRpc, stackSubType, elapsed, /*numFramesToSkip*/3);
    return retAddr;
}

PVOID Real_NdrClientCall2;
_declspec(naked) void DetourNdrClientCall2()
{
    /*
    detouring NdrClientCall2 is a little complicated because it's CDECL. Wrapping (running code before/after the call to get elapsed time) is thus more complex
    https://msdn.microsoft.com/en-us/library/windows/desktop/aa374215(v=vs.85).aspx
    Network Data Representation (NDR) Engine
    A caller looks like:

    CLIENT_CALL_RETURN RPC_VAR_ENTRY
    NdrClientCall4(
    PMIDL_STUB_DESC     pStubDescriptor,
    PFORMAT_STRING      pFormat,
    ...
    )
    {
    RPCP_PAGED_CODE();

    va_list                     ArgList;

    //
    // On x86 we pass in a pointer to the first parameter, and expect that
    // the rest of the parameters are directly behind it on the stack. The
    // only way this silly assumption is true is if we disable optimization
    // in the stub. We will switch to passing in all the parameters the same
    // as in AMD64 and ARM. Due to back compat, we can't change NdrClientCall2,
    // so this is a copy of the AMD/ARM version of NdrClientCall2 only called
    // on x86 running Win8.1 or newer.
    //
    INIT_ARG( ArgList, pFormat);
    uchar *StartofStack = (uchar*)GET_STACK_START(ArgList);

    return NdrClientCall2( pStubDescriptor, pFormat, StartofStack );
    }


    The asm looks like:
    lea         eax,[ebp+10h]
    push        eax
    push        dword ptr [ebp+0Ch]
    push        dword ptr [ebp+8]
    call        NdrClientCall2 (76C57590h)
    add         esp,0Ch

    this is the prototype:
    CLIENT_CALL_RETURN RPC_VAR_ENTRY
    NdrClientCall2(
    PMIDL_STUB_DESC     pStubDescriptor,
    PFORMAT_STRING      pFormat,
    ...
    );
    Upon entry here, ESP points to the caller return address (where the caller cleans the stack by adding 4 * the # of parms it pushed to ESP)
    */
    _asm call pushvalue // call psuhvalue (which expects 1 param, but we don't push one, so it uses the curvalue on the stack which is the ret addr) to put return addr in our threadlocal std::stack
    _asm call Real_NdrClientCall2   // call the real code (it's cdecl with variable # parms)

    _asm push eax       // We're just Pushing here to decrement the stack by 4 to save space to put the orig return addr. This slot will be replaced by the orig ret addr
    _asm push eax       // save real function's return value
    _asm call popvalue  // get the original return address in eax
    _asm mov[esp + 4], eax       // put the original ret addr on stack where the "ret" below will return correctly

    _asm pop eax      // restore the real function's return value
    _asm ret          // ret to caller
}
#endif _WIN64

PVOID WINAPI MyHeapReAlloc( // no re-new
    HANDLE hHeap,
    DWORD dwFlags,
    LPVOID lpMem,
    SIZE_T dwBytes
)
{
    bool IsTracked = false;
    PVOID pNewMem;

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


class myclass
{
public:
    myclass()
    {
        _dwThreadId = GetCurrentThreadId();
        _pMem = malloc(100);
    }
    DWORD _dwThreadId;
    LPVOID _pMem;
public:
    ~myclass()
    {
        delete _pMem;
    }
};

thread_local myclass g_myclass;


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

        res = fnRedirectDetour(DTF_HeapReAlloc, MyHeapReAlloc, (PVOID *)&Real_HeapReAlloc);
        _ASSERT_EXPR(res == S_OK, L"Redirecting detour to heapReAlloc");


        res = fnRedirectDetour(DTF_RtlFreeHeap, MyRtlFreeHeap, (PVOID *)&Real_RtlFreeHeap);
        _ASSERT_EXPR(res == S_OK, L"Redirecting detour to free heap");

#ifndef _WIN64
        res = fnRedirectDetour(DTF_NdrClientCall2, DetourNdrClientCall2, (PVOID*)&Real_NdrClientCall2);
        _ASSERT_EXPR(res == S_OK, L"Redirecting detour to MyNdrClientCall2");
#endif _WIN64

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
        if (fnRedirectDetour(DTF_HeapReAlloc, nullptr, nullptr) != S_OK)
        {
            _ASSERT_EXPR(false, L"Failed to redirect detour");
        }
        if (fnRedirectDetour(DTF_RtlFreeHeap, nullptr, nullptr) != S_OK)
        {
            _ASSERT_EXPR(false, L"Failed to redirect detour");
        }
        if (fnRedirectDetour(DTF_NdrClientCall2, nullptr, nullptr) != S_OK)
        {
            _ASSERT_EXPR(false, L"Failed to redirect detour");
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
		CollectStack(StackTypeHeapAlloc, 1, 0, 2);
	}
}


CLINKAGE void EXPORT StartVisualStudio()
{
    g_dwMainThread = GetCurrentThreadId();

    splitStr(g_strHeapAllocSizesToCollect, ',', [](wstring valPair)
    {
        auto ndxColon = valPair.find(':');
        auto heapSize = _wtol(valPair.substr(0, ndxColon).c_str());
        auto heapThresh = _wtol(valPair.substr(ndxColon + 1).c_str());
        g_heapAllocSizes.push_back(HeapSizeData(heapSize, heapThresh));

    });

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



    InitCollectStacks();

	RecurDownSomeLevels(200);

    HookInMyOwnVersion(true);




    auto h = GetModuleHandleA(0);
    string buff(MAX_PATH, '\0');

    GetModuleFileNameA(0, &buff[0], (DWORD)buff.length());
    //buff.resize(strlen(&buff[0]));

    MessageBoxA(0, buff.c_str(), "Calling the WinApi version of MessageboxA", 0);
    for (int i = 0; i < 10000; i++)
    {
        void *p = HeapAlloc(GetProcessHeap(), 0, 1000 + i);
        //    HeapFree(GetProcessHeap(), 0, p);
    }

    DoSomeThreadingModelExperiments();

    CreateComObject();

    DoSomeManagedCode();


    // now undo detour redirection, so detouring to stubs:
    HookInMyOwnVersion(false);

    GetModuleFileNameA(0, &buff[0], (DWORD)buff.length());

    LONGLONG nStacksCollected = GetNumStacksCollected();
    sprintf_s(&buff[0], buff.length(), "Detours unhooked, calling WinApi MessageboxA # allocs = %d   AllocSize = %lld  Stks Collected=%lld    StackSpaceUsed=%d", g_MyStlAllocStats._nTotNumHeapAllocs, g_MyStlAllocStats._TotNumBytesHeapAlloc, nStacksCollected, g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc);

    UninitCollectStacks();

    MessageBoxA(0, &buff[0], "Calling the WinApi version of MessageboxA", 0);
    _ASSERT_EXPR(g_MyStlAllocStats._nTotNumHeapAllocs > 400 && g_MyStlAllocStats._TotNumBytesHeapAlloc > 3000, L"#expected > 400 allocations of >3000 bytes");
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

        break; 

    case DLL_THREAD_ATTACH:
        break;

    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }

    return retval;
}
