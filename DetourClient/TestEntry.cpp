#include <windows.h>
#import "..\UnitTestProject1\bin\Debug\UnitTestProject1.tlb" raw_interfaces_only
#include "atlbase.h"
#include "atlcom.h"
//#define _ITERATOR_DEBUG_LEVEL 0
#include <queue>
#include <stack>
#include <functional>

#include <initguid.h>

#include "..\DetourSharedBase\DetourShared.h"
#include "DetourClientMain.h"

using namespace UnitTestProject1;

//typedef DWORD(WINAPI* pfnGetModuleFileNameA)(
//	_In_opt_ HMODULE hModule,
//	_Out_writes_to_(nSize, ((return < nSize) ? (return +1) : nSize)) LPSTR lpFilename,
//	_In_ DWORD nSize
//	);
//pfnGetModuleFileNameA real_GetModuleFileNameA;
//
//DWORD WINAPI MyGetModuleFileNameA(
//	_In_opt_ HMODULE hModule,
//	_Out_writes_to_(nSize, ((return < nSize) ? (return +1) : nSize)) LPSTR lpFilename,
//	_In_ DWORD nSize
//)
//{
//	return real_GetModuleFileNameA(hModule, lpFilename, nSize);
//}



// {A90F9940-53C9-45B9-B67B-EE2EDE51CC00}
DEFINE_GUID(CLSID_MyTest,
	0xa90f9940, 0x53c9, 0x45b9, 0xb6, 0x7b, 0xee, 0x2e, 0xde, 0x51, 0xcc, 0x0);

