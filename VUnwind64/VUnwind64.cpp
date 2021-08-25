// VUnwind64.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "VUnwind64.h"
#include <vector>

using namespace std;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

typedef vector<PVOID> vecFrames;

// https://github.com/electronicarts/EAThread/blob/master/source/pc/eathread_callstack_win64.cpp


/*


https://github.com/JochenKalmbach/StackWalker

https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext
You cannot get a valid context for a running thread. Use the SuspendThread function to suspend the thread before calling GetThreadContext.

If you call GetThreadContext for the current thread, the function returns successfully; however, the context returned is not valid.
*/

extern "C" int __declspec(dllexport) CALLBACK TestInterop(
	_In_opt_ CONTEXT * pcontext,
	_In_ int NumFramesToSkip,
	_In_ int NumFramesToCapture,
	__out_ecount_part(NumFramesToCapture, return) PVOID pFrames[],
	_Out_opt_ PULONGLONG pHash
)
{
	CONTEXT context = { 0 };
	if (pHash != 0)
	{
		*pHash = 3;
	}
	if (pcontext == nullptr)
	{
		context.Rsi = 0;
		context.Rip = 0;
		context.P1Home = 1;
		memset(&context, 0, 10);
//		ZeroMemory(&context, 8);
//		RtlCaptureContext(&context);
//		pcontext = &context;
	}
	pFrames[0] = (PVOID)10;
	pFrames[1] = (PVOID)11;
	return 2;
}


extern "C" int __declspec(dllexport) CALLBACK GetCallStack(
	_In_opt_ CONTEXT * pcontext,
	_In_ int NumFramesToSkip,
	_In_ int NumFramesToCapture,
	__out_ecount_part(NumFramesToCapture, return) PVOID pFrames[],
	_Out_opt_ PULONGLONG pHash
)
{
	CONTEXT context = { 0 };
	int nFrames = 0;
	if (pHash != 0)
	{
		*pHash = 0;
	}
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
					pHash += (ULONGLONG)addr; // 32 bit val overflow ok... 
				}
				pFrames[nFrames++] = addr;
				if (nFrames == NumFramesToCapture)
				{
					break;
				}
			}
		}

	}
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
	return nFrames;
}

void RecurSomeLevels(int nLevel)
{
	if (nLevel < 10)
	{
		if (nLevel % 2 == 0)
		{
			RecurSomeLevels(nLevel + 1);
		}
		else
		{
			RecurSomeLevels(nLevel + 1);
		}
	}
	else
	{
		try
		{
			vecFrames _vecFrames; // the stack frames
			_vecFrames.resize(40);
			ULONGLONG pHash = 0;
			int nFrames = GetCallStack(/*context*/nullptr, 1, 40, &_vecFrames[0], &pHash);
			_vecFrames.resize(nFrames);

		}
		catch (const std::exception&)
		{

		}
	}
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	RecurSomeLevels(0);

	// TODO: Place code here.

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_VUNWIND64, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VUNWIND64));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VUNWIND64));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_VUNWIND64);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
