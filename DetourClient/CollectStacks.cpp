
#include "..\DetourSharedBase\DetourShared.h"
#include "atlbase.h"
#include "DetourClientMain.h"


using namespace std;

// these are settable by remote settings
WCHAR * g_strHeapAllocSizesToCollect = L"8:271 , 72:220, 1031:40";
int g_NumFramesTocapture = 20;
int g_HeapAllocSizeMinValue = 0;// 1048576;
long g_MyStlAllocLimit = 65536 * 1;


// our private heap
HANDLE g_hHeapDetourData;


vector<HeapSizeData> g_heapAllocSizes;


CComAutoCriticalSection g_critSectHeapAlloc;


typedef std::vector<PVOID, MySTLAlloc<PVOID, StlAllocUseCallStackHeap>> vecFrames;

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

typedef std::unordered_map<mapKey, StacksForStackType,
	std::hash<mapKey>,
	std::equal_to<mapKey>,
	MySTLAlloc<std::pair<const mapKey, StacksForStackType>, StlAllocUseCallStackHeap>
> mapStacksByStackType;

// map the Size of an alloc to all the stacks that allocated that size.
// note: if we're looking for all allocs of a specific size (e.g. 1Mb), then no need for a map by size (because all keys will be the same): more efficient to just use a mapStacks

mapStacksByStackType *g_pmapStacksByStackType[StackTypeMax];

StlAllocStats g_MyStlAllocStats;

void InitCollectStacks()
{
	// create a heap that stores our private data: MyTlsData and Call stacks
	g_hHeapDetourData = HeapCreate(/*options*/0, /*dwInitialSize*/65536,/*dwMaxSize*/ 0);

	// init Tls map
	// create an allocator type from the mapThreadIdToTls allocator
	using myTlsAllocator = typename allocator_traits<mapThreadIdToTls::allocator_type>::rebind_alloc<mapThreadIdToTls>;
	// create an instance of the allocator
	mapThreadIdToTls::allocator_type  allocTls_Type;
	myTlsAllocator allocTls(allocTls_Type);
	
	g_pmapThreadIdToTls = new (allocTls.allocate(1)) mapThreadIdToTls();



	// init stack maps
	// create an allocator type for BYTE from the same allocator as the map
	using myStackTypeAllocator = typename allocator_traits<mapStacksByStackType::allocator_type>::rebind_alloc<mapStacksByStackType>;
	// create an instance of the allocator type
	mapStacksByStackType::allocator_type alloc_type;
	myStackTypeAllocator allocator(alloc_type);
	for (int i = 0; i < StackTypeMax; i++)
	{
		// create using our allocator using placement new 
		g_pmapStacksByStackType[i] = new (allocator.allocate(1)) mapStacksByStackType();
	}
}