class MyTest :
	public ITestHeapStacks,
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<MyTest, &CLSID_MyTest>
{
public:
	BEGIN_COM_MAP(MyTest)
		COM_INTERFACE_ENTRY_IID(CLSID_MyTest, MyTest)
		COM_INTERFACE_ENTRY(ITestHeapStacks)
	END_COM_MAP()
	DECLARE_NOT_AGGREGATABLE(MyTest)
	DECLARE_NO_REGISTRY()

	STDMETHOD(DoHeapStackTests)(long parm1, long* pparm2, BSTR bstrStrin, BSTR* pBstr)
	{
		*pparm2 = parm1 + 1;
		CComBSTR strIn(bstrStrin);
		strIn += L"GotStr";
		*pBstr = strIn.Detach();// SysAllocString(strIn.m_str);
		return S_OK;
	}
	STDMETHOD(StartDetours)(long* pparm2)
	{
		StartDetouring((PVOID*)pparm2);

		InitCollectStacks();

		HMODULE hmDevenv = GetModuleHandleA("DetourLib.dll");

		//	g_mapStacks = new (malloc(sizeof(mapStacks)) mapStacks(MySTLAlloc < pair<const SIZE_T, vecStacks>(GetProcessHeap());
		if (hmDevenv == 0)
		{
			return E_FAIL;
		}
		auto fnRedirectDetour = reinterpret_cast<pfnRedirectDetour>(GetProcAddress(hmDevenv, REDIRECTDETOUR));
		VSASSERT(fnRedirectDetour != nullptr, "Failed to get RedirectDetour");
		auto res = fnRedirectDetour(DTF_GetModuleFileNameA, MyGetModuleFileNameA, (PVOID*)&g_real_GetModuleFileNameA);
		VSASSERT(res == S_OK, "Redirecting detour to MyGetModuleFileNameA");

		char szBuff[MAX_PATH];
		auto len = GetModuleFileNameA(0, szBuff, sizeof(szBuff));

		HeapLock(GetProcessHeap());

		res = fnRedirectDetour(DTF_RtlAllocateHeap, MyRtlAllocateHeap, (PVOID*)&Real_RtlAllocateHeap);
		VSASSERT(res == S_OK, "Redirecting detour to allocate heap");

		res = fnRedirectDetour(DTF_HeapReAlloc, MyHeapReAlloc, (PVOID*)&Real_HeapReAlloc);
		VSASSERT(res == S_OK, "Redirecting detour to heapReAlloc");

		res = fnRedirectDetour(DTF_RtlFreeHeap, MyRtlFreeHeap, (PVOID*)&Real_RtlFreeHeap);
		VSASSERT(res == S_OK, "Redirecting detour to free heap");

		HeapUnlock(GetProcessHeap());
		return S_OK;
	}
	STDMETHOD(SetHeapCollectParams)(
		BSTR HeapSizesToCollect,
		long NumFramesToCapture,
		long HeapAllocSizeMinValue,
		long StlAllocLimit)
	{
		g_NumFramesTocapture = NumFramesToCapture;
		g_HeapAllocSizeMinValue = HeapAllocSizeMinValue;
		g_MyStlAllocLimit = StlAllocLimit;
		SetHeapSizesToCollect(HeapSizesToCollect);
		return S_OK;
	}


	STDMETHOD(StopDetours)(long pDetours)
	{
		HeapLock(GetProcessHeap());
		StopDetouring((PVOID)pDetours);
		HeapUnlock(GetProcessHeap());
		return S_OK;
	}

	STDMETHOD(GetStats)(long ptrHeapStats)
	{
		if (ptrHeapStats != 0)
		{
			// heapstats should be read before UuninitCollectStacks
			auto pHeapStats = (HeapCollectStats*)ptrHeapStats;

			pHeapStats->MyRtlAllocateHeapCount = g_MyRtlAllocateHeapCount;
			pHeapStats->MyStlAllocLimit = g_MyStlAllocLimit;
			pHeapStats->MyStlAllocCurrentTotalAlloc = g_MyStlAllocStats._MyStlAllocCurrentTotalAlloc[StlAllocUseCallStackHeap];
			pHeapStats->MyStlAllocBytesEverAlloc = g_MyStlAllocStats._MyStlAllocBytesEverAlloc[StlAllocUseCallStackHeap];
			pHeapStats->MyStlTotBytesEverFreed = g_MyStlAllocStats._MyStlTotBytesEverFreed[StlAllocUseCallStackHeap];
			pHeapStats->NumUniqueStacks = g_MyStlAllocStats._NumUniqueStacks;
			pHeapStats->nTotNumHeapAllocs = g_MyStlAllocStats._nTotNumHeapAllocs;
			pHeapStats->nTotFramesCollected = g_MyStlAllocStats._nTotFramesCollected;
			pHeapStats->TotNumBytesHeapAlloc = g_MyStlAllocStats._TotNumBytesHeapAlloc;
			pHeapStats->NumStacksMissed = g_MyStlAllocStats._NumStacksMissed[StackTypeHeapAlloc];
			auto x = g_pmapStacksByStackType[StackTypeHeapAlloc]->size();

			pHeapStats->fReachedMemLimit = g_MyStlAllocStats._fReachedMemLimit;
			for (int i = 0; i < pHeapStats->NumDetailRecords; i++)
			{
				auto pHeapDetail = (HeapCollectStatDetail*)(ptrHeapStats + sizeof(HeapCollectStats) + i * sizeof(HeapCollectStatDetail));
				auto sizeAlloc = pHeapDetail->AllocSize;
				auto it = find_if(g_heapAllocSizes.begin(), g_heapAllocSizes.end(), [sizeAlloc](HeapSizeData data)
					{
						return data._nSize == sizeAlloc;
					});
				VSASSERT(it != g_heapAllocSizes.end(), "size not found?");
				pHeapDetail->NumStacksCollected = it->_nStacksCollected;
				pHeapDetail->AllocThresh = it->_nThreshold;
			}

		}
		return S_OK;
	}

	STDMETHOD(GetCollectedStacks)(long* pnumAllocs, long* pAddresses)
	{
		if (g_pmapAllocToStackHash != nullptr)
		{
			*pnumAllocs = g_pmapAllocToStackHash->size();
			*pAddresses = (long)CoTaskMemAlloc(*pnumAllocs * 2 * sizeof(PVOID));
			UINT * ptr = (UINT *)*pAddresses;
			for (auto& itm : *g_pmapAllocToStackHash)
			{
				ptr[0] = (UINT)itm.first;
				ptr[1] = itm.second.stackHash;
				ptr += 2;
			}
		}
		return S_OK;
	}
	STDMETHOD(CollectStacksUninitialize)()
	{
		UninitCollectStacks();
		return S_OK;
	}
};

OBJECT_ENTRY_AUTO(CLSID_MyTest, MyTest)

//// define a class that represents this module
//class CMyTestModule : public ATL::CAtlDllModuleT< CMyTestModule >
//{
//#if _DEBUG
//public:
//	CMyTestModule()
//	{
//		int x = 0; // set a bpt here
//	}
//	~CMyTestModule()
//	{
//		int x = 0; // set a bpt here
//	}
//#endif _DEBUG
//};
//
//
//// instantiate a static instance of this class on module load
//CMyTestModule _AtlModule;
// this gets called by CLR due to env var settings
_Check_return_
STDAPI DllGetClassObject(__in REFCLSID rclsid, __in REFIID riid, __deref_out LPVOID FAR* ppv)
{
	HRESULT hr = E_FAIL;
	hr = AtlComModuleGetClassObject(&_AtlComModule, rclsid, riid, ppv);
	//  hr= CComModule::GetClassObject();
	return hr;
}
//tell the linker to export the function
#pragma comment(linker, "/EXPORT:DllGetClassObject=_DllGetClassObject@12,PRIVATE")

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow()
{
	return S_OK;
}
//tell the linker to export the function
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=_DllCanUnloadNow@0,PRIVATE")
