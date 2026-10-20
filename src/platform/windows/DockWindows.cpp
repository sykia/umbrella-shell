#include "DockWindows.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QWindow>
#include <QColor>
#include <QThread>
#include <Windows.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <appmodel.h>
#include <roapi.h>
#include <wrl/client.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;
namespace umbrella::platform {
namespace {
DockWindows* owner = nullptr; // One event source per existing Umbrella process.
QString normalized(QString value) { return value.trimmed().toCaseFolded(); }
QImage iconImage(HICON icon) {
    if (!icon) return {};
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 64; info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    const HDC dc = CreateCompatibleDC(nullptr);
    const HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage result;
    if (dc && bitmap && bits) {
        const auto previous = SelectObject(dc, bitmap);
        ZeroMemory(bits, 64 * 64 * 4);
        if (DrawIconEx(dc, 0, 0, icon, 64, 64, 0, nullptr, DI_NORMAL)) {
            result = QImage(static_cast<uchar*>(bits), 64, 64, QImage::Format_ARGB32_Premultiplied).copy();
            // Legacy icons can have a zero alpha channel after GDI rendering.
            bool alpha = false;
            for (int y = 0; y < 64 && !alpha; ++y) for (int x = 0; x < 64; ++x) if (qAlpha(result.pixel(x,y))) { alpha = true; break; }
            if (!alpha) {
                memset(bits,0xff,64*64*4);
                if (DrawIconEx(dc,0,0,icon,64,64,0,nullptr,DI_MASK)) {
                    const QImage mask(static_cast<uchar*>(bits),64,64,QImage::Format_RGB32);
                    for (int y=0;y<64;++y) for (int x=0;x<64;++x)
                        result.setPixel(x,y,qRed(mask.pixel(x,y))>127 ? 0 : result.pixel(x,y) | 0xff000000);
                }
            }
        }
        SelectObject(dc, previous);
    }
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    return result;
}
QString readAppId(IPropertyStore* store) {
    PROPVARIANT value{};
    QString result;
    if (store && SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)) && value.vt == VT_LPWSTR && value.pwszVal)
        result = QString::fromWCharArray(value.pwszVal);
    PropVariantClear(&value);
    return result;
}
DockApp packagedApp(const QString& aumid) {
    DockApp app;
    if (aumid.isEmpty() || aumid.size()>512 || aumid.contains(QChar::Null)) return app;
    const auto parsingName=QStringLiteral("shell:AppsFolder\\")+aumid;
    ComPtr<IShellItem> item;
    if (FAILED(SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(parsingName.utf16()),nullptr,IID_PPV_ARGS(&item)))) return app;
    app.id="app:"+normalized(aumid); app.aumid=aumid; app.path=parsingName;
    PWSTR display=nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&display))) { app.title=QString::fromWCharArray(display); CoTaskMemFree(display); }
    if (app.title.isEmpty()) app.title=aumid;
    ComPtr<IShellItemImageFactory> images;
    HBITMAP bitmap=nullptr;
    if (SUCCEEDED(item.As(&images)) && SUCCEEDED(images->GetImage({64,64},SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK,&bitmap))) {
        BITMAP source{};
        if (GetObjectW(bitmap,sizeof(source),&source) && source.bmWidth>0 && source.bmHeight>0 && source.bmWidth<=256 && source.bmHeight<=256) {
            QImage image(source.bmWidth,source.bmHeight,QImage::Format_ARGB32_Premultiplied);
            BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=image.width(); info.bmiHeader.biHeight=-image.height();
            info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
            const HDC dc=CreateCompatibleDC(nullptr);
            if (dc && GetDIBits(dc,bitmap,0,image.height(),image.bits(),&info,DIB_RGB_COLORS)) app.icon=image;
            if (dc) DeleteDC(dc);
        }
        DeleteObject(bitmap);
    }
    return app;
}
bool eligible(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return false;
    const auto style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((style & WS_EX_TOOLWINDOW) && !(style & WS_EX_APPWINDOW)) return false;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || pid == GetCurrentProcessId()) return false;
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return false;
    wchar_t name[128]{}; GetClassNameW(hwnd, name, 128);
    return wcscmp(name, L"Shell_TrayWnd") && wcscmp(name, L"Shell_SecondaryTrayWnd") && wcscmp(name, L"Progman") && wcscmp(name, L"WorkerW") && GetWindowTextLengthW(hwnd) > 0;
}
quint64 creationTime(HANDLE process) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process,&created,&exited,&kernel,&user)) return 0;
    return (static_cast<quint64>(created.dwHighDateTime)<<32) | created.dwLowDateTime;
}
bool sameWindow(const DockWindow& window) {
    const auto hwnd = reinterpret_cast<HWND>(window.handle);
    if (!eligible(hwnd)) return false;
    DWORD pid=0; GetWindowThreadProcessId(hwnd,&pid);
    if (pid != window.processId || !window.processCreated) return false;
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if (!process) return false;
    const bool same = creationTime(process) == window.processCreated;
    CloseHandle(process); return same;
}
void CALLBACK changed(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD) {
    if (!owner || !hwnd || child != CHILDID_SELF) return;
    if (event != EVENT_SYSTEM_FOREGROUND && (object != OBJID_WINDOW || (event != EVENT_OBJECT_DESTROY && !eligible(hwnd)))) return;
    // OUTOFCONTEXT callbacks execute on the registering GUI thread.
    QMetaObject::invokeMethod(owner, "windowsChanged", Qt::QueuedConnection);
}
}
struct DockWindows::Impl {
    QList<HWINEVENTHOOK> hooks;
    mutable QHash<QString,DockApp> cache;
    bool com = false;
    QThread worker;
    QObject* workerContext = nullptr;
};
DockWindows::DockWindows(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>()) {
    impl_->com = SUCCEEDED(RoInitialize(RO_INIT_SINGLETHREADED));
}
DockWindows::~DockWindows() { stop(); if (impl_->com) RoUninitialize(); }
bool DockWindows::start() {
    if (!impl_->hooks.isEmpty()) return true;
    if (owner && owner != this) return false;
    owner = this;
    for (const auto range : {std::pair<DWORD,DWORD>{EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND},
         {EVENT_OBJECT_CREATE,EVENT_OBJECT_HIDE}, {EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_NAMECHANGE},
         {EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND}}) {
        const auto hook = SetWinEventHook(range.first, range.second, nullptr, changed, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (!hook) { stop(); return false; }
        impl_->hooks.append(hook);
    }
    QCoreApplication::instance()->installNativeEventFilter(this);
    impl_->workerContext = new QObject;
    impl_->workerContext->moveToThread(&impl_->worker);
    connect(&impl_->worker,&QThread::finished,impl_->workerContext,&QObject::deleteLater);
    impl_->worker.start();
    return true;
}
void DockWindows::stop() {
    ++generation_; snapshotPending_ = false; snapshotAgain_ = false;
    QCoreApplication::instance()->removeNativeEventFilter(this);
    for (auto hook : impl_->hooks) UnhookWinEvent(hook);
    impl_->hooks.clear();
    impl_->worker.quit(); impl_->worker.wait(); impl_->workerContext = nullptr;
    impl_->cache.clear();
    if (owner == this) owner = nullptr;
}
void DockWindows::requestSnapshot() {
    if (!impl_->workerContext) return;
    if (snapshotPending_) { snapshotAgain_ = true; return; }
    snapshotPending_ = true;
    const auto generation = generation_;
    QMetaObject::invokeMethod(impl_->workerContext,[this,generation] {
        const bool initialized = SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED));
        const auto snapshot = windows();
        if (initialized) RoUninitialize();
        QMetaObject::invokeMethod(this,[this,generation,snapshot] {
            if (generation != generation_) return;
            snapshotPending_ = false;
            const bool again = snapshotAgain_; snapshotAgain_ = false;
            emit snapshotReady(snapshot);
            if (again) requestSnapshot();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
DockApp DockWindows::application(const QString& path) {
    const auto prefix=QStringLiteral("shell:AppsFolder\\");
    if (path.startsWith(prefix,Qt::CaseInsensitive)) return packagedApp(path.mid(prefix.size()));
    DockApp app;
    const QFileInfo file(path);
    if (!file.isFile() || !QStringList{"exe","lnk","appref-ms"}.contains(file.suffix().toLower())) return app;
    app.path = file.absoluteFilePath(); app.title = file.completeBaseName();
    QString executable = app.path;
    if (file.suffix().compare("lnk",Qt::CaseInsensitive) == 0) {
        ComPtr<IShellLinkW> link;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))) {
            ComPtr<IPersistFile> persist;
            if (SUCCEEDED(link.As(&persist)) && SUCCEEDED(persist->Load(reinterpret_cast<LPCWSTR>(app.path.utf16()),STGM_READ))) {
                wchar_t target[32768]{};
                if (SUCCEEDED(link->GetPath(target,32768,nullptr,SLGP_RAWPATH)) && *target) executable = QString::fromWCharArray(target);
                ComPtr<IPropertyStore> store; if (SUCCEEDED(link.As(&store))) app.aumid = readAppId(store.Get());
            }
        }
    }
    app.id = app.aumid.isEmpty() ? "path:" + normalized(QDir::toNativeSeparators(executable)) : "app:" + normalized(app.aumid);
    app.executable = QDir::toNativeSeparators(executable);
    SHFILEINFOW info{};
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(app.path.utf16()),0,&info,sizeof(info),SHGFI_ICON | SHGFI_LARGEICON)) {
        app.icon = iconImage(info.hIcon); DestroyIcon(info.hIcon);
    }
    return app;
}
QList<DockApp> DockWindows::importPins() const {
    QList<DockApp> result;
    PWSTR roaming=nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,KF_FLAG_DEFAULT,nullptr,&roaming))) return result;
    const auto directory=QString::fromWCharArray(roaming)+"/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar";
    CoTaskMemFree(roaming);
    for (const auto& file : QDir(directory).entryInfoList({"*.lnk"},QDir::Files,QDir::Name)) {
        if (result.size() == 128) break;
        auto app = application(file.absoluteFilePath()); if (!app.id.isEmpty()) result.append(app);
    }
    return result;
}
QList<DockWindow> DockWindows::windows() const {
    struct Context { QList<DockWindow> result; const DockWindows* self; HWND active; } context{{},this,GetForegroundWindow()};
    EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
        auto& c = *reinterpret_cast<Context*>(data);
        if (c.result.size() >= 512) return FALSE;
        if (!eligible(hwnd)) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(hwnd,&pid);
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if (!process) return TRUE;
        wchar_t path[32768]{}; DWORD size = 32768;
        const bool havePath = QueryFullProcessImageNameW(process,0,path,&size);
        wchar_t model[512]{}; UINT32 count = 512;
        QString aumid; if (GetApplicationUserModelId(process,&count,model) == ERROR_SUCCESS) aumid = QString::fromWCharArray(model);
        const auto created=creationTime(process);
        CloseHandle(process);
        if (!havePath) return TRUE;
        ComPtr<IPropertyStore> store;
        if (SUCCEEDED(SHGetPropertyStoreForWindow(hwnd,IID_PPV_ARGS(&store)))) {
            auto windowId = readAppId(store.Get()); if (!windowId.isEmpty()) aumid = windowId;
        }
        auto executable = QString::fromWCharArray(path);
        // Frame-hosted UWP windows expose the packaged process through child HWNDs.
        // Use public process/AUMID APIs; never group all packages under the host exe.
        if (aumid.isEmpty() && QFileInfo(executable).fileName().compare("ApplicationFrameHost.exe",Qt::CaseInsensitive)==0) {
            struct ChildIdentity { DWORD host; int inspected=0; QString path, aumid; } identity{pid};
            EnumChildWindows(hwnd,[](HWND child,LPARAM data)->BOOL {
                auto& found=*reinterpret_cast<ChildIdentity*>(data);
                if (++found.inspected>64) return FALSE;
                DWORD childPid=0; GetWindowThreadProcessId(child,&childPid);
                if (!childPid || childPid==found.host) return TRUE;
                const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,childPid);
                if (!process) return TRUE;
                wchar_t app[512]{}, image[32768]{}; UINT32 count=512; DWORD size=32768;
                if (GetApplicationUserModelId(process,&count,app)==ERROR_SUCCESS && QueryFullProcessImageNameW(process,0,image,&size)) {
                    found.aumid=QString::fromWCharArray(app); found.path=QString::fromWCharArray(image);
                }
                CloseHandle(process); return found.aumid.isEmpty();
            },reinterpret_cast<LPARAM>(&identity));
            if (!identity.aumid.isEmpty()) { aumid=identity.aumid; executable=identity.path; }
        }
        const auto id = aumid.isEmpty() ? "path:" + normalized(executable) : "app:" + normalized(aumid);
        auto& cache = c.self->impl_->cache;
        if (!cache.contains(id)) {
            if (cache.size() >= 512) cache.clear();
            auto app = aumid.isEmpty() ? DockApp{} : packagedApp(aumid);
            if (app.id.isEmpty()) app = application(executable);
            if (app.path.isEmpty()) app.path=executable;
            if (app.title.isEmpty()) app.title=QFileInfo(executable).completeBaseName();
            app.executable=executable; app.id = id; app.aumid = aumid;
            DWORD_PTR icon = 0;
            if (SendMessageTimeoutW(hwnd,WM_GETICON,ICON_BIG,0,SMTO_ABORTIFHUNG,20,&icon) && icon) app.icon = iconImage(reinterpret_cast<HICON>(icon));
            cache.insert(id,app);
        }
        wchar_t title[1024]{}; GetWindowTextW(hwnd,title,1024);
        RECT r{}; GetWindowRect(hwnd,&r);
        MONITORINFOEXW monitor{}; monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
        DockWindow window; window.app = cache[id]; window.handle = reinterpret_cast<quintptr>(hwnd);
        window.processId=pid; window.processCreated=created;
        window.title = QString::fromWCharArray(title); window.screen = QString::fromWCharArray(monitor.szDevice);
        window.rect = QRect(r.left,r.top,r.right-r.left,r.bottom-r.top); window.active = hwnd == c.active;
        window.monitorRect = QRect(monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top);
        window.fullscreen = !IsIconic(hwnd) && r.left <= monitor.rcMonitor.left && r.top <= monitor.rcMonitor.top && r.right >= monitor.rcMonitor.right && r.bottom >= monitor.rcMonitor.bottom;
        c.result.append(window); return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    return context.result;
}
bool DockWindows::launch(const DockApp& app) {
    if (!app.aumid.isEmpty()) {
        ComPtr<IApplicationActivationManager> manager;
        DWORD pid = 0;
        if (SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager))) &&
            SUCCEEDED(manager->ActivateApplication(reinterpret_cast<LPCWSTR>(app.aumid.utf16()),nullptr,AO_NONE,&pid))) return true;
    }
    if (application(app.path).id.isEmpty()) return false;
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",reinterpret_cast<LPCWSTR>(app.path.utf16()),nullptr,nullptr,SW_SHOWNORMAL)) > 32;
}
bool DockWindows::activate(const DockWindow& window, bool minimizeActive) {
    auto hwnd = reinterpret_cast<HWND>(window.handle);
    if (!sameWindow(window)) return false;
    if (minimizeActive && hwnd == GetForegroundWindow()) return ShowWindowAsync(hwnd,SW_MINIMIZE) != FALSE;
    if (IsIconic(hwnd)) ShowWindowAsync(hwnd,SW_RESTORE);
    return SetForegroundWindow(hwnd) != FALSE;
}
bool DockWindows::close(const DockWindow& window) {
    const auto hwnd = reinterpret_cast<HWND>(window.handle);
    return sameWindow(window) && PostMessageW(hwnd,WM_CLOSE,0,0);
}
QVariantMap DockWindows::theme() {
    QColor background(Qt::white), foreground(Qt::black), accent(0,120,215);
    bool animations = true, transparency = false;
    try {
        const winrt::Windows::UI::ViewManagement::UISettings settings;
        const auto convert = [](auto c) { return QColor(c.R,c.G,c.B,c.A); };
        using Type = winrt::Windows::UI::ViewManagement::UIColorType;
        background = convert(settings.GetColorValue(Type::Background)); foreground = convert(settings.GetColorValue(Type::Foreground));
        accent = convert(settings.GetColorValue(Type::Accent)); animations = settings.AnimationsEnabled();
        transparency = settings.AdvancedEffectsEnabled();
    } catch (const winrt::hresult_error&) { }
    HIGHCONTRASTW high{sizeof(HIGHCONTRASTW)};
    const bool contrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0) && (high.dwFlags & HCF_HIGHCONTRASTON);
    if (contrast) {
        auto color = [](int index) { auto c = GetSysColor(index); return QColor(GetRValue(c),GetGValue(c),GetBValue(c)); };
        background = color(COLOR_WINDOW); foreground = color(COLOR_WINDOWTEXT); accent = color(COLOR_HIGHLIGHT);
    }
    return {{"background",background},{"foreground",foreground},{"accent",accent},{"dark",background.lightness() < 128},
        {"contrast",contrast},{"animations",animations && !contrast},{"transparency",transparency && !contrast}};
}
bool DockWindows::backdrop(QWindow& window, bool enabled, bool dark) {
    const auto hwnd = reinterpret_cast<HWND>(window.winId());
    const BOOL mode = dark;
    DwmSetWindowAttribute(hwnd,DWMWA_USE_IMMERSIVE_DARK_MODE,&mode,sizeof(mode));
    const DWM_SYSTEMBACKDROP_TYPE type = enabled ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
    return SUCCEEDED(DwmSetWindowAttribute(hwnd,DWMWA_SYSTEMBACKDROP_TYPE,&type,sizeof(type))) && enabled;
}
bool DockWindows::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    const auto msg = static_cast<MSG*>(message);
    if (msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED || msg->message == WM_DWMCOLORIZATIONCOLORCHANGED) emit themeChanged();
    return false;
}
}
