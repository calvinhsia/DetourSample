
#include "..\DetourSharedBase\DetourShared.h"
#include "atlbase.h"
#include "DetourClientMain.h"


using namespace std;

WCHAR * g_strHeapAllocSizesToCollect = L"8:271 , 72:220, 1031:40";
int g_NumFramesTocapture = 20;
SIZE_T g_HeapAllocSizeMinValue = 0;// 1048576;

HANDLE g_hHeap;

vector<HeapSizeData> g_heapAllocSizes;

extern pfnRtlAllocateHeap Real_RtlAllocateHeap;

CComAutoCriticalSection g_critSectHeapAlloc;

mapStacksByStackType *g_pmapStacksByStackType[StackTypeMax];

StlAllocStats g_MyStlAllocStats;

void StlAllocStats::clear()
{
	for (int i = 0; i < StackTypeMax; i++)
	{
		if (g_pmapStacksByStackType[i] != nullptr)
		{
			g_pmapStacksByStackType[i]->clear();
		}
	}
	_MyStlAllocCurrentTotalAlloc = 0;
	_NumUniqueStacks = 0;;
	_fReachedMemLimit = false;
	_MyStlAllocBytesEverAlloc = 0;
	_MyStlTotBytesEverFreed = 0;
	_nTotFramesCollected = 0;
	_nTotNumHeapAllocs = 0;
	_TotNumBytesHeapAlloc = 0;
	_fReachedMemLimit = false;
}

void InitCollectStacks()
{
	g_hHeap = HeapCreate(/*options*/0, /*dwInitialSize*/65536,/*dwMaxSize*/ g_MyStlAllocStats._MyStlAllocLimit);
	for (int i = 0; i < StackTypeMax; i++)
	{
		MySTLAlloc<BYTE> allocator;
		
		g_pmapStacksByStackType[i] = new (allocator.allocate(sizeof(mapStacksByStackType))) mapStacksByStackType();
//		g_pmapStacksByStackType[i] = new mapStacksByStackType();
	}
}

// call after undetouring
void UninitCollectStacks()
{
	for (int i = 0; i < StackTypeMax; i++)
	{
		MySTLAlloc<BYTE> allocator;
		if (g_pmapStacksByStackType[i] != nullptr)
		{
	//		delete g_pmapStacksByStackType[i];
			g_pmapStacksByStackType[i]->~mapStacksByStackType();
			allocator.deallocate((BYTE *)g_pmapStacksByStackType[i], sizeof(mapStacksByStackType));
		}
	}
	Sleep(1000000);
//	MessageBoxA(0, "about to Heap destroy", "", 0);
	HeapDestroy(g_hHeap);
	g_hHeap = 0;
}


#if TESTING
typedef unordered_map<UINT, unique_ptr<CallStack>> mapHashToStack;

class StkForStackType
{
public:
	StkForStackType(int numFramesToSkip)
	{
		AddNewStack(numFramesToSkip);
	}
	bool AddNewStack(int numFramesToSkip)
	{
		bool fDidAdd = false;
		auto stk = make_unique<CallStack>(numFramesToSkip);
		auto hash = stk->stackHash;
		_mapHashToStack.insert(mapHashToStack::value_type(hash, move(stk)));
		return true;
	}
	mapHashToStack _mapHashToStack;

};


typedef unordered_map<mapKey, unique_ptr<StkForStackType>> mapStksByStkType;

mapStksByStkType g_mapStksByStkType;
#endif TESTING

