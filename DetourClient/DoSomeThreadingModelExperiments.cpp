#include <Windows.h>
#include "crtdbg.h"


#include <atlbase.h>
#include <atlcom.h>




/*// CATLSimpleObject

class ATL_NO_VTABLE CATLSimpleObject :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CATLSimpleObject, &CLSID_ATLSimpleObject>,
{
public:
    CATLSimpleObject()
    {
    }

    //DECLARE_REGISTRY_RESOURCEID(106)


    BEGIN_COM_MAP(CATLSimpleObject)
        COM_INTERFACE_ENTRY(IATLSimpleObject)
    END_COM_MAP()



    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct()
    {
        return S_OK;
    }

    void FinalRelease()
    {
    }

public:



};

OBJECT_ENTRY_AUTO(__uuidof(ATLSimpleObject), CATLSimpleObject)
*/

using namespace ATL;


// see https://blogs.msdn.microsoft.com/calvin_hsia/2014/02/26/see-how-often-your-code-runs-and-how-much-time-it-takes/
#include <initguid.h>
DEFINE_GUID(CLSID_MyObject,
    0xa7d0f17, 0x5c8d, 0x442b, 0xa3, 0x68, 0xd1, 0xdd, 0x36, 0x76, 0x71, 0xbb);

struct __declspec(uuid("F58DA4BA-7971-4A08-BD21-584304A3E8CF"))
IMyObject : IUnknown
{
    virtual HRESULT DoSomething()=0;
};

class CMyObject:
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CMyObject,&CLSID_MyObject>,
    public IMyObject
{
public:
    BEGIN_COM_MAP(CMyObject)
        COM_INTERFACE_ENTRY_IID(CLSID_MyObject, CMyObject)
        COM_INTERFACE_ENTRY(IMyObject)
    END_COM_MAP();

    DECLARE_NOT_AGGREGATABLE(CMyObject)
    DECLARE_NO_REGISTRY()

    CMyObject()
    {
        _ASSERT(true);
    }
    ~CMyObject()
    {
        _ASSERT(true);
    }
    HRESULT DoSomething(void) 
    {
        _ASSERT(true);
        return S_OK;
    }
};

OBJECT_ENTRY_AUTO(CLSID_MyObject, CMyObject);

// define a class that represents this module
class CMyObjectModule : public ATL::CAtlDllModuleT< CMyObjectModule >
{
#if _DEBUG
public:
    CMyObjectModule()
    {
        _ASSERT(true);
    }
    ~CMyObjectModule()
    {
        _ASSERT(true);
    }
#endif _DEBUG
};

// instantiate a static instance of this class on module load
CMyObjectModule _AtlModule;


DWORD WINAPI MyThreadStartRoutine(PVOID param)
{
    CoInitializeEx(0, COINIT_MULTITHREADED); //COINIT_APARTMENTTHREADED and COINIT_MULTITHREADED 
    CComObject<CMyObject>* pMyObj;
    if (S_OK == CComObject<CMyObject>::CreateInstance(&pMyObj))
    {
        CComPtr<IMyObject> pIMyObj;
        if (S_OK == pMyObj->QueryInterface(__uuidof(IMyObject), (LPVOID *)&pIMyObj))
        {
            pIMyObj->DoSomething();
            IMyObject *praw = pIMyObj.Detach();
            praw->AddRef();
            praw->DoSomething();
            praw->Release();
            praw->Release();

        }
    }

    CoUninitialize();
    return 0;
}

void DoSomeThreadingModelExperiments()
{
    DWORD dwThreadId;
    HANDLE hThread = CreateThread(/*LPSECURITY_ATTRIBUTES=*/NULL,
        /*dwStackSize=*/ NULL,
        &MyThreadStartRoutine,
        /* lpThreadParameter*/0,
        /*dwCreateFlags*/ 0, /// CREATE_SUSPENDED
        &dwThreadId
    );

    if (hThread == 0)
    {
        auto err = GetLastError();
        _ASSERT_EXPR(0, "failed to create thread");
    }
    else
    {
        WaitForSingleObject(hThread, /*dwMilliseconds*/ INFINITE);
        auto err = GetLastError();
    }

}