
#include "..\DetourSharedBase\DetourShared.h"
#include "vector"
#include "unordered_map"
#include "atlbase.h"
#include "DetourClientMain.h"

using namespace std;

extern pfnRtlAllocateHeap Real_RtlAllocateHeap;

CComAutoCriticalSection g_critSectHeapAlloc;
int g_nTotalAllocs;
LONGLONG g_TotalAllocSize;

LONG g_MyStlAllocTotalAlloc = 0;
LONG g_MyStlAllocLimit = 65536 * 20;
int g_NumFramesTocapture = 20;
bool g_fReachedMemLimit = false;

#define USEMYSTLALLOC 
//#define MYSTLALLOCSTATICHEAP
//#define LIMITSTACKMEMORY

#ifdef MYSTLALLOCSTATICHEAP
static HANDLE m_hHeap = GetProcessHeap();
#endif MYSTLALLOCSTATICHEAP

template <class T>
struct MySTLAlloc // https://blogs.msdn.microsoft.com/calvin_hsia/2010/03/16/use-a-custom-allocator-for-your-stl-container/
{
    typedef T value_type;
    MySTLAlloc()
    {
#ifndef MYSTLALLOCSTATICHEAP
        m_hHeap = GetProcessHeap();
#endif MYSTLALLOCSTATICHEAP
    }
    // A converting copy constructor:
    template<class U> MySTLAlloc(const MySTLAlloc<U>& other)
    {
#ifndef MYSTLALLOCSTATICHEAP
        m_hHeap = other.m_hHeap;
#endif MYSTLALLOCSTATICHEAP
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
#ifdef LIMITSTACKMEMORY

        if (g_MyStlAllocTotalAlloc + (int)nSize >= g_MyStlAllocLimit)
        {
            throw std::bad_alloc();
        }
#endif LIMITSTACKMEMORY
        InterlockedAdd(&g_MyStlAllocTotalAlloc, nSize);
        void *pv;
        if (Real_RtlAllocateHeap == nullptr)
        {
            pv = HeapAlloc(m_hHeap, 0, nSize);
        }
        else
        {
            pv = Real_RtlAllocateHeap(m_hHeap, 0, nSize);
        }
        if (pv == 0)
        {
            _ASSERT_EXPR(false, L"MyStlAlloc failed to allocate:");// out of memmory allocating %d(%x).\n Try reducing stack size limit.For 32 bit proc, try http://blogs.msdn.com/b/calvin_hsia/archive/2010/09/27/10068359.aspx ", nSize, nSize));
        }
        return static_cast<T*>(pv);
    }
    void deallocate(T* const p, size_t size) const
    {
        InterlockedAdd(&g_MyStlAllocTotalAlloc, -((int)size));
        HeapFree(m_hHeap, 0, p);
    }
#ifndef MYSTLALLOCSTATICHEAP
    HANDLE m_hHeap; // a heap to use to allocate our stuff. If 0, use VSAssert private debug heap
#endif MYSTLALLOCSTATICHEAP
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
#ifdef USEMYSTLALLOC
    , MySTLAlloc<PVOID>
#endif USEMYSTLALLOC
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

typedef unordered_map<UINT, CallStack
#ifdef USEMYSTLALLOC
    ,
    hash<UINT>,
    equal_to<UINT>,
    MySTLAlloc<pair<const UINT, CallStack> >
#endif USEMYSTLALLOC
> mapStackHashToStack; // stackhash=>CallStack

// represents the stacks for a particular stack type : e.g. the 100k allocations
// if the stacks are identical, the count is bumped.
struct StacksForStackType
{
    StacksForStackType(CallStack stack)
    {
        AddNewStack(stack);
    }
    void AddNewStack(CallStack stack)
    {
        auto res = _stacks.find(stack.stackHash);
        if (res == _stacks.end())
        {
            _stacks.insert(pair<UINT, CallStack>(stack.stackHash, stack));
        }
        else
        {
            res->second.cnt++;
        }
    }
    LONGLONG GetTotalNumStacks()
    {
        auto tot = 0l;
        for (auto stack : _stacks)
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
#ifdef USEMYSTLALLOC
    ,
    hash<mapKey>,
    equal_to<mapKey>,
    MySTLAlloc<pair<const mapKey, StacksForStackType>  >
#endif USEMYSTLALLOC
> mapStacksByStackType;

// map the Size of an alloc to all the stacks that allocated that size.
// note: if we're looking for all allocs of a specific size (e.g. 1Mb), then no need for a map by size (because all keys will be the same): more efficient to just use a mapStacks
mapStacksByStackType g_mapStacksByStackType;


LONGLONG GetNumStacksCollected()
{
    LONGLONG nTotCnt = 0;
    LONGLONG nTotSize = 0;
    int nUniqueStacks = 0;
    int nFrames = 0;
    LONGLONG nRpcStacks[2] = {0};
    g_fReachedMemLimit = false;
    g_MyStlAllocLimit *= 2; // double mem used: we're done with detouring
    auto save_g_MyStlAllocTotalAlloc = g_MyStlAllocTotalAlloc;
    for (auto entry : g_mapStacksByStackType)
    {
        auto key = entry.first;
        switch (key.first)
        {
        case StackTypeHeapAlloc:
        {
            auto stackBySize = entry.second;
            auto sizeAlloc = key.second;
            auto cnt = stackBySize.GetTotalNumStacks(); // to see the output, use a tracepoint (breakpoint action): output: sizeAlloc={sizeAlloc} cnt={cnt}
            nTotSize += sizeAlloc * cnt;
            nTotCnt += cnt;
            for (auto stk : entry.second._stacks)
            {
                nUniqueStacks++;
                for (auto frm : stk.second.vecFrames)
                {
                    nFrames++;
                    auto f = frm;  // output {frm}
                }
            }
        }
        break;
        case StackTypeRpc:
            nRpcStacks[key.second] += entry.second.GetTotalNumStacks();
            break;
        default:
            break;
        }
    }
    g_MyStlAllocTotalAlloc = save_g_MyStlAllocTotalAlloc;
    /*
    Sample output from OutputWindow:
    sizeAlloc=72 cnt=25
    0x0f7df441 {DetourClient.dll!MyRtlAllocateHeap(void * hHeap, unsigned long dwFlags, unsigned long size), Line 35}
    0x752f67d0 {KernelBase.dll!LocalAlloc(unsigned int uFlags, unsigned long uBytes), Line 96}
    0x73cf4245 {msctf.dll!CVoidStructArray::Insert(int iIndex, int cElems), Line 67}
    0x73cf419f {msctf.dll!CVoidStructArray::Append(int cElems), Line 61}
    0x73d066b6 {msctf.dll!CStructArray<TF_INPUTPROCESSORPROFILE>::Clone(void), Line 138}
    0x73cf7163 {msctf.dll!CThreadInputMgr::_CleanupContexts(int fSync, CStructArray<TF_INPUTPROCESSORPROFILE> * pProfiles), Line 278}
    0x73cdaa75 {msctf.dll!CThreadInputMgr::DeactivateTIPs(void), Line 1252}
    0x7428e8a6 {user32.dll!CtfHookProcWorker(int dw, unsigned int wParam, long lParam, unsigned long xParam), Line 2986}
    0x7428fb88 {user32.dll!CallHookWithSEH(_GENERICHOOKHEADER * pmsg, void * pData, unsigned long * pFlags, unsigned long), Line 78}
    0x7424bcb6 {user32.dll!__fnHkINDWORD(_FNHKINDWORDMSG * pmsg), Line 5307}
    0x76f60bcd {ntdll.dll!_KiUserCallbackDispatcher@12(void), Line 517}
    0x7426655a {user32.dll!InternalDialogBox(void * hModule, DLGTEMPLATE * lpdt, HWND__ * hwndOwner, int(__stdcall*)(HWND__ *, unsigned int, unsigned int, long) pfnDialog, long lParam, unsigned int fSCDLGFlags), Line 1836}
    0x742a043b {user32.dll!SoftModalMessageBox(_MSGBOXDATA * lpmb), Line 1305}
    0x74282093 {user32.dll!MessageBoxWorker(_MSGBOXDATA * pMsgBoxParams), Line 840}
    0x7429fcb5 {user32.dll!MessageBoxTimeoutW(HWND__ * hwndOwner, const wchar_t * lpszText, const wchar_t * lpszCaption, unsigned int wStyle, unsigned short wLanguageId, unsigned long dwTimeout), Line 495}
    0x7429fb1b {user32.dll!MessageBoxTimeoutA(HWND__ * hwndOwner, const char * lpszText, const char * lpszCaption, unsigned int wStyle, unsigned short wLanguageId, unsigned long dwTimeout), Line 539}
    0x7429f8ca {user32.dll!MessageBoxA(HWND__ * hwndOwner, const char * lpszText, const char * lpszCaption, unsigned int wStyle), Line 398}
    0x0f7df39d {DetourClient.dll!MyMessageBoxA(HWND__ * hWnd, const char * lpText, const char * lpCaption, unsigned int uType), Line 52}
    0x0f7df5a1 {DetourClient.dll!StartVisualStudio(void), Line 130}
    0x00f0a81f {DetourSharedBase.exe!wWinMain(HINSTANCE__ * hInstance, HINSTANCE__ * hPrevInstance, wchar_t * lpCmdLine, int nCmdShow), Line 377}

    */

    _ASSERT_EXPR(g_nTotalAllocs == nTotCnt, L"Total # allocs shouuld match");
    _ASSERT_EXPR(g_TotalAllocSize == nTotSize, L"Total size allocs should match");
    return nTotCnt;
}

// extraInfo can be e.g. elapsed ticks. Don't store in stacks, but raise ETW event with it
bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraInfo, int numFramesToSkip)
{
    bool fDidCollectStack = false;
    if (!g_fReachedMemLimit)
    {
        try
        {
            if (extraInfo > 1)
            {
                auto x = 2;
            }

            CComCritSecLock<CComAutoCriticalSection> lock(g_critSectHeapAlloc);
#ifdef LIMITSTACKMEMORY
            // try limiting to a fixed amount of mem. We could use VirtualAlloc for a 64k block (or multiple of 64)
            if (g_MyStlAllocTotalAlloc + 10 * (g_NumFramesTocapture * (int)(sizeof(PVOID))) > g_MyStlAllocLimit)
            {
                g_fReachedMemLimit = true;
            }
            else
#endif LIMITSTACKMEMORY

            {
                switch (stackType)
                {
                case StackTypeHeapAlloc:
                    g_nTotalAllocs++;
                    g_TotalAllocSize += (int)stackSubType;
                    break;
                case StackTypeRpc:
                    break;
                default:
                    break;
                }
                CallStack callStack(numFramesToSkip);

                // We want to use the size as the key: see if we've seen this key before
                mapKey key(stackType, stackSubType);
                auto res = g_mapStacksByStackType.find(key);
                if (res == g_mapStacksByStackType.end())
                {
                    g_mapStacksByStackType.insert(pair<mapKey, StacksForStackType>(key, StacksForStackType(callStack)));
                }
                else
                {
                    res->second.AddNewStack(callStack);
                }
                fDidCollectStack = true;
            }
        }
        catch (const std::bad_alloc&)
        {
            g_fReachedMemLimit = true;
        }
    }
    return fDidCollectStack;
}

bool UnCollectStack(StackType stackType, DWORD stackParam)
{
    bool IsTracked = false;
    mapKey key(stackType, stackParam);
    auto res = g_mapStacksByStackType.find(key);
    if (res != g_mapStacksByStackType.end())
    {
        IsTracked = true;
    }
    return IsTracked;
}