LONGLONG GetNumStacksCollected()
{
#if TESTING
	auto key = pair<StackType, UINT>(StackTypeHeapAlloc, 1);
	g_mapStksByStkType[key] = make_unique<StkForStackType>(2);
	g_mapStksByStkType.insert(pair<mapKey, unique_ptr<StkForStackType>>(mapKey(StackTypeHeapAlloc, 2), make_unique<StkForStackType>(2)));

	g_mapStksByStkType.insert(mapStksByStkType::value_type(mapKey(StackTypeHeapAlloc, 3), make_unique < StkForStackType>(2)));


	g_mapStksByStkType.clear();
#endif TESTING
	LONGLONG nTotCnt = 0;
	LONGLONG nTotSize = 0;
	int nUniqueStacks = 0;
	int nFrames = 0;
	LONGLONG nRpcStacks[2] = { 0 };
	//g_MyStlAllocStats.g_fReachedMemLimit = false;
	//g_MyStlAllocStats.g_MyStlAllocLimit *= 2; // double mem used: we're done with detouring
	auto save_g_MyStlAllocTotalAlloc = g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc;
	for (int i = 0; i < StackTypeMax; i++)
	{
		for (auto &entry : *g_pmapStacksByStackType[i])
		{
			auto key = entry.first;
			switch (i)
			{
			case StackTypeHeapAlloc:
			{
				auto &stackBySize = entry.second;
				auto sizeAlloc = key;
				auto cnt = stackBySize.GetTotalNumStacks(); // to see the output, use a tracepoint (breakpoint action): output: sizeAlloc={sizeAlloc} cnt={cnt}
				nTotSize += sizeAlloc * cnt;
				nTotCnt += cnt;
				for (auto &stk : entry.second._stacks)
				{
					nUniqueStacks++;
					for (auto frm : stk.second._vecFrames)
					{
						nFrames++;
						auto f = frm;  // output {frm}
					}
				}
			}
			break;
			case StackTypeRpc:
				nRpcStacks[key] += entry.second.GetTotalNumStacks();
				break;
			default:
				break;
			}
		}
	}
	g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc = save_g_MyStlAllocTotalAlloc;
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

	_ASSERT_EXPR(g_MyStlAllocStats._fReachedMemLimit || g_MyStlAllocStats._nTotNumHeapAllocs == nTotCnt, L"Total # allocs shouuld match");
	_ASSERT_EXPR(g_MyStlAllocStats._fReachedMemLimit || g_MyStlAllocStats._TotNumBytesHeapAlloc == nTotSize, L"Total size allocs should match");
	return nTotCnt;
}

// extraInfo can be e.g. elapsed ticks. Don't store in stacks, but raise ETW event with it
bool _stdcall CollectStack(StackType stackType, DWORD stackSubType, DWORD extraInfo, int numFramesToSkip) //noexcept
{
	bool fDidCollectStack = false;
	try
	{
		CComCritSecLock<CComAutoCriticalSection> lock(g_critSectHeapAlloc);
		switch (stackType)
		{
		case StackTypeHeapAlloc:
			g_MyStlAllocStats._nTotNumHeapAllocs++;
			g_MyStlAllocStats._TotNumBytesHeapAlloc += (int)stackSubType;
			break;
		case StackTypeRpc:
			break;
		default:
			break;
		}
		// We want to use the size as the key: see if we've seen this key before
		mapKey key(stackSubType);
		auto res = g_pmapStacksByStackType[stackType]->find(key);
		if (res == g_pmapStacksByStackType[stackType]->end())
		{
			if (!g_MyStlAllocStats._fReachedMemLimit)
			{
				g_pmapStacksByStackType[stackType]->insert(mapStacksByStackType::value_type(key, move(StacksForStackType(numFramesToSkip))));
				fDidCollectStack = true;
			}
		}
		else
		{
			// we still want to calc stack hash and bump count if stack already collected
			fDidCollectStack = res->second.AddNewStack(numFramesToSkip);
		}
	}
	catch (const std::bad_alloc&)
	{
		g_MyStlAllocStats._fReachedMemLimit = true;
	}
	if (fDidCollectStack && !g_MyStlAllocStats._fReachedMemLimit)
	{
		if (g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc >= g_MyStlAllocStats._MyStlAllocLimit)
		{
			g_MyStlAllocStats._fReachedMemLimit = true;
		}
	}
	return fDidCollectStack;
}
