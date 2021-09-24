// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

/*
-		pFrames,10	0x000001fd28d3bbb0 {0x00007ff622794043 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 78}, 0x00007ff622794032 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 73}, ...}	void *[0x0000000a]
		[0x00000000]	0x00007ff622794043 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 78}	void *
		[0x00000001]	0x00007ff622794032 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 73}	void *
		[0x00000002]	0x00007ff622794043 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 78}	void *
		[0x00000003]	0x00007ff622794032 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 73}	void *
		[0x00000004]	0x00007ff622794043 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 78}	void *
		[0x00000005]	0x00007ff622794032 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 73}	void *
		[0x00000006]	0x00007ff622794043 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 78}	void *
		[0x00000007]	0x00007ff622794032 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 73}	void *
		[0x00000008]	0x00007ff622794043 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 78}	void *
		[0x00000009]	0x00007ff622794032 {VUnwind64.exe!RecurSomeLevels(int nLevel), Line 73}	void *
	*/
extern "C" 
__declspec(dllexport) HRESULT __stdcall GetCallstack64(
	_In_opt_ CONTEXT * pcontext,
	_In_ int NumFramesToSkip,
	_In_ int NumFramesToCapture,
	__out_ecount_part(NumFramesToCapture, return) PVOID pFrames[],
	_Out_ INT* pnFramesWritten,
	_Out_opt_ PULONGLONG pHash
)
{
	HRESULT hr = S_OK;
	CONTEXT context = { 0 };
	int nFrames = 0;
	if (!pnFramesWritten)
		return E_POINTER;

	if (NumFramesToCapture == 0 || !pFrames)
		return E_INVALIDARG;
	if (pHash != 0)
	{
		*pHash = 0;
	}
	*pnFramesWritten = 0;
	try 
	{
		if (pcontext == nullptr)
		{
			ZeroMemory(&context, sizeof(context));
			RtlCaptureContext(&context);
			pcontext = &context;
		}
		PRUNTIME_FUNCTION pRuntimeFunction;
		ULONG64 nImageBase = 0;
		ULONG64 nPrevImageBase = 0;
		while (pcontext->Rip)
		{
			nPrevImageBase = nImageBase;
			pRuntimeFunction = (PRUNTIME_FUNCTION)RtlLookupFunctionEntry(pcontext->Rip, &nImageBase, NULL /*unwindHistoryTable*/);
			if (pRuntimeFunction)
			{
				PVOID handlerData = nullptr;
				ULONG64        establisherFramePointers[2] = { 0, 0 };
				RtlVirtualUnwind(UNW_FLAG_NHANDLER, nImageBase, pcontext->Rip, pRuntimeFunction, pcontext, &handlerData, establisherFramePointers, NULL);
				if (pcontext->Rip)
				{
					auto addr = (PVOID)pcontext->Rip;
					if (pHash != 0)
					{
						*pHash += (ULONGLONG)addr; // 32 bit val overflow ok... 
					}
					pFrames[nFrames++] = addr;
					if (nFrames == NumFramesToCapture)
					{
						break;
					}
				}
			}
		}
	}
	catch (...)
	{
		hr = E_FAIL;
	}
	*pnFramesWritten = nFrames;
	return S_OK;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

