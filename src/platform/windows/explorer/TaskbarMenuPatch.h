#pragma once
#include <Windows.h>
#include <array>
#include <cstring>

namespace umbrella::explorer::menu_patch {
// Complete, relocation-free instructions at the verified x64 entry point.
// An unknown prologue is rejected; this is not a general instruction decoder.
inline constexpr std::array<BYTE, 18> prologue{
    0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24,0xe1};
inline void jump(BYTE* output, const void* target) {
    output[0] = 0xff; output[1] = 0x25;
    std::memset(output + 2, 0, 4);
    std::memcpy(output + 6, &target, sizeof(target));
}
class Patch final {
    BYTE* target_{};
    BYTE* original_{};
    DWORD protection_{};
    std::array<BYTE, prologue.size()> installed_{};
    bool write(const BYTE* bytes) {
        DWORD protection{};
        if (!VirtualProtect(target_, prologue.size(), PAGE_EXECUTE_READWRITE, &protection)) return false;
        if (!protection_) protection_ = protection;
        std::memcpy(target_, bytes, prologue.size());
        DWORD ignored{};
        const bool protectedAgain = VirtualProtect(target_, prologue.size(), protection_, &ignored) != FALSE;
        const bool flushed = FlushInstructionCache(GetCurrentProcess(), target_, prologue.size()) != FALSE;
        return protectedAgain && flushed;
    }
public:
    Patch() = default;
    Patch(const Patch&) = delete;
    Patch& operator=(const Patch&) = delete;
    // Never silently unload code that might still be referenced by a hook.
    ~Patch() = default;
    bool install(BYTE* target, const void* replacement) {
        if (target_ || !target || !replacement || std::memcmp(target, prologue.data(), prologue.size())) return false;
        auto* copy = static_cast<BYTE*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!copy) return false;
        std::memcpy(copy, prologue.data(), prologue.size());
        jump(copy + prologue.size(), target + prologue.size());
        DWORD ignored{};
        if (!VirtualProtect(copy, 4096, PAGE_EXECUTE_READ, &ignored) || !FlushInstructionCache(GetCurrentProcess(), copy, 4096)) {
            VirtualFree(copy, 0, MEM_RELEASE); return false;
        }
        installed_.fill(0x90);
        jump(installed_.data(), replacement);
        target_ = target; original_ = copy;
        if (write(installed_.data())) return true;
        // Even a protection/flush error can follow a successful byte write.
        if (!std::memcmp(target_, installed_.data(), installed_.size())) restore();
        return false;
    }
    void* original() const { return original_; }
    bool active() const { return target_ != nullptr; }
    bool restore() {
        if (!target_) return true;
        if (std::memcmp(target_, installed_.data(), installed_.size())) return false;
        if (!write(prologue.data())) return false;
        VirtualFree(original_, 0, MEM_RELEASE);
        target_ = nullptr; original_ = nullptr; protection_ = 0;
        return true;
    }
};
}
