#pragma once
#include <Windows.h>
namespace umbrella::explorer::menu_protocol {
inline constexpr ULONG_PTR version = 0x554d4e01;
inline constexpr wchar_t ownerClass[] = L"Umbrella.TaskbarMenu.Owner.1";
inline constexpr wchar_t attach[] = L"Umbrella.TaskbarMenu.Attach.1";
inline constexpr wchar_t detach[] = L"Umbrella.TaskbarMenu.Detach.1";
inline constexpr wchar_t readyProperty[] = L"Umbrella.TaskbarMenu.Ready.1";
inline constexpr UINT action = WM_APP + 61;
inline constexpr UINT status = WM_APP + 62;
inline constexpr UINT detached = WM_APP + 63;
inline constexpr UINT refresh = WM_APP + 64;
}
