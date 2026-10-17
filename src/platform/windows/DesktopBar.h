#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace umbrella::platform {
unsigned taskbarState();
bool setAutoHide(bool enabled);
bool requestTaskbarRecovery();
std::uint64_t workingSetBytes();
int guardTaskbar(std::uint32_t parent, unsigned original, const std::wstring& ready, const std::wstring& stop,
                 bool (*restore)(bool) = setAutoHide, bool suppress = false);
bool setStartup(bool enabled, const std::wstring& executable);
bool startupEnabled(const std::wstring& executable);
enum class BarEdge : unsigned { Left = 0, Top = 1, Right = 2, Bottom = 3 };
class DesktopBar {
public:
    explicit DesktopBar(std::uintptr_t window);
    ~DesktopBar();
    DesktopBar(const DesktopBar&) = delete;
    DesktopBar& operator=(const DesktopBar&) = delete;
    bool registered() const;
    bool position(int logicalHeight, double scale, BarEdge edge = BarEdge::Bottom);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