// call after undetouring
void UninitCollectStacks()
{
	// create an allocator type for BYTE from the same allocator as the map
	using myByteAllocator = typename allocator_traits<mapStacksByStackType::allocator_type>::rebind_alloc<mapStacksByStackType>;
	// create an instance of the allocator type
	mapStacksByStackType::allocator_type alloc_type;
	myByteAllocator allocator(alloc_type);

	for (int i = 0; i < StackTypeMax; i++)
	{
		if (g_pmapStacksByStackType[i] != nullptr)
		{
			g_pmapStacksByStackType[i]->~mapStacksByStackType(); // invoke dtor
			allocator.deallocate(g_pmapStacksByStackType[i], 1); // delete the placement new
		}
	}
	//	MessageBoxA(0, "about to Heap destroy", "", 0);
	_ASSERT_EXPR(g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[StlAllocUseCallStackHeap] == 0, L"Should be leakless");


	if (g_pmapThreadIdToTls != nullptr)
	{
		// now uninit the Tls map
		// create an allocator type from the mapThreadIdToTls allocator
		using myTlsAllocator = typename allocator_traits<mapThreadIdToTls::allocator_type>::rebind_alloc<BYTE>;
		// create an instance of the allocator
		mapThreadIdToTls::allocator_type  allocTls_Type;
		myTlsAllocator allocTls(allocTls_Type);

		for (auto &item : *g_pmapThreadIdToTls)
		{
			item.second->~MyTlsData();
			allocTls.deallocate((BYTE *)item.second, sizeof(*item.second));
		}
		g_pmapThreadIdToTls->clear();
		_ASSERT_EXPR(MyTlsData::g_numTlsInstances == 0, L"tls instance leak");

		g_pmapThreadIdToTls->~mapThreadIdToTls();
		allocTls.deallocate((BYTE *)g_pmapThreadIdToTls, sizeof(mapThreadIdToTls));
		g_pmapThreadIdToTls = nullptr;
	}
	_ASSERT_EXPR(g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[StlAllocUseTlsHeap] == 0, L"tls instance mem leak");


	// now destroy the heap
	for (int i = 0; i < StackTypeHeapAlloc; i++)
	{
		_ASSERT_EXPR(g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[i] == 0, L"Heap leak");
	}
	HeapDestroy(g_hHeapDetourData);

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
        CComCritSecLock<CComAutoCriticalSection> lock(g_critSectHeapAlloc);
        auto res = g_pmapStacksByStackType[stackType]->find(key);
		if (res == g_pmapStacksByStackType[stackType]->end())
		{
			if (!g_MyStlAllocStats._fReachedMemLimit)
			{
				g_pmapStacksByStackType[stackType]->insert(mapStacksByStackType::value_type(key, move(StacksForStackType(numFramesToSkip))));
				fDidCollectStack = true;
			}
			else
			{
				g_MyStlAllocStats._NumStacksMissed[stackType]++;
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
		if (g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[StlAllocUseCallStackHeap] >= g_MyStlAllocLimit)
		{
			g_MyStlAllocStats._fReachedMemLimit = true;

            /*
            Sending telemetry can cause:
            clr.dll!MdaXmlMessage::SendDebugEvent() Line 929	C++
            clr.dll!MdaXmlMessage::SendEvent() Line 806	C++
            clr.dll!MdaXmlMessage::SendMessage() Line 1027	C++
            clr.dll!MdaXmlMessage::SendMessagef(int resourceID=6440, ...) Line 958	C++
            clr.dll!MdaReentrancy::ReportViolation() Line 1918	C++
            clr.dll!HasIllegalReentrancyRare() Line 13510	C++
            0167d04d()	Unknown
            [Frames below may be incorrect and/or missing]
            msenv.dll!CGlobalServiceProvider::IsAsyncService(const _GUID & serviceId={...}) Line 1959	C++
            msenv.dll!CGlobalServiceProvider::QueryServiceInternal(bool failIfPackageNotLoaded=false, const _GUID & serviceId={...}, const _GUID & serviceInterface={...}, void * * ppvObj=0x010fd6e8) Line 1875	C++
            msenv.dll!CGlobalServiceProvider::QueryService(const _GUID & rsid={...}, const _GUID & riid={...}, void * * ppvObj=0x010fd6e8) Line 1837	C++
            vslog.dll!CVsComModule::QueryService(const _GUID & rsid={...}, const _GUID & riid={...}, void * * ppvObj=0x010fd6e8) Line 51	C++
            >	vslog.dll!VSResponsiveness::CollectStack(VSResponsiveness::StackType stackSubType, unsigned long) Line 283	C++
            vslog.dll!VSResponsiveness::Detours::DetourRtlAllocateHeap(void * hHeapHandle=0x013f0000, unsigned long dwFlags=0, unsigned long size=82) Line 641	C++
            clr.dll!RtlAllocateHeap(void * HeapHandle=0x013f0000, unsigned long Flags=0, unsigned long Size=74) Line 203	C++
            clr.dll!IsolationAllocateStringRoutine(unsigned long ByteCount=74) Line 34	C++
            clr.dll!RtlAllocateLUnicodeString(unsigned long Bytes=74, _LUNICODE_STRING * String=0x06bab7c0) Line 258	C++
            clr.dll!RtlDuplicateLUnicodeString(const _LUNICODE_STRING * Source=0x7680760b, _LUNICODE_STRING * Destination=0x01440b54) Line 427	C++
            clr.dll!id_AssignInternalAttributeFromPublicAttribute<Windows::Isolation::Rtl::_IDENTITY_ATTRIBUTE>(_RTL_ALLOCATION_LIST * AllocationList=0x00000000, const Windows::Isolation::Rtl::_IDENTITY_ATTRIBUTE & Attribute={...}, CInternalIdentityAttribute & NewInternalAttribute={...}) Line 417	C++
            */
            //CComPtr<IVsTaskSchedulerService> pTaskSchedulerService;
            //if (SUCCEEDED(_Module.QueryService(IID_SVsTaskSchedulerService, IID_PPV_ARGS(&pTaskSchedulerService))))
            //{
            //    CComPtr<IVsTask> spTask;
            //    HRESULT hr = VsTaskLibraryHelper::CreateAndStartTask(
            //        VSTC_UITHREAD_BACKGROUND_PRIORITY,
            //        VSTCRO_None,
            //        []() -> HRESULT
            //    {
            //        CComPtr<IVsTelemetryEvent> spStacksMemoryFull;
            //        if (SUCCEEDED(TelemetryHelper::CreateEventW(L"vs/core/detours/collectstacks/StackMemFull", &spStacksMemoryFull)))
            //        {
            //            spStacksMemoryFull->SetLongProperty(L"vs.core.detours.collectstacks.StackMemFull.limit", g_MyStlAllocLimit);
            //            spStacksMemoryFull->SetLongProperty(L"vs.core.detours.collectstacks.StackMemFull.TotalAlloc", g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[StlAllocUseCallStackHeap]);
            //            TelemetryHelper::PostEvent(spStacksMemoryFull);
            //        }

            //        return S_OK;
            //    },
            //        pTaskSchedulerService,
            //        L"DetourCollectStack",
            //        &spTask);

            //}

		}
	}
	return fDidCollectStack;
}
