#include "DesktopLayer.h"
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QDebug>
#include <Windows.h>
#include <vector>
#include <algorithm>

namespace umbrella::platform {
namespace {
bool isClass(HWND window, const wchar_t* name) {
    wchar_t value[64]{};
    return GetClassNameW(window, value, 64) && wcscmp(value, name) == 0;
}
DWORD process(HWND window) { DWORD pid{}; GetWindowThreadProcessId(window, &pid); return pid; }
bool below(HWND window, HWND above) {
    unsigned remaining = 512;
    for (auto current = GetWindow(window, GW_HWNDPREV); current && remaining--; current = GetWindow(current, GW_HWNDPREV))
        if (current == above) return true;
    return false;
}
struct Tree { HWND parent{}, icons{}, wallpaper{}; bool operator==(const Tree&) const = default; };
Tree discover() {
    const auto shell = GetShellWindow();
    const auto pid = process(shell);
    if (!shell || !pid) return {};
    const auto progman = FindWindowW(L"Progman", nullptr);
    if (!progman || process(progman) != pid) return {};
    // Raised desktop on recent Windows 11: layered icon view and wallpaper
    // are siblings inside Progman. Never hide/reorder/style Explorer windows.
    const auto icons = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
    const auto wallpaper = FindWindowExW(progman, nullptr, L"WorkerW", nullptr);
    if (icons && wallpaper && process(icons) == pid && process(wallpaper) == pid
        && IsWindowVisible(icons) && IsWindowVisible(wallpaper) && below(wallpaper, icons)
        && (GetWindowLongPtrW(icons, GWL_EXSTYLE) & WS_EX_LAYERED))
        return {progman, icons, wallpaper};
    struct Search { DWORD pid; Tree tree; } search{pid, {}};
    EnumWindows([](HWND owner, LPARAM data) -> BOOL {
        auto& s = *reinterpret_cast<Search*>(data);
        if (process(owner) != s.pid || (!isClass(owner, L"WorkerW") && !isClass(owner, L"Progman"))) return TRUE;
        const auto view = FindWindowExW(owner, nullptr, L"SHELLDLL_DefView", nullptr);
        if (!view || process(view) != s.pid) return TRUE;
        const auto worker = GetWindow(owner, GW_HWNDNEXT);
        if (worker && isClass(worker, L"WorkerW") && process(worker) == s.pid
            && IsWindowVisible(worker) && !FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr)) {
            s.tree = {worker, view, worker}; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.tree;
}
}
struct DesktopLayer::Impl {
    DesktopLayer* owner;
    Tree tree;
    HWINEVENTHOOK hook{};
    UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    bool queued = false;
    DWORD requestedPid = 0;
    int retries = 0;
    std::unique_ptr<QWindow> foreign;
    std::vector<QPointer<QWindow>> windows;
    static inline std::vector<Impl*> active;
    explicit Impl(DesktopLayer* value) : owner(value) { active.push_back(this); }
    void detach() {
        for (auto& window : windows) if (window) {
            window->hide(); window->destroy(); window->setParent(nullptr);
        }
        foreign.reset();
    }
    void schedule() {
        if (queued) return;
        queued = true;
        QTimer::singleShot(80, owner, [this] {
            queued = false;
            const auto next = discover();
            bool misplaced = false;
            for (const auto& window : windows) if (window && window->isVisible()) {
                const auto hwnd = reinterpret_cast<HWND>(window->winId());
                misplaced = misplaced || !IsWindow(hwnd) || GetParent(hwnd) != next.parent
                    || (next.parent != next.wallpaper && (!below(hwnd, next.icons) || !below(next.wallpaper, hwnd)));
            }
            if (!(next == tree) || misplaced) {
                detach(); tree = next;
                if (owner->changed) owner->changed();
            }
            // A finite readiness burst, only after shell events; idle has no timer.
            if (!tree.parent && retries > 0) { --retries; QTimer::singleShot(500, owner, [this] { schedule(); }); }
        });
    }
    static void CALLBACK event(HWINEVENTHOOK hook, DWORD, HWND window, LONG object, LONG child, DWORD, DWORD) {
        if (object != OBJID_WINDOW || child != CHILDID_SELF) return;
        const auto found = std::find_if(active.begin(), active.end(), [hook](const auto* item) { return item->hook == hook; });
        if (found == active.end()) return;
        auto* current = *found;
        const bool tracked = window == current->tree.parent || window == current->tree.icons || window == current->tree.wallpaper;
        if (!tracked && process(window) != process(GetShellWindow())) return;
        if (tracked
            || isClass(window, L"Progman") || isClass(window, L"WorkerW") || isClass(window, L"SHELLDLL_DefView")) {
            current->retries = 10; current->schedule();
        }
    }
};
DesktopLayer::DesktopLayer(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {
    qGuiApp->installNativeEventFilter(this);
    impl_->hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_REORDER, nullptr, Impl::event,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}
DesktopLayer::~DesktopLayer() {
    qGuiApp->removeNativeEventFilter(this);
    if (impl_->hook) UnhookWinEvent(impl_->hook);
    std::erase(Impl::active, impl_.get());
    impl_->detach();
}
bool DesktopLayer::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    const auto* msg = static_cast<MSG*>(message);
    if (msg->message == impl_->taskbarCreated || msg->message == WM_DISPLAYCHANGE || msg->message == WM_THEMECHANGED) {
        impl_->retries = 10; impl_->schedule();
    }
    return false;
}
bool DesktopLayer::place(QWindow* window, QScreen* screen, const QRect& geometry) {
    if (!window || !screen || !impl_->hook) return false;
    auto tree = discover();
    if (!tree.parent) {
        const auto progman = FindWindowW(L"Progman", nullptr);
        const auto pid = process(progman);
        if (pid && pid == process(GetShellWindow()) && impl_->requestedPid != pid) {
            impl_->requestedPid = pid;
            DWORD_PTR result{};
            // Undocumented, bounded request, once per Explorer generation.
            SendMessageTimeoutW(progman, 0x052c, 0xD, 1, SMTO_ABORTIFHUNG | SMTO_BLOCK, 250, &result);
            tree = discover();
        }
    }
    if (!tree.parent) { window->hide(); impl_->retries = 10; impl_->schedule(); return false; }
    if (!(impl_->tree == tree)) { impl_->detach(); impl_->tree = tree; }
    if (!impl_->foreign) impl_->foreign.reset(QWindow::fromWinId(reinterpret_cast<WId>(tree.parent)));
    if (!impl_->foreign) return false;
    std::erase_if(impl_->windows, [](const auto& item) { return item.isNull(); });
    if (std::none_of(impl_->windows.begin(), impl_->windows.end(), [window](const auto& item) { return item == window; }))
        impl_->windows.push_back(window);
    if (window->parent() != impl_->foreign.get()) {
        window->hide(); window->setScreen(screen);
        const auto before = reinterpret_cast<HWND>(window->winId());
        // SetParent can reset DPI awareness across processes; reject mismatch
        // before calling it, rather than repairing the process afterwards.
        if (!AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(before), GetWindowDpiAwarenessContext(tree.parent))) { qWarning() << "Desktop DPI preflight failed"; return false; }
        window->setParent(impl_->foreign.get());
    }
    window->resize(geometry.size());
    // Showing a Qt child can apply its pending geometry and raise it. Finish
    // that transition before the final native placement, not afterwards in QML.
    window->show();
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (GetParent(hwnd) != tree.parent || process(tree.parent) != process(GetShellWindow())
        || !AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(hwnd), GetWindowDpiAwarenessContext(tree.parent))) {
        qWarning() << "Desktop parent/DPI validation failed" << (GetParent(hwnd) == tree.parent);
        window->hide(); return false;
    }
    // Qt global coordinates use monitor-local DIP with native screen origins.
    // Convert within the selected monitor, then into the shell parent's client.
    const auto origin = screen->geometry().topLeft();
    const auto scale = screen->devicePixelRatio();
    struct Target { QPoint origin; QSize pixels; RECT rectangle{}; bool found = false; } target{
        origin, QSize(qRound(screen->geometry().width() * scale), qRound(screen->geometry().height() * scale))};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
        auto& target = *reinterpret_cast<Target*>(data);
        MONITORINFOEXW info{}; info.cbSize = sizeof(info);
        // QScreen::name can be the display's friendly EDID name, not DISPLAYn.
        // Qt on Windows preserves native origins and scales only screen sizes.
        if (GetMonitorInfoW(monitor, &info) && info.rcMonitor.left == target.origin.x()
            && info.rcMonitor.top == target.origin.y()
            && info.rcMonitor.right - info.rcMonitor.left == target.pixels.width()
            && info.rcMonitor.bottom - info.rcMonitor.top == target.pixels.height()) {
            target.rectangle = info.rcMonitor; target.found = true; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&target));
    if (!target.found) { qWarning() << "Desktop monitor validation failed" << origin << target.pixels << screen->name(); window->hide(); return false; }
    POINT point{target.rectangle.left + qRound((geometry.x() - origin.x()) * scale),
        target.rectangle.top + qRound((geometry.y() - origin.y()) * scale)};
    if (!ScreenToClient(tree.parent, &point)) { qWarning() << "Desktop coordinate conversion failed"; window->hide(); return false; }
    const auto style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    constexpr LONG_PTR flags = WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | flags);
    // The previous style may be zero. Qt's synchronous style-change handler can
    // change last-error on success, so verify the actual resulting style.
    if ((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & flags) != flags) { window->hide(); return false; }
    const bool modern = tree.parent != tree.wallpaper;
    if (!SetWindowPos(hwnd, modern ? tree.icons : HWND_BOTTOM, point.x, point.y,
        qRound(geometry.width() * scale), qRound(geometry.height() * scale), SWP_NOACTIVATE | SWP_FRAMECHANGED)) {
        qWarning() << "Desktop SetWindowPos failed:" << GetLastError(); window->hide(); return false;
    }
    if (modern && (!below(hwnd, tree.icons) || !below(tree.wallpaper, hwnd))) {
        qWarning() << "Desktop sibling validation failed:" << below(hwnd, tree.icons) << below(tree.wallpaper, hwnd);
        window->hide(); return false;
    }
    return true;
}
}
