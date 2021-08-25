#include <Windows.h>
#include "crtdbg.h"


#include <atlbase.h>
#include <atlcom.h>


using namespace ATL;

//Here is code that defines a simple interface, a COM object that implements that interface (can be apartment model or free threaded)

void LogOutput(LPCWSTR wszFormat, ...)
{
    if (IsDebuggerPresent())
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        WCHAR buf[1000];
        swprintf_s(buf, L"%2d/%02d/%02d %2d:%02d:%02d:%03d thrd=%d ", st.wMonth, st.wDay, st.wYear - 2000, st.wHour,
            st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId());
        OutputDebugStringW(buf);
        va_list insertionArgs;
        va_start(insertionArgs, wszFormat);
        _vsnwprintf_s(buf, _countof(buf), wszFormat, insertionArgs);
        va_end(insertionArgs);
        OutputDebugStringW(buf);
        OutputDebugStringW(L"\r\n");
    }
}




// see https://blogs.msdn.microsoft.com/calvin_hsia/2014/02/26/see-how-often-your-code-runs-and-how-much-time-it-takes/
#include <initguid.h>
#include <tuple>
DEFINE_GUID(CLSID_MyObject,
    0xa7d0f17, 0x5c8d, 0x442b, 0xa3, 0x68, 0xd1, 0xdd, 0x36, 0x76, 0x71, 0xbb);

struct __declspec(uuid("F58DA4BA-7971-4A08-BD21-584304A3E8CF"))
    IMyObject : IUnknown
{
    // a method with a standard marshallable parameter
    virtual HRESULT STDMETHODCALLTYPE DoSomething() = 0;
    virtual HRESULT STDMETHODCALLTYPE DoSomethingWithAString(BSTR strParam) = 0;
};

class CMyObject :
    public CComObjectRootEx<CComMultiThreadModel>,
    //    public CComCoClass<CMyObject, &CLSID_MyObject>, // cocreateinstance
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
        LogOutput(L"In CMyObj CTor");
    }
    ~CMyObject()
    {
        LogOutput(L"In CMyObj DTor");
    }
    STDMETHOD(DoSomething)(void)
    {
        LogOutput(L"In CMyObj DoSomething");
        return S_OK;
    }
    STDMETHOD(DoSomethingWithAString)(BSTR strParam)
    {
        LogOutput(L"In CMyObj DoSomethingWithAString %s", strParam);
        return S_OK;
    }
};

// OBJECT_ENTRY_AUTO(CLSID_MyObject, CMyObject); // cocreateinstance

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
    LogOutput(L"In MyThread routine");
    //HRESULT hr = CoGetInterfaceAndReleaseStream(LPSTREAM(param), __uuidof(IMyObject), (LPVOID *)&pMyObj);
    //_ASSERT_EXPR(hr == S_OK, L"Failed to CoGetInterfaceAndReleaseStream");
    CComObject<CMyObject>* pMyObj = (CComObject<CMyObject>*)param;
    pMyObj->DoSomething();
    return 0;
}

void DoSomeThreadingModelExperiments()
{
    std::ignore = CoInitializeEx(0, COINIT_MULTITHREADED); //COINIT_APARTMENTTHREADED and COINIT_MULTITHREADED 
    CComObject<CMyObject>* pMyObj;
    HRESULT hr;
    hr = CComObject<CMyObject>::CreateInstance(&pMyObj);
    _ASSERT_EXPR(hr == S_OK, L"Failed to createinstance");
    CComPtr<IMyObject> pIMyObj;
    hr = pMyObj->QueryInterface(__uuidof(IMyObject), (LPVOID *)&pIMyObj);
    _ASSERT_EXPR(hr == S_OK, L"Failed to qi");

    pIMyObj->DoSomethingWithAString(CComBSTR(L"foo"));
    CComPtr<IStream> pStream;
    //hr = CoMarshalInterThreadInterfaceInStream(__uuidof(IMyObject), pMyObj->_GetRawUnknown(), &pStream);
    //// REGDB_E_IIDNOTREG Interface not registered 80040155
    //_ASSERT_EXPR(hr == S_OK, L"Failed to marshal");


    DWORD dwThreadId;
    HANDLE hThread = CreateThread(/*LPSECURITY_ATTRIBUTES=*/NULL,
        /*dwStackSize=*/ NULL,
        &MyThreadStartRoutine,
        /* lpThreadParameter*/pMyObj,
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
    CoUninitialize();
}