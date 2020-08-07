#include <windows.h>
#import "..\UnitTestProject1\bin\Debug\UnitTestProject1.tlb"
#include "atlbase.h"
#include "atlcom.h"
//#define _ITERATOR_DEBUG_LEVEL 0
#include <queue>
#include <stack>
#include <functional>

#include <initguid.h>
using namespace UnitTestProject1;

// {A90F9940-53C9-45B9-B67B-EE2EDE51CC00}
DEFINE_GUID(CLSID_MyTest ,
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

	HRESULT __stdcall raw_DoHeapStackTests(long parm1, long *pparm2)
	{
		*pparm2 = parm1 + 1;
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
