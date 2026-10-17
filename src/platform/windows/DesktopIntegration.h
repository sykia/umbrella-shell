#pragma once
#include <string>
namespace umbrella::platform {
std::wstring desktopWallpaper();
bool applyDesktopWallpaper(const std::wstring& path);
std::wstring activeApplication();
bool userRecentlyActive();
}
