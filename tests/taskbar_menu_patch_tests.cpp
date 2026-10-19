#include "platform/windows/explorer/TaskbarMenuPatch.h"
#include <cstdio>

using namespace umbrella::explorer;
int replacement() { return 77; }
int main() {
    auto* memory = static_cast<BYTE*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) return 1;
    std::memcpy(memory, menu_patch::prologue.data(), menu_patch::prologue.size());
    // Return 42 after undoing the verified prologue's eight nonvolatile pushes.
    constexpr BYTE tail[]{0xb8,42,0,0,0,0x41,0x5f,0x41,0x5e,0x41,0x5d,0x41,0x5c,0x5f,0x5e,0x5b,0x5d,0xc3};
    std::memcpy(memory + menu_patch::prologue.size(), tail, sizeof(tail));
    DWORD previous{};
    if (!VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &previous)) return 2;
    using Function = int(*)();
    const auto function = reinterpret_cast<Function>(memory);
    if (function() != 42) return 3;
    menu_patch::Patch patch;
    if (!patch.install(memory, reinterpret_cast<const void*>(&replacement))) return 4;
    if (function() != 77 || reinterpret_cast<Function>(patch.original())() != 42) return 5;
    if (patch.install(memory, reinterpret_cast<const void*>(&replacement))) return 6;
    if (!patch.restore() || patch.active() || function() != 42) return 7;
    // Unknown code must remain untouched, including the first invalid byte.
    if (!VirtualProtect(memory, 4096, PAGE_READWRITE, &previous)) return 8;
    memory[0] = 0xcc;
    if (!VirtualProtect(memory, 4096, PAGE_EXECUTE_READ, &previous)) return 9;
    if (patch.install(memory, reinterpret_cast<const void*>(&replacement)) || memory[0] != 0xcc) return 10;
    if (!VirtualFree(memory, 0, MEM_RELEASE)) return 11;
    std::puts("Passed: replacement, original-call fallback, restoration, duplicate/unknown-code rejection; Explorer untouched.");
    return 0;
}
