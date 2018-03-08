#include "atlsafe.h"
#include "atlcom.h"
#include <string>
// from https://blogs.msdn.microsoft.com/calvin_hsia/2014/01/30/create-your-own-clr-profiler/

#import <mscorlib.tlb> raw_interfaces_only rename("ReportEvent","ReportEventManaged")

#include <metahost.h>
#pragma comment(lib,"mscoree.lib")
#include <mscoree.h>

#include "shellapi.h"

using namespace mscorlib;
using namespace std;

// find a specified assembly from an AppDomain by enumerating assemblies
HRESULT GetAssemblyFromAppDomain(
    _AppDomain* pAppDomain,
    LPCWSTR wszAssemblyName,
    _Deref_out_opt_ _Assembly **ppAssembly)
{
    *ppAssembly = NULL;
    // get the assemblies into a safearray
    SAFEARRAY *pAssemblyArray = NULL;
    HRESULT hr = pAppDomain->GetAssemblies(&pAssemblyArray);
    if (FAILED(hr))
    {
        return hr;
    }
    // put the safearray into a smart ptr, so it gets released
    CComSafeArray<IUnknown*>    csaAssemblies;
    csaAssemblies.Attach(pAssemblyArray);

    size_t cchAssemblyName = wcslen(wszAssemblyName);

    long cAssemblies = csaAssemblies.GetCount();
    for (long i = 0; i < cAssemblies; i++)
    {
        CComPtr<_Assembly> spAssembly;
        spAssembly = csaAssemblies[i];
        if (spAssembly == NULL)
            continue;
        CComBSTR cbstrAssemblyFullName;
        hr = spAssembly->get_FullName(&cbstrAssemblyFullName);
        if (FAILED(hr))
            continue;
        // is it the one we want?
        if (cbstrAssemblyFullName != NULL &&
            _wcsnicmp(cbstrAssemblyFullName,
                wszAssemblyName,
                cchAssemblyName) == 0)
        {
            *ppAssembly = spAssembly.Detach();
            hr = S_OK;
            break;
        }
    }
    if (*ppAssembly == 0)
    {
        hr = E_FAIL;
    }
    return hr;
}


