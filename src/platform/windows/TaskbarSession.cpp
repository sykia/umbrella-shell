#include "TaskbarSession.h"
#include "DesktopBar.h"
#include "TaskbarVisibility.h"
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QUuid>
#include <windows.h>
#include <shellapi.h>
namespace umbrella::platform {
namespace {
struct Close { void operator()(void* handle) const { if (handle) CloseHandle(handle); } };
using Handle = std::unique_ptr<void, Close>;
constexpr UINT barCallback = WM_APP + 31;
}
struct TaskbarSession::Impl {
    Handle instance, ready, stop, arm, applied;
    QProcess guard;
    unsigned original = 0;
    bool active = false, hotkey = false, installed = false, acquired = false;
};
TaskbarSession::TaskbarSession(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>()) {
    connect(&impl_->guard, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (impl_->active) emit recoveryRequested();
    });
}
TaskbarSession::~TaskbarSession() {
    restore();
    if (impl_->hotkey) UnregisterHotKey(nullptr, 1);
    if (impl_->installed) QCoreApplication::instance()->removeNativeEventFilter(this);
    if (impl_->acquired) ReleaseMutex(impl_->instance.get());
}
bool TaskbarSession::acquire() {
    impl_->instance.reset(CreateMutexW(nullptr, TRUE, L"Local\\UmbrellaShell.DesktopBar"));
    if (!impl_->instance || GetLastError() == ERROR_ALREADY_EXISTS) return false;
    impl_->acquired = true;
    QCoreApplication::instance()->installNativeEventFilter(this); impl_->installed = true;
    impl_->hotkey = RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x55) != FALSE;
    return true;
}
bool TaskbarSession::hotkeyAvailable() const { return impl_->hotkey; }
bool TaskbarSession::originalAutoHide() const { return (impl_->original & ABS_AUTOHIDE) != 0; }
bool TaskbarSession::prepare() {
    if (!restore()) return false;
    impl_->original = taskbarState();
    const auto token = QStringLiteral("Local\\UmbrellaShell.") + QUuid::createUuid().toString(QUuid::Id128);
    const QString readyName = token + ".ready", stopName = "Local\\UmbrellaShell.Recovery";
    impl_->ready.reset(CreateEventW(nullptr, TRUE, FALSE, readyName.toStdWString().c_str()));
    impl_->stop.reset(CreateEventW(nullptr, TRUE, FALSE, stopName.toStdWString().c_str()));
    impl_->arm.reset(CreateEventW(nullptr, TRUE, FALSE, (readyName + ".arm").toStdWString().c_str()));
    impl_->applied.reset(CreateEventW(nullptr, TRUE, FALSE, (readyName + ".applied").toStdWString().c_str()));
    if (!impl_->ready || !impl_->stop || !impl_->arm || !impl_->applied) { restore(); return false; }
    impl_->guard.start(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
        {"--guard", QString::number(GetCurrentProcessId()), QString::number(impl_->original), readyName, stopName, "suppress"});
    if (!impl_->guard.waitForStarted(5000) || WaitForSingleObject(impl_->ready.get(), 5000) != WAIT_OBJECT_0) {
        restore(); return false;
    }
    return true;
}
bool TaskbarSession::hide() {
    impl_->active = true;
    if (!setAutoHide(true) || !SetEvent(impl_->arm.get())) return false;
    return WaitForSingleObject(impl_->applied.get(), 5000) == WAIT_OBJECT_0;
}
bool TaskbarSession::restore() {
    const bool wasActive = impl_->active; impl_->active = false;
    if (impl_->stop) SetEvent(impl_->stop.get());
    const bool stopped = impl_->guard.state() == QProcess::NotRunning || impl_->guard.waitForFinished(5000);
    const bool shown = !wasActive || restoreTaskbarWindows();
    const bool restored = !wasActive || setAutoHide(originalAutoHide());
    // Do not start another guardian while the old one can still restore stale state.
    if (stopped) { impl_->ready.reset(); impl_->stop.reset(); impl_->arm.reset(); impl_->applied.reset(); }
    return stopped && restored && shown;
}
bool TaskbarSession::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    auto* msg = static_cast<MSG*>(message);
    if (GetPropW(msg->hwnd, L"UmbrellaShell.AppBar")) {
        APPBARDATA data{sizeof(data)}; data.hWnd = msg->hwnd;
        if (msg->message == WM_ACTIVATE) SHAppBarMessage(ABM_ACTIVATE, &data);
        if (msg->message == WM_WINDOWPOSCHANGED) SHAppBarMessage(ABM_WINDOWPOSCHANGED, &data);
    }
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (msg->message == taskbarCreated) emit explorerRestarted();
    if (msg->message == WM_HOTKEY && msg->wParam == 1) emit recoveryRequested();
    if (msg->message == WM_QUERYENDSESSION || msg->message == WM_ENDSESSION) emit sessionEnding();
    if (msg->message == barCallback && msg->wParam == ABN_FULLSCREENAPP)
        SetWindowPos(msg->hwnd, msg->lParam ? HWND_BOTTOM : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (msg->message == barCallback && msg->wParam == ABN_POSCHANGED)
        emit positionChanged(reinterpret_cast<quintptr>(msg->hwnd));
    return false;
}
}
