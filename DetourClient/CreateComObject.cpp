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
    CComPtr<IATLSimpleObject> pMyObj;

    HRESULT hr = CoCreateInstance(__uuidof(ATLSimpleObject), nullptr, CLSCTX_INPROC, __uuidof(IATLSimpleObject), (LPVOID *) &pMyObj);
    hr  = pMyObj->MyMethod(L"foo");
}