void StartClrCode(CComBSTR asmFileName, CComBSTR typeNameToInstantiate, CComBSTR typeMemberToCall)
{
    CComPtr<ICLRMetaHost> spClrMetaHost;
    // get a MetaHost
    HRESULT hr = CLRCreateInstance(
        CLSID_CLRMetaHost,
        IID_PPV_ARGS(&spClrMetaHost)
    );
    _ASSERT(hr == S_OK);

    // get a particular runtime version
    CComPtr<ICLRRuntimeInfo> spCLRRuntimeInfo;
    hr = spClrMetaHost->GetRuntime(L"v4.0.30319",
        IID_PPV_ARGS(&spCLRRuntimeInfo)
    );
    _ASSERT(hr == S_OK);

    // get the CorRuntimeHost
    CComPtr<ICorRuntimeHost> spCorRuntimeHost;
    hr = spCLRRuntimeInfo->GetInterface(
        CLSID_CorRuntimeHost,
        IID_PPV_ARGS(&spCorRuntimeHost)
    );
    _ASSERT(hr == S_OK);

    // Start the CLR
    hr = spCorRuntimeHost->Start();
    _ASSERT(hr == S_OK);

    // get the Default app domain as an IUnknown
    CComPtr<IUnknown> spAppDomainThunk;
    hr = spCorRuntimeHost->GetDefaultDomain(&spAppDomainThunk);
    _ASSERT(hr == S_OK);

    // convert the Appdomain IUnknown to a _AppDomain
    CComPtr<_AppDomain> spAppDomain;
    hr = spAppDomainThunk->QueryInterface(IID_PPV_ARGS(&spAppDomain));
    _ASSERT(hr == S_OK);

    // Get the mscorlib assembly
    CComPtr<_Assembly> sp_mscorlib;
    hr = GetAssemblyFromAppDomain(spAppDomain, L"mscorlib", &sp_mscorlib);
    _ASSERT(hr == S_OK);


    // get the Type of "System.Reflection.Assembly"
    CComPtr<_Type> _typeReflectionAssembly;
    hr = sp_mscorlib->GetType_2(
        CComBSTR(L"System.Reflection.Assembly"),
        &_typeReflectionAssembly);
    _ASSERT(hr == S_OK);

    // create the array of args. only need 1 argument, array 
    auto psaLoadFromArgs = SafeArrayCreateVector(
        VT_VARIANT,
        0, //start array at 0
        1); //# elems = 1
    long index = 0;
    // set the array element
    CComVariant arg1(asmFileName); // the argument: the asm to load
    SafeArrayPutElement(psaLoadFromArgs, &index, &arg1);

    //invoke the "Assembly.LoadFrom" public static member to load the paddle.exe
    CComVariant cvtEmptyTarget;
    CComVariant cvtLoadFromReturnValue;
    hr = _typeReflectionAssembly->InvokeMember_3(
        CComBSTR(L"LoadFrom"),
        static_cast<BindingFlags>(BindingFlags_InvokeMethod |
            BindingFlags_Public |
            BindingFlags_Static),
        nullptr, //Binder
        cvtEmptyTarget, // target. Since the method is static, an empty variant
        psaLoadFromArgs, //args
        &cvtLoadFromReturnValue);
    _ASSERT(hr == S_OK);
    SafeArrayDestroy(psaLoadFromArgs); // don't need args any more
    _ASSERT(cvtLoadFromReturnValue.vt == VT_DISPATCH);

    // get the assembly from the return value
    CComPtr<_Assembly> srpAssemblyTarget;
    srpAssemblyTarget.Attach(
        static_cast<_Assembly *>(cvtLoadFromReturnValue.pdispVal
            ));

    // get the desired type from the assembly
    CComPtr<_Type> _typeForm;
    hr = srpAssemblyTarget->GetType_2(
        typeNameToInstantiate,
        &_typeForm);
    _ASSERT(hr == S_OK && _typeForm != nullptr);

    // create an instance of the target type
    CComVariant resTargetInstance;
    hr = srpAssemblyTarget->CreateInstance(
        typeNameToInstantiate,
        &resTargetInstance);
    _ASSERT(hr == S_OK);

    // create an array var for return value of Type->GetMember
    SAFEARRAY *psaMember = nullptr;
    hr = _typeForm->GetMember(typeMemberToCall,
        MemberTypes_Method,
        (BindingFlags)(BindingFlags_Instance + BindingFlags_Public),
        &psaMember);
    _ASSERT(hr == S_OK);

    // put into SafeArray so it gets released
    CComSafeArray<IUnknown *> psaMem;
    psaMem.Attach(psaMember);

    // Get the Method Info for "ShowDialog" from the 1st type in the array
    CComPtr<_MethodInfo> methodInfo;
    hr = psaMem[0]->QueryInterface(
        IID_PPV_ARGS(&methodInfo)
    );
    _ASSERT(hr == S_OK);

    // invoke the ShowDialog method on the Form(WinForm) or Window (wpf)
    hr = methodInfo->Invoke_3(
        resTargetInstance,
        nullptr, //parameters
        nullptr // return value
    );
    _ASSERT(hr == S_OK);

    // stop the runtime
    hr = spCorRuntimeHost->Stop();
    _ASSERT(hr == S_OK);
}


void DoSomeManagedCode()
{

    WCHAR fname[MAX_PATH];
    auto x = GetModuleFileNameW(NULL, fname, MAX_PATH);
    WCHAR szDrive[_MAX_DRIVE];
    WCHAR szDir[MAX_PATH];
    WCHAR szName[MAX_PATH];
    WCHAR szExt[MAX_PATH];

    _wsplitpath_s(fname, szDrive, szDir, szName, szExt);
    _wmakepath_s(fname,szDrive,szDir,L"..\\cslife",L".exe");
    
    StartClrCode(fname, L"CSLife.Form1", L"ShowDialog");
}