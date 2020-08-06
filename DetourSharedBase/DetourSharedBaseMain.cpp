#include <Windows.h>
#include "..\Detours\detours.h"
#include "DetourShared.h"
#include "crtdbg.h"
#include "string"
#include "stdio.h"
#include <vector>

using namespace std;

typedef int (WINAPI* PfnStartVisualStudio)(void);



int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	string desc = g_szArch;
#if _DEBUG
	desc += " debug";
#else
	desc += " release";
#endif _DEBUG
	{
		HeapSetInformation(GetProcessHeap(), HeapEnableTerminationOnCorruption, nullptr, 0);
		int nCnt = 0;
		LPVOID x = 0;
		LPVOID lastNonZero = 0;
		vector<LPVOID> myvec;
		while (1)
		{

			x = VirtualAlloc(0, 1 * 1024 * 1024, MEM_RESERVE, PAGE_READWRITE);
			if (x == 0)
			{
				//while (1)
				//{
				//	auto y = VirtualAlloc(0, 1 * 1024, MEM_RESERVE, PAGE_READWRITE);
				//	if (y == 0)
				//	{
				//		break;
				//	}
				//}
				break;
			}
			else
			{
				lastNonZero = x;
				myvec.push_back(x);
			}
			nCnt++;
		}
		for (UINT i = 3500; i < myvec.size()-1  ; i++)
		{
			auto ndx = i;// myvec.size() - 1;
			auto addr = myvec[ndx];
//			myvec.erase(myvec.end() - 1);

			VirtualFree(addr, 0, MEM_RELEASE);

		}
		PVOID pDetours = nullptr;
		StartDetouring(&pDetours);
//		SharedDetours sharedDetours;
		MessageBoxA(0, "Detours in place, calling unredirected version of MessageboxA", desc.c_str(), 0);

		HMODULE hModule = LoadLibrary(L"DetourClient.dll");
		if (hModule == nullptr)
		{
			auto x = GetLastError();
		}
		_ASSERT_EXPR(hModule != nullptr, L"couldn't load DetourCLient.dll");
		auto StartVS = (PfnStartVisualStudio)GetProcAddress(hModule, STARTVISUALSTUDIO);
		_ASSERT_EXPR(StartVS, "Could not get _StartVisualStudio");
		StartVS();

		auto h = GetModuleHandleA(0);
		StopDetouring(pDetours);
	}

	// detours are now uninstalled:
	auto h2 = GetModuleHandleA(0);
	MessageBoxA(0, "No more Detours in place, calling WinApi MessageboxA", desc.c_str(), 0);

}