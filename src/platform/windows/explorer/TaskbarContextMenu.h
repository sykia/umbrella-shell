#pragma once
#include "NativeTaskbarLayout.h"
#include <unknwn.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <memory>

namespace umbrella::explorer::context_menu {
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
inline void report(HWND owner, HRESULT status) {
    PostMessageW(owner, RegisterWindowMessageW(L"Umbrella.Taskbar.MenuOpened.1"), 0, status);
}
inline bool readPointer(const void* address, void*& result) {
    SIZE_T read{};
    return ReadProcessMemory(GetCurrentProcess(), address, &result, sizeof(result), &read) && read == sizeof(result);
}
inline FrameworkElement taskbarElement(HWND taskbar) {
    const auto module = GetModuleHandleW(L"Taskbar.dll");
    constexpr GUID identity{0xdab69a79,0xfee0,0x33f0,{0xd8,0x1e,0x02,0x3f,0xbf,0x17,0x2b,0x51}};
    if (!native_layout::matches(module, identity)) return nullptr;
    auto* base = reinterpret_cast<BYTE*>(module);
    wchar_t name[64]{};
    GetClassNameW(taskbar, name, 64);
    const bool secondary = !wcscmp(name, L"Shell_SecondaryTrayWnd");
    const HWND bandWindow = secondary ? FindWindowExW(taskbar, nullptr, L"WorkerW", nullptr)
                                     : reinterpret_cast<HWND>(GetPropW(taskbar, L"TaskbandHWND"));
    if (!bandWindow) return nullptr;
    const auto* band = reinterpret_cast<const BYTE*>(GetWindowLongPtrW(bandWindow, 0));
    if (!band) return nullptr;
    const void* wantedTable = base + (secondary ? 0x1c14c8 : 0x1cf0c8);
    const BYTE* site{};
    for (unsigned i = 0; i < 21; ++i) {
        void* table{};
        if (!readPointer(band + i * sizeof(void*), table)) return nullptr;
        if (table == wantedTable) { site = band + i * sizeof(void*); break; }
    }
    if (!site) return nullptr;
    // Exact-build PDB RVAs. Read the existing host; never patch code or layout data.
    using GetHost = void*(WINAPI*)(const void*, void**);
    using Decref = void(WINAPI*)(void*);
    const auto getHost = reinterpret_cast<GetHost>(base + (secondary ? 0x16be10 : 0x10b540));
    const auto decref = reinterpret_cast<Decref>(base + 0x27ba0);
    void* shared[2]{};
    getHost(site, shared);
    struct Release { void* pointer; Decref release; ~Release() { if (pointer) release(pointer); } } release{shared[1], decref};
    if (!shared[0] || !shared[1]) return nullptr;
    void* unknown{};
    if (!readPointer(static_cast<BYTE*>(shared[0]) + 0x10, unknown) || !unknown) return nullptr;
    FrameworkElement element{nullptr};
    winrt::check_hresult(static_cast<IUnknown*>(unknown)->QueryInterface(winrt::guid_of<FrameworkElement>(), winrt::put_abi(element)));
    return element;
}
}
