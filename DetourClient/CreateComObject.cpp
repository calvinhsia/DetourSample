#include <Windows.h>
//#import "Atlproject1.tlb"
#if _DEBUG
#import "..\..\..\ATLProject1\Debug\ATLProject1.tlb"
#else
#import "..\..\..\ATLProject1\Release\ATLProject1.tlb"
#endif
#include <atlbase.h>
#include <atlcom.h>

//#import "C:\Users\calvinh\source\repos\DetourSharedBase\ATLProject1\Debug\ATLProject1.tlb"

using namespace ATLProject1Lib;


void CreateComObject()
{
    //    CoInitializeEx(0, COINIT_APARTMENTTHREADED); //COINIT_APARTMENTTHREADED and COINIT_MULTITHREADED 
    CoInitializeEx(0, COINIT_MULTITHREADED); //COINIT_APARTMENTTHREADED and COINIT_MULTITHREADED 

    //HMODULE hComBase = GetModuleHandleA("combase.dll");
    //auto addr = GetProcAddress(hComBase, "ObjectStublessClient3");
    //_asm mov eax, addr
    //_asm jmp eax

    /*
        CLIENT_CALL_RETURN RPC_VAR_ENTRY
        NdrClientCall2(
        PMIDL_STUB_DESC     pStubDescriptor,
        PFORMAT_STRING      pFormat,
        ...
        )
    /*
    This routine is called from the object stubless proxy dispatcher.
    */


    CComPtr<IATLSimpleObject> pMyObj;

    HRESULT hr = CoCreateInstance(__uuidof(ATLSimpleObject), nullptr, CLSCTX_INPROC, __uuidof(IATLSimpleObject), (LPVOID *)&pMyObj);
    for (int i = 0; i < 100; i++)
    {
        hr = pMyObj->MyMethod(L"foo");
    }

}