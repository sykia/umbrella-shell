#include "TaskbarAppearance.h"
#include "TaskbarModuleSession.h"
#include "TaskbarWindows.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>
#include <Windows.h>
#include <shellapi.h>
#include <array>
#include <vector>
#include <algorithm>

namespace umbrella::platform {
namespace {
constexpr int maxWindows = 16;
struct Handle {
    HANDLE value{};
    ~Handle() { if (value) CloseHandle(value); }
};
struct LocationSetting { bool present{}; DWORD value{}; };
constexpr auto advancedKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
bool readLocationSetting(LocationSetting& setting) {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, advancedKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD type{}, bytes = sizeof(setting.value);
    const auto result = RegQueryValueExW(key, L"TaskbarLocation", nullptr, &type,
                                        reinterpret_cast<BYTE*>(&setting.value), &bytes);
    RegCloseKey(key);
    setting.present = result == ERROR_SUCCESS;
    return result == ERROR_FILE_NOT_FOUND || (result == ERROR_SUCCESS && type == REG_DWORD && bytes == sizeof(setting.value));
}
bool restoreLocationSetting(const LocationSetting& setting) {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, advancedKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;
    const auto result = setting.present ? RegSetValueExW(key, L"TaskbarLocation", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&setting.value), sizeof(setting.value)) : RegDeleteValueW(key, L"TaskbarLocation");
    RegCloseKey(key);
    return result == ERROR_SUCCESS || (!setting.present && result == ERROR_FILE_NOT_FOUND);
}
QString journalDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/TaskbarSessions";
}
quint64 creationTime(DWORD pid) {
    Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
    if (!process.value) return 0;
    std::array<wchar_t, 32768> path{};
    DWORD length = static_cast<DWORD>(path.size());
    std::array<wchar_t, MAX_PATH> windows{};
    const UINT windowsLength = GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
    if (!windowsLength || windowsLength >= windows.size() ||
        !QueryFullProcessImageNameW(process.value, 0, path.data(), &length)) return 0;
    const auto explorer = QString::fromWCharArray(windows.data()) + "\\explorer.exe";
    if (QString::compare(QString::fromWCharArray(path.data(), static_cast<int>(length)),
                         explorer, Qt::CaseInsensitive)) return 0;
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process.value, &created, &exited, &kernel, &user)) return 0;
    return (quint64(created.dwHighDateTime) << 32) | created.dwLowDateTime;
}
bool taskbar(HWND hwnd) {
    wchar_t name[64]{};
    if (!GetClassNameW(hwnd, name, 64)) return false;
    return !wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd");
}
struct Snapshot {
    HWND hwnd{}; DWORD pid{}; quint64 created{};
    bool layered{}; BYTE alpha = 255; COLORREF key{}; DWORD alphaFlags{};
    QByteArray region;
    RECT lastRect{}; bool regionApplied = false;
    int lastGap = -1;
    bool detached = false, islands = false;
};
bool valid(const Snapshot& s) {
    DWORD pid{};
    return taskbar(s.hwnd) && GetWindowThreadProcessId(s.hwnd, &pid) && pid == s.pid
        && s.created && creationTime(pid) == s.created;
}
bool capture(HWND hwnd, Snapshot& s) {
    s.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &s.pid);
    s.created = creationTime(s.pid);
    if (!s.created) return false;
    s.layered = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) != 0;
    if (s.layered && !GetLayeredWindowAttributes(hwnd, &s.key, &s.alpha, &s.alphaFlags))
        return false; // Per-pixel layered windows cannot be snapshotted safely here.
    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (!region) return false;
    bool ok = true;
    if (GetWindowRgn(hwnd, region) != ERROR) {
        const DWORD size = GetRegionData(region, 0, nullptr);
        if (!size || size > 65536) ok = false;
        else {
            s.region.resize(static_cast<int>(size));
            ok = GetRegionData(region, size, reinterpret_cast<RGNDATA*>(s.region.data())) == size;
        }
    }
    DeleteObject(region);
    return ok;
}
bool sendEdge(HWND hwnd, int edge) {
    DWORD_PTR result{};
    // Native Windows 11 taskbar location notification; undocumented, verified after use.
    return SendMessageTimeoutW(hwnd, 0x5ca, 6, edge, SMTO_ABORTIFHUNG, 1000, &result) != 0;
}
int currentEdge() {
    APPBARDATA data{}; data.cbSize = sizeof(data);
    return SHAppBarMessage(ABM_GETTASKBARPOS, &data) ? static_cast<int>(data.uEdge) : -1;
}
bool restoreSnapshots(const std::vector<Snapshot>& snapshots, int edge) {
    bool ok = true;
    const auto primary = FindWindowW(L"Shell_TrayWnd", nullptr);
    for (const auto& s : snapshots) {
        if (!valid(s)) continue; // Never touch a recycled HWND or a new Explorer process.
        // Re-sending the current edge can raise/rebuild the taskbar over a
        // fullscreen application. Restore it only when it actually changed.
        if (s.hwnd == primary && edge >= 0 && edge <= 3 && currentEdge()!=edge) ok = sendEdge(primary, edge) && ok;
        auto style = GetWindowLongPtrW(s.hwnd, GWL_EXSTYLE);
        if (s.layered) {
            SetLastError(0);
            const auto previous = SetWindowLongPtrW(s.hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
            if (!previous && GetLastError()) ok = false;
            ok = SetLayeredWindowAttributes(s.hwnd, s.key, s.alpha, s.alphaFlags) && ok;
        } else {
            SetLastError(0);
            const auto previous = SetWindowLongPtrW(s.hwnd, GWL_EXSTYLE, style & ~LONG_PTR(WS_EX_LAYERED));
            if (!previous && GetLastError()) ok = false;
        }
        HRGN region = s.region.isEmpty() ? nullptr : ExtCreateRegion(nullptr,
            static_cast<DWORD>(s.region.size()), reinterpret_cast<const RGNDATA*>(s.region.constData()));
        if (!s.region.isEmpty() && !region) { ok = false; continue; }
        if (!SetWindowRgn(s.hwnd, region, TRUE)) { if (region) DeleteObject(region); ok = false; }
        RedrawWindow(s.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    return ok;
}
QJsonObject serialize(const Snapshot& s) {
    return {{"hwnd", QString::number(reinterpret_cast<quintptr>(s.hwnd), 16)},
        {"pid", double(s.pid)}, {"created", QString::number(s.created, 16)},
        {"layered", s.layered}, {"alpha", int(s.alpha)}, {"key", double(s.key)},
        {"alphaFlags", double(s.alphaFlags)}, {"region", QString::fromLatin1(s.region.toBase64())}};
}
bool save(const QString& path, const std::vector<Snapshot>& snapshots, int edge, const LocationSetting& location, bool restored = false) {
    QJsonArray array;
    for (const auto& s : snapshots) array.append(serialize(s));
    QSaveFile file(path);
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"edge", edge},
        {"restored", restored}, {"windows", array},
        {"locationSetting", QJsonObject{{"present", location.present}, {"value", double(location.value)}}}}).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
bool restoreJournal(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) return false;
    QJsonParseError parse{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    file.close();
    if (parse.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto root = document.object();
    if (root["version"].toInt() != 1) return false;
    if (root["restored"].toBool()) return QFile::remove(path);
    const auto array = root["windows"].toArray();
    if (array.isEmpty() || array.size() > maxWindows) return false;
    std::vector<Snapshot> snapshots;
    for (const auto& value : array) {
        const auto obj = value.toObject();
        Snapshot s;
        s.hwnd = reinterpret_cast<HWND>(obj["hwnd"].toString().toULongLong(nullptr, 16));
        s.pid = static_cast<DWORD>(obj["pid"].toDouble());
        s.created = obj["created"].toString().toULongLong(nullptr, 16);
        s.layered = obj["layered"].toBool(); s.alpha = static_cast<BYTE>(obj["alpha"].toInt(255));
        s.key = static_cast<COLORREF>(obj["key"].toDouble());
        s.alphaFlags = static_cast<DWORD>(obj["alphaFlags"].toDouble());
        s.region = QByteArray::fromBase64(obj["region"].toString().toLatin1());
        if (s.region.size() > 65536) return false;
        if (!s.region.isEmpty()) {
            if (s.region.size() < int(sizeof(RGNDATAHEADER))) return false;
            const auto* region = reinterpret_cast<const RGNDATA*>(s.region.constData());
            if (region->rdh.dwSize != sizeof(RGNDATAHEADER) || region->rdh.iType != RDH_RECTANGLES ||
                region->rdh.nCount > 4096 || sizeof(RGNDATAHEADER) + quint64(region->rdh.nCount) * sizeof(RECT)
                    != quint64(s.region.size())) return false;
        }
        snapshots.push_back(std::move(s));
    }
    const int edge = root["edge"].toInt(-1);
    if (root.contains("locationSetting")) {
        const auto setting = root["locationSetting"].toObject();
        const double value = setting["value"].toDouble(-1);
        if (!setting["present"].isBool() || value < 0 || value > 4294967295.0 || value != double(static_cast<DWORD>(value))) return false;
        if (!restoreLocationSetting({setting["present"].toBool(), static_cast<DWORD>(value)})) return false;
    }
    if (edge < 0 || edge > 3 || !restoreSnapshots(snapshots, edge)) return false;
    return QFile::remove(path);
}
}
struct TaskbarAppearance::Impl {
    std::vector<Snapshot> snapshots;
    int originalEdge = -1;
    LocationSetting originalLocation;
    QString journal, stopName;
    HANDLE stop{};
    QProcess watcher;
    TaskbarModuleSession module;
    QObject eventContext;
    HWINEVENTHOOK locationHook{};
    bool detached{}, islands{}, refreshQueued{};
    inline static Impl* active{};
    void updateRegions() {
        for (auto& snapshot : snapshots) {
            if (snapshot.islands) continue;
            if (!valid(snapshot)) continue;
            RECT rect{};
            if (!GetWindowRect(snapshot.hwnd, &rect)) continue;
            const int width = rect.right - rect.left, height = rect.bottom - rect.top;
            const UINT dpi = GetDpiForWindow(snapshot.hwnd);
            const int inset = MulDiv(6, dpi ? static_cast<int>(dpi) : 96, 96);
            MONITORINFO monitor{sizeof(monitor)};
            const bool fullyVisible = GetMonitorInfoW(MonitorFromWindow(snapshot.hwnd, MONITOR_DEFAULTTONEAREST), &monitor)
                && rect.left >= monitor.rcMonitor.left && rect.top >= monitor.rcMonitor.top
                && rect.right <= monitor.rcMonitor.right && rect.bottom <= monitor.rcMonitor.bottom;
            const int gap = snapshot.detached && fullyVisible && width > 2 * inset && height > 2 * inset ? inset : 0;
            if (snapshot.regionApplied && snapshot.lastGap == gap &&
                snapshot.lastRect.right - snapshot.lastRect.left == width &&
                snapshot.lastRect.bottom - snapshot.lastRect.top == height) continue;
            snapshot.lastRect = rect; snapshot.lastGap = gap; snapshot.regionApplied = true;
            HRGN region = gap ? CreateRectRgn(gap, gap, width - gap, height - gap) : nullptr;
            if (gap && !region) { snapshot.regionApplied = false; continue; }
            if (!SetWindowRgn(snapshot.hwnd, region, TRUE)) {
                if (region) DeleteObject(region);
                snapshot.regionApplied = false;
            }
        }
    }
    static void CALLBACK locationChanged(HWINEVENTHOOK, DWORD, HWND hwnd, LONG object, LONG child, DWORD, DWORD) {
        auto* self = active;
        if (!self || object != OBJID_WINDOW || child != CHILDID_SELF || self->refreshQueued) return;
        bool ours = false;
        for (const auto& snapshot : self->snapshots) if (snapshot.hwnd == hwnd) { ours = true; break; }
        if (!ours) return;
        self->refreshQueued = true;
        QMetaObject::invokeMethod(&self->eventContext, [self] {
            self->refreshQueued = false;
            if (active == self) self->updateRegions();
        }, Qt::QueuedConnection);
    }
    void unhook() {
        if (locationHook) UnhookWinEvent(locationHook);
        locationHook = nullptr;
        if (active == this) active = nullptr;
    }
    ~Impl() { unhook(); if (stop) CloseHandle(stop); }
};
TaskbarAppearance::TaskbarAppearance() : impl_(std::make_unique<Impl>()) {}
TaskbarAppearance::~TaskbarAppearance() {
    QString ignored;
    if (!restore(ignored)) {
        // Keep QProcess alive until OS teardown so its destructor cannot kill recovery.
        impl_->unhook();
        (void)impl_.release();
    }
}

bool TaskbarAppearance::apply(const QString& edge, bool detached, int transparency, int widthPercent, int thickness, bool islands, QString& error) {
    const int requested = edge == "left" ? ABE_LEFT : edge == "top" ? ABE_TOP
        : edge == "right" ? ABE_RIGHT : edge == "bottom" ? ABE_BOTTOM : -1;
    if (requested < 0 || transparency < 0 || transparency > 100 || widthPercent < 40 || widthPercent > 100 || thickness < 48 || thickness > 96) {
        error = QStringLiteral("Недопустимые параметры панели."); return false;
    }
    const bool vertical = requested == ABE_LEFT || requested == ABE_RIGHT;
    if (vertical && !TaskbarModuleSession::layoutAvailable()) {
        error = QStringLiteral("Боковая раскладка не проверена для этой сборки Windows."); return false;
    }
    auto& d = *impl_;
    if (d.stop && d.watcher.state() == QProcess::NotRunning) {
        error = QStringLiteral("Процесс восстановления завершился. Сначала верните исходный вид."); return false;
    }
    if (d.originalEdge < 0) {
        if (!readLocationSetting(d.originalLocation)) {
            error = QStringLiteral("Не удалось сохранить настройку расположения панели."); return false;
        }
        d.originalEdge = currentEdge();
    }
    if (d.originalEdge < 0) { error = QStringLiteral("Панель Windows недоступна."); return false; }
    const auto windows = taskbarWindows();
    if (windows.empty() || windows.size() > maxWindows) {
        error = QStringLiteral("Недопустимое количество системных панелей."); return false;
    }
    bool snapshotAdded = false;
    for (const auto hwnd : windows) {
        bool found = false;
        for (const auto& s : d.snapshots) if (s.hwnd == hwnd && valid(s)) { found = true; break; }
        if (found) continue;
        Snapshot s;
        if (d.snapshots.size() >= maxWindows || !capture(hwnd, s)) {
            error = QStringLiteral("Не удалось сохранить исходное оформление панели."); return false;
        }
        d.snapshots.push_back(std::move(s));
        snapshotAdded = true;
    }
    if (d.journal.isEmpty()) {
        const auto directory = journalDirectory();
        if (!QDir().mkpath(directory)) { error = QStringLiteral("Не удалось создать журнал восстановления."); return false; }
        const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        d.journal = directory + "/" + id + ".json";
        d.stopName = "Local\\UmbrellaTaskbar-" + id;
    }
    if ((snapshotAdded || !d.stop) && !save(d.journal, d.snapshots, d.originalEdge, d.originalLocation)) {
        error = QStringLiteral("Не удалось сохранить журнал восстановления."); return false;
    }
    if (!d.stop) {
        d.stop = CreateEventW(nullptr, TRUE, FALSE, reinterpret_cast<LPCWSTR>(d.stopName.utf16()));
        if (!d.stop) { error = QStringLiteral("Не удалось подготовить восстановление."); return false; }
        d.watcher.setProgram(QCoreApplication::applicationFilePath());
        d.watcher.setArguments({"--taskbar-watch", QString::number(QCoreApplication::applicationPid()), d.journal, d.stopName});
        d.watcher.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            args->flags |= CREATE_NO_WINDOW;
        });
        d.watcher.start();
        if (!d.watcher.waitForStarted(2000) || (d.watcher.bytesAvailable() == 0 && !d.watcher.waitForReadyRead(2000)) ||
            d.watcher.readLine().trimmed() != "ready") {
            error = QStringLiteral("Процесс восстановления не готов; панель не изменена.");
            SetEvent(d.stop); d.watcher.waitForFinished(2000);
            CloseHandle(d.stop); d.stop = nullptr;
            return false;
        }
    }
    for (const auto hwnd : windows) {
        bool identityValid = false;
        for (const auto& snapshot : d.snapshots) if (snapshot.hwnd == hwnd && valid(snapshot)) identityValid = true;
        if (!identityValid) { error = QStringLiteral("Системная панель была пересоздана; повторите применение."); break; }
        RECT rect{};
        if (!GetWindowRect(hwnd, &rect)) { error = QStringLiteral("Не удалось прочитать размер панели."); break; }
        const auto style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        SetLastError(0);
        const auto previous = SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
        if ((!previous && GetLastError()) || !SetLayeredWindowAttributes(hwnd, 0,
            static_cast<BYTE>((255 * (100 - transparency) + 50) / 100), LWA_ALPHA)) {
            error = QStringLiteral("Windows отклонила прозрачность панели."); break;
        }
    }
    for (const auto hwnd : windows) {
        BYTE alpha{}; COLORREF key{}; DWORD flags{};
        const BYTE expected = static_cast<BYTE>((255 * (100 - transparency) + 50) / 100);
        if (!GetLayeredWindowAttributes(hwnd, &key, &alpha, &flags) || alpha != expected || !(flags & LWA_ALPHA))
            error = QStringLiteral("Windows не подтвердила прозрачность панели.");
    }
    if (error.isEmpty() && !d.module.apply(detached ? widthPercent : 100, edge, thickness, islands, detached, error)) {
        QString ignored; restore(ignored); return false;
    }
    if (currentEdge() != requested) error = QStringLiteral("Windows не подтвердила расположение панели.");
    if (d.detached != detached || d.islands != islands)
        for (auto& snapshot : d.snapshots) snapshot.regionApplied = false;
    d.detached = detached;
    d.islands = islands;
    for (auto& snapshot : d.snapshots) { snapshot.detached = detached; snapshot.islands = islands; }
    d.updateRegions();
    for (const auto& snapshot : d.snapshots)
        if (!islands && valid(snapshot) && !snapshot.regionApplied) error = QStringLiteral("Windows отклонила форму панели.");
    if (!d.locationHook && error.isEmpty()) {
        Impl::active = &d;
        d.locationHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
            nullptr, Impl::locationChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (!d.locationHook) error = QStringLiteral("Не удалось включить обработку геометрии панели.");
    }
    if (!error.isEmpty()) { QString ignored; restore(ignored); return false; }
    return true;
}
bool TaskbarAppearance::applyMonitor(const QString& device, const QString& edge, bool detached, int transparency, int widthPercent, int thickness, bool islands, QString& error) {
    auto& d = *impl_;
    if (transparency < 0 || transparency > 100 || thickness < (islands ? 64 : 48) || thickness > 96) {
        error = QStringLiteral("Invalid monitor configuration."); return false;
    }
    for (auto& snapshot : d.snapshots) {
        if (!valid(snapshot)) continue;
        MONITORINFOEXW monitor{}; monitor.cbSize = sizeof(monitor);
        if (!GetMonitorInfoW(MonitorFromWindow(snapshot.hwnd, MONITOR_DEFAULTTONULL), &monitor) || device != QString::fromWCharArray(monitor.szDevice)) continue;
        if (!d.module.apply(detached ? widthPercent : 100, edge, thickness, islands, detached, error, device)) return false;
        if (!SetLayeredWindowAttributes(snapshot.hwnd, 0, static_cast<BYTE>((255 * (100 - transparency) + 50) / 100), LWA_ALPHA)) {
            error = QStringLiteral("Windows rejected monitor transparency."); return false;
        }
        snapshot.detached = detached; snapshot.islands = islands; snapshot.regionApplied = false;
        d.updateRegions();
        return islands || snapshot.regionApplied;
    }
    error = QStringLiteral("Monitor taskbar unavailable."); return false;
}
bool TaskbarAppearance::nativeSessionActive() const {return impl_->module.running();}
bool TaskbarAppearance::reorderTray(const QString& device,unsigned order,QString& error) {return impl_->module.reorderTray(device,order,error);}
bool TaskbarAppearance::placeComponents(const QByteArray& command,const QString& device,QString& error) {return impl_->module.placeComponents(command,device,error);}
void TaskbarAppearance::setComponentLossHandler(std::function<void(const QString&)> handler) {impl_->module.componentLost=std::move(handler);}
bool TaskbarAppearance::restore(QString& error) {
    auto& d = *impl_;
    const bool alive = std::any_of(d.snapshots.begin(), d.snapshots.end(), [](const Snapshot& value) { return valid(value); });
    if (!d.module.stop(error) && alive) return false;
    error.clear();
    if (d.snapshots.empty()) return true;
    d.unhook();
    if (!restoreLocationSetting(d.originalLocation) || !restoreSnapshots(d.snapshots, d.originalEdge)) {
        error = QStringLiteral("Восстановление панели не завершено; журнал сохранён."); return false;
    }
    if (!d.journal.isEmpty() && !save(d.journal, d.snapshots, d.originalEdge, d.originalLocation, true)) {
        error = QStringLiteral("Не удалось отметить восстановление в журнале."); return false;
    }
    if (d.stop) {
        SetEvent(d.stop); d.watcher.waitForFinished(2000);
        CloseHandle(d.stop); d.stop = nullptr;
    }
    d.snapshots.clear(); d.originalEdge = -1; d.journal.clear();
    return true;
}
int watchTaskbarAppearance(unsigned long parent, const QString& journal, const QString& stopEvent) {
    const auto expectedDirectory = QFileInfo(journalDirectory()).absoluteFilePath();
    const QFileInfo info(journal);
    if (info.absolutePath() != expectedDirectory || info.suffix() != "json") return 2;
    Handle process{OpenProcess(SYNCHRONIZE, FALSE, parent)};
    Handle stop{OpenEventW(SYNCHRONIZE, FALSE, reinterpret_cast<LPCWSTR>(stopEvent.utf16()))};
    if (!process.value || !stop.value) return 2;
    QTextStream(stdout) << "ready\n" << Qt::flush;
    const HANDLE handles[]{process.value, stop.value};
    if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_FAILED) return 1;
    return restoreJournal(journal) ? 0 : 1;
}
bool recoverTaskbarAppearance() {
    QDir directory(journalDirectory());
    bool ok = true;
    const auto files = directory.entryList({"*.json"}, QDir::Files);
    if (files.size() > 64) return false;
    for (const auto& file : files) ok = restoreJournal(directory.filePath(file)) && ok;
    return ok;
}
}
