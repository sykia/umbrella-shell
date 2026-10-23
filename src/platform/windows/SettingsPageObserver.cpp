#include <Windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include "SettingsPageObserver.h"
#include "core/settings/SettingsPagePolicy.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>

namespace umbrella::platform {
using Microsoft::WRL::ComPtr;
namespace {
bool systemProcess(HWND window, const wchar_t* relative) {
    DWORD pid{}; GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    wchar_t path[32768]{}, windows[MAX_PATH]{}; DWORD size = 32768;
    const bool read = QueryFullProcessImageNameW(process, 0, path, &size) != FALSE;
    CloseHandle(process);
    const auto count = GetWindowsDirectoryW(windows, MAX_PATH);
    if (!read || !count || count >= MAX_PATH) return false;
    return _wcsicmp(path, (std::wstring(windows) + relative).c_str()) == 0;
}
}
// Shared with the GUI adapter. A frame-host alone is insufficient evidence.
bool isSettingsWindow(HWND window) {
    if (!window || !IsWindow(window) || !IsWindowVisible(window) || IsIconic(window)) return false;
    if (systemProcess(window, L"\\ImmersiveControlPanel\\SystemSettings.exe")) return true;
    if (!systemProcess(window, L"\\System32\\ApplicationFrameHost.exe")) return false;
    struct Search { bool found{}; int count{}; } search;
    EnumChildWindows(window, [](HWND child, LPARAM data) -> BOOL {
        auto& s = *reinterpret_cast<Search*>(data);
        if (++s.count > 256) return FALSE;
        if (systemProcess(child, L"\\ImmersiveControlPanel\\SystemSettings.exe")) { s.found = true; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.found;
}
bool settingsClient(HWND window, settings::Rect& result) {
    RECT rect{}; POINT origin{};
    if (!GetClientRect(window, &rect) || !ClientToScreen(window, &origin)) return false;
    result = {origin.x, origin.y, origin.x + rect.right, origin.y + rect.bottom};
    return result.valid();
}
namespace {
void send(const QJsonObject& value) {
    const auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
    fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout); fflush(stdout);
}
class Events final : public IUIAutomationStructureChangedEventHandler,
                     public IUIAutomationPropertyChangedEventHandler,
                     public IUIAutomationEventHandler {
    std::atomic<ULONG> references_{1};
public:
    std::atomic<bool> dirty{false};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (id == __uuidof(IUnknown) || id == __uuidof(IUIAutomationStructureChangedEventHandler)) *out = static_cast<IUIAutomationStructureChangedEventHandler*>(this);
        else if (id == __uuidof(IUIAutomationPropertyChangedEventHandler)) *out = static_cast<IUIAutomationPropertyChangedEventHandler*>(this);
        else if (id == __uuidof(IUIAutomationEventHandler)) *out = static_cast<IUIAutomationEventHandler*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto n = --references_; if (!n) delete this; return n; }
    void invalidate() {
        if (!dirty.exchange(true)) QMetaObject::invokeMethod(QCoreApplication::instance(), [] { send({{"invalidate", true}}); }, Qt::QueuedConnection);
    }
    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(IUIAutomationElement*, StructureChangeType, SAFEARRAY*) override { invalidate(); return S_OK; }
    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement*, PROPERTYID, VARIANT) override { invalidate(); return S_OK; }
    HRESULT STDMETHODCALLTYPE HandleAutomationEvent(IUIAutomationElement*, EVENTID) override { invalidate(); return S_OK; }
};
std::wstring stringProperty(IUIAutomationElement* element, PROPERTYID id) {
    VARIANT value{};
    if (FAILED(element->GetCachedPropertyValue(id, &value))) return {};
    std::wstring text;
    if (value.vt == VT_BSTR && value.bstrVal && SysStringLen(value.bstrVal) <= 256) text.assign(value.bstrVal, SysStringLen(value.bstrVal));
    VariantClear(&value); return text;
}
bool snapshot(IUIAutomation* automation, IUIAutomationElement* root, std::vector<settings::Node>& nodes) {
    ComPtr<IUIAutomationTreeWalker> walker;
    ComPtr<IUIAutomationCacheRequest> cache;
    if (FAILED(automation->get_ControlViewWalker(&walker)) || FAILED(automation->CreateCacheRequest(&cache))) return false;
    if (FAILED(cache->put_TreeScope(TreeScope_Element))) return false;
    for (auto id : {UIA_AutomationIdPropertyId, UIA_NamePropertyId, UIA_ControlTypePropertyId,
                    UIA_BoundingRectanglePropertyId, UIA_IsOffscreenPropertyId, UIA_IsScrollPatternAvailablePropertyId})
        if (FAILED(cache->AddProperty(id))) return false;
    ComPtr<IUIAutomationElement> cachedRoot;
    if (FAILED(root->BuildUpdatedCache(cache.Get(), &cachedRoot))) return false;
    struct Item { ComPtr<IUIAutomationElement> element; int parent; int depth; };
    std::vector<Item> pending{{cachedRoot, -1, 0}};
    QElapsedTimer budget; budget.start();
    // Both traversal and provider transactions are bounded; timeout means no overlay.
    for (size_t i = 0; i < pending.size(); ++i) {
        if (pending.size() > 256 || budget.elapsed() > 350 || pending[i].depth > 24) return false;
        auto element = pending[i].element;
        RECT rect{}; BOOL offscreen = TRUE; CONTROLTYPEID type{}; VARIANT scroll{};
        if (FAILED(element->get_CachedBoundingRectangle(&rect)) || FAILED(element->get_CachedIsOffscreen(&offscreen))
            || FAILED(element->get_CachedControlType(&type)) || FAILED(element->GetCachedPropertyValue(UIA_IsScrollPatternAvailablePropertyId, &scroll))) return false;
        const auto name = stringProperty(element.Get(), UIA_NamePropertyId);
        nodes.push_back({pending[i].parent, stringProperty(element.Get(), UIA_AutomationIdPropertyId),
            {rect.left, rect.top, rect.right, rect.bottom}, !offscreen,
            type == UIA_TextControlTypeId && (name == L"Панель задач" || name == L"Taskbar"),
            scroll.vt == VT_BOOL && scroll.boolVal == VARIANT_TRUE,
            type == UIA_EditControlTypeId || type == UIA_TreeControlTypeId});
        VariantClear(&scroll);
        ComPtr<IUIAutomationElement> child;
        if (FAILED(walker->GetFirstChildElementBuildCache(element.Get(), cache.Get(), &child))) return false;
        while (child) {
            if (pending.size() >= 256 || budget.elapsed() > 350) return false;
            pending.push_back({child, static_cast<int>(i), pending[i].depth + 1});
            ComPtr<IUIAutomationElement> next;
            if (FAILED(walker->GetNextSiblingElementBuildCache(child.Get(), cache.Get(), &next))) return false;
            child = std::move(next);
        }
    }
    return true;
}
}
int inspectSettingsPage() {
    if (!SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
        || FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 1;
    struct ComScope { ~ComScope() { CoUninitialize(); } } scope;
    struct Search { std::vector<HWND> windows; unsigned count{}; } search;
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto& s = *reinterpret_cast<Search*>(data);
        if (++s.count > 512) return FALSE;
        if (isSettingsWindow(window)) s.windows.push_back(window);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    QJsonObject report{{"settingsWindows", static_cast<int>(search.windows.size())}, {"enumeratedWindows", static_cast<int>(search.count)}};
    if (search.windows.size() != 1) { report["result"] = "no-unique-visible-settings-window"; send(report); return 2; }
    const auto window = search.windows.front();
    report["foreground"] = GetAncestor(GetForegroundWindow(), GA_ROOT) == window;
    report["window"] = QString::number(reinterpret_cast<quintptr>(window), 16);
    ComPtr<IUIAutomation2> automation;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation)))
        || FAILED(automation->put_ConnectionTimeout(250)) || FAILED(automation->put_TransactionTimeout(500))) return 1;
    ComPtr<IUIAutomationElement> root;
    if (FAILED(automation->ElementFromHandle(window, &root))) { report["result"] = "uia-root-unavailable"; send(report); return 3; }
    settings::Rect client{}; std::vector<settings::Node> nodes; QElapsedTimer timer; timer.start();
    const bool complete = settingsClient(window, client) && snapshot(automation.Get(), root.Get(), nodes);
    report["complete"] = complete; report["elapsedMs"] = timer.elapsed();
    report["client"] = QJsonArray{client.left, client.top, client.right, client.bottom};
    QJsonArray tree;
    for (const auto& node : nodes) {
        const bool taskbarId = node.id.starts_with(L"SystemSettings_Taskbar_") || node.id.starts_with(L"SystemSettings_DesktopTaskbar_");
        tree.append(QJsonObject{{"parent", node.parent}, {"id", taskbarId ? QString::fromStdWString(node.id) : QString()},
            {"rect", QJsonArray{node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.bottom}},
            {"visible", node.visible}, {"title", node.title}, {"scroll", node.scroll}, {"navigation", node.navigation}});
    }
    report["nodes"] = tree;
    if (const auto bounds = settings::contentBounds(nodes, client, complete)) {
        report["result"] = "recognized";
        report["rect"] = QJsonArray{bounds->left, bounds->top, bounds->right, bounds->bottom};
    } else report["result"] = complete ? "unrecognized-tree" : "incomplete-tree";
    send(report); return 0;
}
int runSettingsPageObserver() {
    if (!SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return 1;
    // This is a QCoreApplication process, never a GUI/STA thread.
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 1;
    struct ComScope { ~ComScope() { CoUninitialize(); } } scope;
    ComPtr<IUIAutomation2> automation;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation)))) return 1;
    if (FAILED(automation->put_ConnectionTimeout(250)) || FAILED(automation->put_TransactionTimeout(500))) return 1;
    ComPtr<Events> events; events.Attach(new Events);
    ComPtr<IUIAutomationElement> subscribed;
    quintptr subscribedWindow{};
    QObject context;
    std::atomic<bool> commandPending{false};
    // Only one outstanding request is accepted. The GUI coalesces later requests.
    std::thread reader([&context, &commandPending, &automation, &events, &subscribed, &subscribedWindow] {
        char line[128]{};
        while (fgets(line, sizeof(line), stdin)) {
            const QByteArray bytes(line);
            if (!bytes.endsWith('\n') || commandPending.exchange(true)) break;
            QMetaObject::invokeMethod(&context, [&, bytes] {
                const auto parts = bytes.trimmed().split(' ');
                bool epochOk = false, windowOk = false;
                const auto epoch = parts.value(0).toULongLong(&epochOk);
                const auto value = parts.value(1).toULongLong(&windowOk, 16);
                const auto window = reinterpret_cast<HWND>(static_cast<quintptr>(value));
                QJsonObject reply{{"epoch", QString::number(epoch)}, {"window", QString::number(value, 16)}};
                if (parts.size() == 2 && epochOk && windowOk && GetAncestor(GetForegroundWindow(), GA_ROOT) == window && isSettingsWindow(window)) {
                    ComPtr<IUIAutomationElement> root;
                    if (SUCCEEDED(automation->ElementFromHandle(window, &root))) {
                        if (subscribedWindow != value) {
                            automation->RemoveAllEventHandlers(); subscribed.Reset(); subscribedWindow = 0;
                            PROPERTYID properties[]{UIA_BoundingRectanglePropertyId, UIA_NamePropertyId, UIA_IsOffscreenPropertyId};
                            const bool attached = SUCCEEDED(automation->AddStructureChangedEventHandler(root.Get(), TreeScope_Subtree, nullptr, events.Get()))
                                && SUCCEEDED(automation->AddPropertyChangedEventHandlerNativeArray(root.Get(), TreeScope_Subtree, nullptr, events.Get(), properties, 3))
                                && SUCCEEDED(automation->AddAutomationEventHandler(UIA_LayoutInvalidatedEventId, root.Get(), TreeScope_Subtree, nullptr, events.Get()));
                            if (attached) { subscribed = root; subscribedWindow = static_cast<quintptr>(value); }
                            else automation->RemoveAllEventHandlers();
                        }
                        events->dirty = false;
                        std::vector<settings::Node> nodes; settings::Rect client{};
                        const bool complete = subscribed && settingsClient(window, client) && snapshot(automation.Get(), root.Get(), nodes);
                        if (const auto rect = settings::contentBounds(nodes, client, complete); rect && !events->dirty.load())
                            reply["rect"] = QJsonArray{rect->left, rect->top, rect->right, rect->bottom};
                    }
                } else {
                    automation->RemoveAllEventHandlers(); subscribed.Reset(); subscribedWindow = 0;
                }
                commandPending = false;
                send(reply);
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(&context, [] { QCoreApplication::quit(); }, Qt::QueuedConnection);
    });
    // The reader normally exits on pipe EOF before app.quit. Cancel a pending read
    // as well if the event loop stops for a different reason.
    const int result = QCoreApplication::exec();
    CancelSynchronousIo(reader.native_handle());
    reader.join();
    automation->RemoveAllEventHandlers();
    fflush(stdout);
    return result;
}
}
