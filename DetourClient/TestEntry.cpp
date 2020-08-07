#include <windows.h>
#import "..\UnitTestProject1\bin\Debug\UnitTestProject1.tlb"
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

	STDMETHOD(raw_DoHeapStackTests)(long parm1, long* pparm2, BSTR bstrStrin, BSTR* pBstr)
	{
		*pparm2 = parm1 + 1;
		CComBSTR strIn(bstrStrin);
		strIn += L"GotStr";
		*pBstr = strIn.Detach();// SysAllocString(strIn.m_str);
		return S_OK;
	}
	STDMETHOD(raw_StartDetours)(long* pparm2)
	{
		StartDetouring((PVOID*)pparm2);

		InitCollectStacks();


		HMODULE hmDevenv = GetModuleHandleA("DetourLib.dll");

		//	g_mapStacks = new (malloc(sizeof(mapStacks)) mapStacks(MySTLAlloc < pair<const SIZE_T, vecStacks>(GetProcessHeap());

		auto fnRedirectDetour = reinterpret_cast<pfnRedirectDetour>(GetProcAddress(hmDevenv, REDIRECTDETOUR));
		VSASSERT(fnRedirectDetour != nullptr, "Failed to get RedirectDetour");
		auto res = fnRedirectDetour(DTF_GetModuleFileNameA, MyGetModuleFileNameA, (PVOID*)&g_real_GetModuleFileNameA);
		VSASSERT(res == S_OK, "Redirecting detour to MyGetModuleFileNameA");

		char szBuff[MAX_PATH];
		auto len = GetModuleFileNameA(0, szBuff, sizeof(szBuff));


		//auto res = fnRedirectDetour(DTF_RtlAllocateHeap, MyRtlAllocateHeap, (PVOID*)&Real_RtlAllocateHeap);
		//VSASSERT(res == S_OK, "Redirecting detour to allocate heap");

		//res = fnRedirectDetour(DTF_HeapReAlloc, MyHeapReAlloc, (PVOID*)&Real_HeapReAlloc);
		//VSASSERT(res == S_OK, "Redirecting detour to heapReAlloc");


		//res = fnRedirectDetour(DTF_RtlFreeHeap, MyRtlFreeHeap, (PVOID*)&Real_RtlFreeHeap);
		//VSASSERT(res == S_OK, "Redirecting detour to free heap");



		return S_OK;
	}
	STDMETHOD(raw_StopDetours)(long pparm2)
	{
		UninitCollectStacks();
		StopDetouring((PVOID)pparm2);
		if (!HeapDestroy(g_hHeapDetourData))
		{
			VSASSERT(false, "Couldn't destroy heap");
		}
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
