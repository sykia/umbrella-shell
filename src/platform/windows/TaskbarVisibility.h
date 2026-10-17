#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace umbrella::platform {
// Explorer taskbar window classes are implementation details, not a Windows API contract.
class TaskbarVisibility {
public:
    TaskbarVisibility();
    ~TaskbarVisibility();
    TaskbarVisibility(const TaskbarVisibility&) = delete;
    TaskbarVisibility& operator=(const TaskbarVisibility&) = delete;
    bool install();
    bool hide();
    bool restore();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
bool restoreTaskbarWindows();
std::string taskbarStatusJson();
}
