#include "DockController.h"
#include "core/DockPolicy.h"
#include "platform/windows/TaskbarRuntime.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QQuickImageProvider>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>

using umbrella::platform::DockWindows;
using umbrella::platform::DockApp;
namespace {
QVariantMap defaults() {
    return {{"edge","bottom"},{"alignment",1},{"margin",10},{"iconSize",48},{"spacing",8},{"radius",18},
        {"opacity",85},{"blur",true},{"indicators",true},{"magnification",20},{"hideMode",0},{"hideDelay",600},
        {"revealDelay",180},{"hideFullscreen",true},{"animations",true},{"speed",100},{"allScreens",false},
        {"screen",QString{}},{"showRunning",true},{"theme","auto"}};
}
bool validate(const QString& key, const QVariant& value) {
    const QHash<QString,QPair<int,int>> ranges{{"alignment",{0,2}},{"margin",{0,80}},{"iconSize",{24,80}},
        {"spacing",{0,24}},{"radius",{0,40}},{"opacity",{20,100}},{"magnification",{0,40}},
        {"hideMode",{0,2}},{"hideDelay",{100,3000}},{"revealDelay",{100,1000}},{"speed",{50,200}}};
    if (ranges.contains(key)) { bool ok = false; const int n = value.toInt(&ok); return ok && n >= ranges[key].first && n <= ranges[key].second; }
    if (key == "edge") return QStringList{"bottom","top","left","right"}.contains(value.toString());
    if (key == "theme") return QStringList{"auto","light","dark"}.contains(value.toString());
    if (key == "screen") return value.toString().size() <= 128;
    return QStringList{"blur","indicators","hideFullscreen","animations","allScreens","showRunning"}.contains(key) && value.metaType().id() == QMetaType::Bool;
}
QString token(const umbrella::platform::DockWindow& window) {
    return QString("%1:%2:%3").arg(window.handle).arg(window.processId).arg(window.processCreated);
}
bool matches(const DockApp& app, const DockApp& window) {
    return app.id==window.id || (window.aumid.isEmpty() && !app.executable.isEmpty() && app.executable.compare(window.executable,Qt::CaseInsensitive)==0);
}
class Icons final : public QQuickImageProvider {
public:
    explicit Icons(DockController* model) : QQuickImageProvider(QQuickImageProvider::Image), model_(model) {}
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        // Images are stored as model data, avoiding native icon work during rendering.
        const auto row = id.section('/',0,0).toInt();
        const auto result = model_->data(model_->index(row), Qt::UserRole+100).value<QImage>();
        if (size) *size = result.size();
        return result;
    }
private: DockController* model_;
};
}
struct DockController::Surface {
    std::unique_ptr<QQuickView> view;
    QScreen* screen = nullptr;
    QTimer* timer = nullptr;
    QRect shown;
    bool hidden = false, hovered = false, menu = false, wantHidden = false;
    int backdropState = -1;
};
DockController::DockController(QObject* parent) : QAbstractListModel(parent), source_(this), config_(defaults()), theme_(DockWindows::theme()) {
    QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    const auto bytes = saved.value("dock/configV1").toByteArray();
    if (bytes.size() <= 32768) {
        const auto object = QJsonDocument::fromJson(bytes).object().toVariantMap();
        for (auto it = object.begin(); it != object.end(); ++it) if (validate(it.key(),it.value())) config_[it.key()] = it.value();
    }
    const auto monitorBytes = saved.value("dock/monitorsV1").toByteArray();
    if (monitorBytes.size() <= 32768) {
        const auto monitors = QJsonDocument::fromJson(monitorBytes).object().toVariantMap();
        if (monitors.size() <= 16) for (auto it = monitors.begin(); it != monitors.end(); ++it) {
            QVariantMap values;
            const auto map = it.value().toMap();
            for (auto option = map.begin(); option != map.end(); ++option)
                if (QStringList{"edge","alignment","margin"}.contains(option.key()) && validate(option.key(),option.value())) values.insert(option.key(),option.value());
            if (it.key().size() <= 128) monitorConfig_.insert(it.key(),values);
        }
    }
    const auto pinBytes = saved.value("dock/pinsV1").toByteArray();
    if (pinBytes.size() <= 262144) {
        const auto pins = QJsonDocument::fromJson(pinBytes).array();
        if (pins.size() <= 128) for (const auto& pin : pins) {
            const auto p = pin.toObject();
            auto app = DockWindows::application(p["path"].toString());
            if (app.id.isEmpty()) continue;
            const auto aumid=p["aumid"].toString();
            if (!aumid.isEmpty() && aumid.size()<=512 && !aumid.contains(QChar::Null)) { app.aumid=aumid; app.id="app:"+aumid.toCaseFolded(); }
            if (std::none_of(pins_.begin(),pins_.end(),[&](const auto& a){ return a.id == app.id; })) pins_.append(app);
        }
    }
    refreshTimer_.setInterval(90); refreshTimer_.setSingleShot(true);
    connect(&source_,&DockWindows::windowsChanged,this,[this] { if (!refreshTimer_.isActive()) refreshTimer_.start(); });
    connect(&refreshTimer_,&QTimer::timeout,this,&DockController::refresh);
    connect(&source_,&DockWindows::snapshotReady,this,[this](const auto& snapshot) { if (enabled_) reconcile(snapshot); });
    connect(&source_,&DockWindows::themeChanged,this,[this] { theme_ = DockWindows::theme(); emit themeChanged(); layoutSurfaces(); });
    const auto topology = [this] { emit screensChanged(); if (enabled_) rebuildSurfaces(); };
    connect(qGuiApp,&QGuiApplication::screenAdded,this,topology);
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,topology);
    connect(qGuiApp,&QGuiApplication::primaryScreenChanged,this,topology);
    for (auto screen : qGuiApp->screens()) {
        connect(screen,&QScreen::geometryChanged,this,[this] { layoutSurfaces(); });
        connect(screen,&QScreen::availableGeometryChanged,this,[this] { layoutSurfaces(); });
        connect(screen,&QScreen::logicalDotsPerInchChanged,this,[this] { layoutSurfaces(); });
    }
    connect(qGuiApp,&QGuiApplication::screenAdded,this,[this](QScreen* screen) {
        connect(screen,&QScreen::geometryChanged,this,[this] { layoutSurfaces(); });
        connect(screen,&QScreen::availableGeometryChanged,this,[this] { layoutSurfaces(); });
        connect(screen,&QScreen::logicalDotsPerInchChanged,this,[this] { layoutSurfaces(); });
    });
}
DockController::~DockController() { enabled_ = false; surfaces_.clear(); source_.stop(); }
void DockController::attachEngine(QQmlEngine& engine) { engine.addImageProvider("dock",new Icons(this)); }
int DockController::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(entries_.size()); }
QHash<int,QByteArray> DockController::roleNames() const { return {{Identity,"identity"},{Title,"appTitle"},{Icon,"iconSource"},{Pinned,"pinned"},{Count,"windowCount"},{Active,"appActive"},{Windows,"appWindows"}}; }
QVariant DockController::data(const QModelIndex& index, int role) const {
    const int row = index.row(); if (!index.isValid() || row < 0 || row >= entries_.size()) return {};
    const auto& entry = entries_[row];
    switch (role) {
    case Identity: return entry.app.id;
    case Title: return entry.app.title;
    case Icon: return entry.app.icon.isNull() ? QString{} : QString("image://dock/%1/%2").arg(row).arg(qHash(entry.app.id));
    case Qt::UserRole+100: return entry.app.icon;
    case Pinned: return entry.pinned;
    case Count: return entry.windows.size();
    case Active: return std::any_of(entry.windows.begin(),entry.windows.end(),[](const auto& w){ return w.active; });
    case Windows: {
        QVariantList list;
        for (const auto& w : entry.windows) list.append(QVariantMap{{"handle",token(w)},{"title",w.title},{"active",w.active},{"screen",w.screen}});
        return list;
    }
    default: return {};
    }
}
void DockController::report(const QString& text) { status_ = text; emit statusChanged(); }
void DockController::save() {
    QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    saved.setValue("dock/enabled",enabled_);
    saved.setValue("dock/configV1",QJsonDocument(QJsonObject::fromVariantMap(config_)).toJson(QJsonDocument::Compact));
    saved.setValue("dock/monitorsV1",QJsonDocument(QJsonObject::fromVariantMap(monitorConfig_)).toJson(QJsonDocument::Compact));
    QJsonArray pins; for (const auto& app : pins_) pins.append(QJsonObject{{"path",app.path},{"aumid",app.aumid}});
    saved.setValue("dock/pinsV1",QJsonDocument(pins).toJson(QJsonDocument::Compact));
    saved.sync();
    if (saved.status() != QSettings::NoError) report(QStringLiteral("Не удалось сохранить настройки Dock."));
}
void DockController::restore() {
    QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    if (saved.value("dock/enabled",false).toBool()) setEnabled(true);
}
bool DockController::setEnabled(bool enabled) {
    if (enabled == enabled_) return true;
    if (enabled && !source_.start()) { report(QStringLiteral("Не удалось подключить события окон. Dock не включён.")); return false; }
    enabled_ = enabled;
    if (enabled_) {
        theme_ = DockWindows::theme(); emit themeChanged();
        if (pins_.isEmpty()) {
            QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
            if (!saved.contains("dock/pinsV1")) importWindowsPins();
        }
        refresh(); rebuildSurfaces();
        if (surfaces_.empty()) { enabled_ = false; source_.stop(); report(QStringLiteral("Окно Dock не загрузилось.")); emit enabledChanged(); return false; }
        if (!umbrella::platform::TaskbarRuntime::setLoginResume(true)) report(QStringLiteral("Dock работает, но автозапуск записать не удалось."));
        else report(QStringLiteral("Dock включён в совместимом режиме. Пуск, трей и приложения Windows остаются доступны на системной панели."));
    } else {
        surfaces_.clear(); refreshTimer_.stop(); source_.stop();
        QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
        if (!saved.value("resumeAppearance",false).toBool()) umbrella::platform::TaskbarRuntime::setLoginResume(false);
        report(QStringLiteral("Dock выключен. Настройки системной панели сохранены."));
    }
    save(); emit enabledChanged(); return true;
}
void DockController::configure(const QString& key, const QVariant& value) {
    if (!validate(key,value) || config_.value(key) == value) return;
    config_[key] = value; save(); emit configChanged();
    if (enabled_) {
        if (key == "allScreens" || key == "screen") rebuildSurfaces();
        else { if (key == "showRunning") refresh(); layoutSurfaces(); }
    }
}
QVariantMap DockController::screenConfig(const QString& name) const {
    auto values = config_; const auto overrides = monitorConfig_.value(name).toMap();
    for (auto it = overrides.begin(); it != overrides.end(); ++it) values[it.key()] = it.value();
    return values;
}
void DockController::configureScreen(const QString& name, const QString& key, const QVariant& value) {
    if (name.size() > 128 || !QStringList{"edge","alignment","margin"}.contains(key) || !validate(key,value)) return;
    if (!monitorConfig_.contains(name) && monitorConfig_.size() >= 16) return;
    auto values = monitorConfig_.value(name).toMap(); values[key] = value; monitorConfig_[name] = values;
    save(); emit configChanged(); layoutSurfaces();
}
QVariantList DockController::screens() const {
    QVariantList result;
    for (auto screen : qGuiApp->screens()) result.append(QVariantMap{{"name",screen->name()},{"label",screen->name() + QString(" · %1×%2").arg(screen->size().width()).arg(screen->size().height())}});
    return result;
}
void DockController::importWindowsPins() {
    const auto imported = source_.importPins();
    for (const auto& app : imported) if (pins_.size() < 128 && std::none_of(pins_.begin(),pins_.end(),[&](const auto& a) { return a.id == app.id; })) pins_.append(app);
    save(); refresh(); report(QStringLiteral("Импортированы доступные ярлыки Windows. Системный порядок и все packaged-закрепления публичным API не предоставляются; порядок можно изменить в Dock."));
}
bool DockController::addApplication(const QString& path) {
    if (pins_.size() >= 128) { report(QStringLiteral("Достигнут предел 128 закреплений.")); return false; }
    auto app = DockWindows::application(QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path);
    if (app.id.isEmpty()) { report(QStringLiteral("Выберите существующий .exe, .lnk или .appref-ms.")); return false; }
    if (std::none_of(pins_.begin(),pins_.end(),[&](const auto& a){ return a.id == app.id; })) pins_.append(app);
    save(); refresh(); return true;
}
void DockController::pin(int row) {
    if (row < 0 || row >= entries_.size()) return;
    const auto app = entries_[row].app;
    if (entries_[row].pinned) pins_.removeIf([&](const auto& a){ return a.id == app.id; });
    else if (pins_.size() < 128) pins_.append(app);
    save(); refresh();
}
void DockController::movePin(int from, int to) {
    if (from < 0 || to < 0 || from >= pins_.size() || to >= pins_.size() || from == to) return;
    pins_.move(from,to); save(); refresh();
}
void DockController::launch(int row) { if (row >= 0 && row < entries_.size() && !DockWindows::launch(entries_[row].app)) report(QStringLiteral("Windows не удалось запустить приложение.")); }
void DockController::activate(int row) {
    if (row < 0 || row >= entries_.size()) return;
    const auto windows = entries_[row].windows;
    if (windows.isEmpty()) { launch(row); return; }
    int target = 0;
    for (int i = 0; i < windows.size(); ++i) if (windows[i].active) { target = (i+1) % windows.size(); break; }
    if (!DockWindows::activate(windows[target],windows.size() == 1)) report(QStringLiteral("Windows отклонила переключение окна."));
}
void DockController::activateWindow(const QString& handle) {
    for (const auto& window:windowSnapshot_) if (token(window)==handle) { DockWindows::activate(window,false); return; }
}
void DockController::closeWindow(const QString& handle) {
    for (const auto& window:windowSnapshot_) if (token(window)==handle) { DockWindows::close(window); return; }
}
void DockController::refresh() {
    if (enabled_) source_.requestSnapshot();
    else reconcile({});
}
void DockController::reconcile(const QList<umbrella::platform::DockWindow>& windows) {
    windowSnapshot_ = windows;
    QList<Entry> wanted;
    for (const auto& pin : pins_) wanted.append({pin,{},true});
    // Retain the order of existing transient applications across foreground events.
    if (config_["showRunning"].toBool()) for (const auto& previous : entries_) {
        if (previous.pinned) continue;
        if (std::any_of(wanted.begin(),wanted.end(),[&](const auto& w){ return matches(w.app,previous.app); })) continue;
        if (std::any_of(windows.begin(),windows.end(),[&](const auto& w){ return w.app.id == previous.app.id; })) wanted.append({previous.app,{},false});
    }
    for (const auto& window : windows) {
        auto it = std::find_if(wanted.begin(),wanted.end(),[&](const auto& entry){ return entry.app.id == window.app.id; });
        if (it == wanted.end()) it = std::find_if(wanted.begin(),wanted.end(),[&](const auto& entry){ return matches(entry.app,window.app); });
        if (it == wanted.end()) {
            if (!config_["showRunning"].toBool()) continue;
            wanted.append({window.app,{},false}); it = wanted.end()-1;
        }
        it->windows.append(window);
        if (it->app.icon.isNull()) it->app.icon = window.app.icon;
    }
    for (int i = static_cast<int>(entries_.size())-1; i >= 0; --i) {
        if (std::none_of(wanted.begin(),wanted.end(),[&](const auto& w){ return w.app.id == entries_[i].app.id; })) {
            beginRemoveRows({},i,i); entries_.removeAt(i); endRemoveRows();
        }
    }
    for (int i = 0; i < wanted.size(); ++i) {
        int found = i;
        while (found < entries_.size() && entries_[found].app.id != wanted[i].app.id) ++found;
        if (found == entries_.size()) { beginInsertRows({},i,i); entries_.insert(i,wanted[i]); endInsertRows(); }
        else if (found != i) { beginMoveRows({},found,found,{},i); entries_.move(found,i); endMoveRows(); }
        const auto beforeWindows = data(index(i),Windows); const auto beforePinned = entries_[i].pinned;
        entries_[i] = wanted[i];
        if (beforeWindows != data(index(i),Windows) || beforePinned != entries_[i].pinned) emit dataChanged(index(i),index(i),{Pinned,Count,Active,Windows});
    }
    if (!entries_.isEmpty()) emit dataChanged(index(0),index(static_cast<int>(entries_.size())-1),{Icon});
    layoutSurfaces();
}
void DockController::rebuildSurfaces() {
    rebuilding_ = true;
    surfaces_.clear();
    if (!enabled_) { rebuilding_ = false; return; }
    const auto available = qGuiApp->screens();
    QScreen* selected = qGuiApp->primaryScreen();
    for (auto s : available) if (s->name() == config_["screen"].toString()) selected = s;
    for (auto screen : available) {
        if (!config_["allScreens"].toBool() && screen != selected) continue;
        auto surface = std::make_unique<Surface>(); surface->screen = screen;
        surface->view = std::make_unique<QQuickView>();
        auto& view = *surface->view;
        view.setScreen(screen); view.setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        view.setColor(Qt::transparent); view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.setPersistentGraphics(false); view.setPersistentSceneGraph(false);
        attachEngine(*view.engine());
        view.rootContext()->setContextProperty("dockSystem",this);
        view.rootContext()->setContextProperty("dockScreen",screen->name());
        view.setSource(QUrl("qrc:/qt/qml/Umbrella/Dock.qml"));
        if (view.status() != QQuickView::Ready) continue;
        surface->timer = new QTimer(&view); surface->timer->setSingleShot(true);
        auto* raw = surface.get();
        connect(raw->timer,&QTimer::timeout,this,[this,raw] {
            raw->hidden = raw->wantHidden;
            if (raw->hidden) raw->view->releaseResources();
            layoutSurfaces();
        });
        view.show(); surfaces_.push_back(std::move(surface));
    }
    rebuilding_ = false;
    layoutSurfaces();
}
void DockController::interaction(const QString& screen, bool hovered, bool menu) {
    if (!enabled_ || rebuilding_) return;
    for (auto& surface : surfaces_) if (surface->screen->name() == screen) {
        surface->hovered = hovered; surface->menu = menu;
    }
    layoutSurfaces();
}
void DockController::layoutSurfaces() {
    if (rebuilding_) return;
    using namespace umbrella::dock;
    for (auto& surface : surfaces_) {
        if (!surface->screen || !surface->view->rootObject()) continue;
        auto& view = *surface->view; auto* root = view.rootObject();
        const auto values = screenConfig(surface->screen->name());
        const auto edgeName = values["edge"].toString();
        const auto edge = edgeName == "top" ? Edge::top : edgeName == "left" ? Edge::left : edgeName == "right" ? Edge::right : Edge::bottom;
        const bool vertical = edge == Edge::left || edge == Edge::right;
        const auto screen = surface->screen->geometry();
        const auto workArea = surface->screen->availableGeometry();
        const int icon = values["iconSize"].toInt(), spacing = values["spacing"].toInt();
        const auto bounds = geometry({workArea.x(),workArea.y(),workArea.width(),workArea.height()},edge,
            (rowCount()+1)*(icon+spacing)+36,icon+32,values["margin"].toInt(),values["alignment"].toInt());
        surface->shown = QRect(bounds.x,bounds.y,bounds.width,bounds.height);
        bool overlap = false, fullscreen = false;
        for (const auto& window : windowSnapshot_) if (window.active && window.screen == surface->screen->name()) {
            fullscreen = window.fullscreen;
            // Win32 rectangles use physical coordinates; convert the Qt surface against this monitor origin.
            const auto factor = surface->screen->devicePixelRatio();
            const QRect physical(window.monitorRect.x()+qRound((bounds.x-screen.x())*factor),window.monitorRect.y()+qRound((bounds.y-screen.y())*factor),qRound(bounds.width*factor),qRound(bounds.height*factor));
            overlap = window.rect.intersects(physical);
        }
        const bool want = shouldHide(values["hideMode"].toInt(),surface->hovered,surface->menu,overlap,fullscreen,values["hideFullscreen"].toBool());
        if (want != surface->wantHidden) {
            surface->wantHidden = want;
            surface->timer->start(values[want ? "hideDelay" : "revealDelay"].toInt());
        } else if (want != surface->hidden && !surface->timer->isActive()) surface->timer->start(values[want ? "hideDelay" : "revealDelay"].toInt());
        auto rect = surface->shown;
        if (surface->hidden) {
            if (vertical) { rect.setWidth(3); rect.moveLeft(edge == Edge::right ? screen.right()-2 : screen.left()); }
            else { rect.setHeight(3); rect.moveTop(edge == Edge::bottom ? screen.bottom()-2 : screen.top()); }
        }
        root->setProperty("options",values);
        root->setProperty("concealed",surface->hidden);
        if (view.geometry() != rect) view.setGeometry(rect);
        const bool dark = values["theme"] == "dark" || (values["theme"] == "auto" && theme_["dark"].toBool());
        const bool blur = values["blur"].toBool() && theme_["transparency"].toBool() && !surface->hidden;
        const int backdropState = (blur ? 1 : 0) | (dark ? 2 : 0);
        if (surface->backdropState != backdropState) {
            surface->backdropState = backdropState;
            root->setProperty("backdropAvailable",DockWindows::backdrop(view,blur,dark));
        }
    }
}
