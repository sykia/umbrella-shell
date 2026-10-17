#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
namespace umbrella::platform {
constexpr UINT barCallback = WM_APP + 31;
unsigned taskbarState();
bool setAutoHide(bool enabled);
int guardTaskbar(DWORD parent, unsigned original, const std::wstring& ready, const std::wstring& stop,
                 bool (*restore)(bool) = setAutoHide);
bool setStartup(bool enabled, const std::wstring& executable);
bool startupEnabled(const std::wstring& executable);
class DesktopBar {
public:
    explicit DesktopBar(HWND window);
    ~DesktopBar();
    DesktopBar(const DesktopBar&) = delete;
    DesktopBar& operator=(const DesktopBar&) = delete;
    bool registered() const { return registered_; }
    bool position(int logicalHeight, double scale);
private:
    HWND window_;
    bool registered_;
    bool positioned_ = false;
};
}
