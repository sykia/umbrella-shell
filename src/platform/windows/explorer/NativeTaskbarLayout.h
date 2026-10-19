#pragma once
#include <Windows.h>
#include <array>
#include <cstring>

namespace umbrella::explorer::native_layout {
struct Cache { volatile LONG* pointer{}; LONG bits{}; };
inline std::array<Cache, 3> saved{};
inline unsigned references = 0;
inline DWORD owningThread = 0;
inline HKEY settings{};
inline bool valueExisted = false;
inline DWORD originalLocation{};
inline void restoreRegistry() {
    if (!settings) return;
    if (valueExisted) RegSetValueExW(settings, L"TaskbarLocation", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&originalLocation), sizeof(originalLocation));
    else RegDeleteValueW(settings, L"TaskbarLocation");
    RegCloseKey(settings); settings = nullptr;
}
inline bool setEdge(unsigned edge) {
    if (!settings || edge > 3) return false;
    const DWORD value = edge;
    return RegSetValueExW(settings, L"TaskbarLocation", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS;
}
inline bool matches(HMODULE module, const GUID& wanted) {
    if (!module) return false;
    auto* bytes = reinterpret_cast<const BYTE*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto& debug = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (debug.Size > 4096 || debug.VirtualAddress + debug.Size > nt->OptionalHeader.SizeOfImage) return false;
    const auto* entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(bytes + debug.VirtualAddress);
    for (unsigned i = 0; i < debug.Size / sizeof(*entries); ++i) {
        const auto& e = entries[i];
        if (e.Type == IMAGE_DEBUG_TYPE_CODEVIEW && e.SizeOfData >= 24 && e.AddressOfRawData + e.SizeOfData <= nt->OptionalHeader.SizeOfImage) {
            const auto* cv = bytes + e.AddressOfRawData;
            if (!std::memcmp(cv, "RSDS", 4) && !std::memcmp(cv + 4, &wanted, sizeof(wanted)) && *reinterpret_cast<const DWORD*>(cv + 20) == 1) return true;
        }
    }
    return false;
}
inline bool acquire() {
    if (references) {
        if (owningThread != GetCurrentThreadId()) return false;
        ++references; return true;
    }
    const HMODULE modules[]{GetModuleHandleW(L"Taskbar.dll"), GetModuleHandleW(L"Taskbar.View.dll"), GetModuleHandleW(L"SystemTray.dll")};
    constexpr GUID identities[]{
        {0xdab69a79,0xfee0,0x33f0,{0xd8,0x1e,0x02,0x3f,0xbf,0x17,0x2b,0x51}},
        {0x24534974,0xe00e,0x46a5,{0x8f,0x69,0xf6,0x0d,0x0c,0xeb,0x9b,0xd9}},
        {0x2007f389,0x0b9d,0x4d16,{0xaf,0x78,0xba,0xfe,0x77,0x12,0x27,0x28}}
    };
    // Exact-build, symbol-derived cache locations. Only Explorer-local cache data changes; OS-wide feature configuration and executable code are untouched.
    constexpr unsigned offsets[]{0x23be38, 0x92ee10, 0x1eddb8};
    for (unsigned i=0; i<3; ++i) {
        if (!matches(modules[i], identities[i])) return false;
        auto* pointer = reinterpret_cast<volatile LONG*>(reinterpret_cast<BYTE*>(modules[i]) + offsets[i]);
        MEMORY_BASIC_INFORMATION memory{};
        if (!VirtualQuery(const_cast<LONG*>(pointer), &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
            (memory.Protect != PAGE_READWRITE && memory.Protect != PAGE_EXECUTE_READWRITE)) return false;
        saved[i] = {pointer, *pointer & 7};
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &settings) != ERROR_SUCCESS) return false;
    DWORD type{}, length = sizeof(originalLocation);
    const auto read = RegQueryValueExW(settings, L"TaskbarLocation", nullptr, &type, reinterpret_cast<BYTE*>(&originalLocation), &length);
    valueExisted = read == ERROR_SUCCESS;
    if ((read != ERROR_FILE_NOT_FOUND && read != ERROR_SUCCESS) || (valueExisted && (type != REG_DWORD || length != sizeof(originalLocation)))) {
        RegCloseKey(settings); settings = nullptr; return false;
    }
    for (const auto& cache : saved) InterlockedOr(cache.pointer, 7);
    references = 1;
    owningThread = GetCurrentThreadId();
    return true;
}
inline void release() {
    if (!references || --references) return;
    restoreRegistry();
    for (const auto& cache : saved) {
        LONG previous = *cache.pointer;
        while (InterlockedCompareExchange(cache.pointer, (previous & ~7) | cache.bits, previous) != previous) previous = *cache.pointer;
    }
}
inline bool installedBuildSupported() {
    constexpr const wchar_t* files[]{L"\\System32\\Taskbar.dll",
        L"\\SystemApps\\MicrosoftWindows.Client.Core_cw5n1h2txyewy\\Taskbar.View.dll",
        L"\\SystemApps\\MicrosoftWindows.Client.Core_cw5n1h2txyewy\\SystemTray.dll"};
    constexpr GUID identities[]{
        {0xdab69a79,0xfee0,0x33f0,{0xd8,0x1e,0x02,0x3f,0xbf,0x17,0x2b,0x51}},
        {0x24534974,0xe00e,0x46a5,{0x8f,0x69,0xf6,0x0d,0x0c,0xeb,0x9b,0xd9}},
        {0x2007f389,0x0b9d,0x4d16,{0xaf,0x78,0xba,0xfe,0x77,0x12,0x27,0x28}}
    };
    for (unsigned i = 0; i < 3; ++i) {
        std::array<wchar_t, MAX_PATH> path{};
        const UINT count = GetWindowsDirectoryW(path.data(), static_cast<UINT>(path.size()));
        if (!count || count >= path.size() || wcscat_s(path.data(), path.size(), files[i])) return false;
        const auto image = LoadLibraryExW(path.data(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
        if (!image) return false;
        const auto base = reinterpret_cast<HMODULE>(reinterpret_cast<ULONG_PTR>(image) & ~ULONG_PTR(3));
        const bool supported = matches(base, identities[i]);
        FreeLibrary(image);
        if (!supported) return false;
    }
    return true;
}
}

