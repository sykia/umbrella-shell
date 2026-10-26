#include "WidgetsModel.h"
#include "core/widgets/Registry.h"
#include "platform/windows/DesktopLayer.h"
#include "platform/windows/TaskbarRuntime.h"
#include "platform/windows/WindowBackdrop.h"
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QSet>
#include <QStringList>
#include <QRegularExpression>
#include <QUuid>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
bool known(const QString& id) {
    for (const auto& entry : umbrella::widgets::catalog) if (text(entry.id) == id) return true;
    return false;
}
QSize dimensions(const QVariantMap& settings) {
    const bool stacked = settings.value("mode").toString() == "stacked";
    const int horizontal = settings.value("horizontalSpacing", 0).toInt();
    const int vertical = settings.value("verticalSpacing", 0).toInt();
    return stacked ? QSize(250 + horizontal * 2, 300 + vertical) : QSize(390 + horizontal * 3, 180);
}
}

WidgetsModel::WidgetsModel(QObject* parent) : QObject(parent), desktop_(std::make_unique<umbrella::platform::DesktopLayer>()) {
    desktop_->changed = [this] { ++desktopRevision_; emit desktopChanged(); };
    settings_.remove("layoutV1");
    const auto raw = settings_.value("layoutV2").toByteArray();
    const auto document = raw.size() <= 65536 ? QJsonDocument::fromJson(raw) : QJsonDocument{};
    bool migrated = false;
    if (document.isArray()) {
        QSet<QString> ids;
        for (const auto& value : document.array()) {
            auto map = value.toObject().toVariantMap();
            auto widgetSettings = map.value("settings").toMap();
            const auto legacyStyle = widgetSettings.value("style").toString();
            if (legacyStyle == "white" || legacyStyle == "blur") {
                widgetSettings.insert("style", legacyStyle == "white" ? "empty" : "blur-card");
                map.insert("settings", widgetSettings);
                migrated = true;
            }
            const auto id = map.value("instanceId").toString();
            if (map.value("definitionId") != "clock" || id.isEmpty() || ids.contains(id)
                || !validSettings(map.value("settings").toMap())) continue;
            ids.insert(id);
            layout_.append(map);
            if (layout_.size() == 32) break;
        }
    }
    if (migrated) settings_.setValue("layoutV2", QJsonDocument(QJsonArray::fromVariantList(layout_)).toJson(QJsonDocument::Compact));
    const auto watch = [this](QScreen* screen) { connect(screen, &QScreen::geometryChanged, this, [this] { refreshMonitors(); }); };
    for (auto* screen : qGuiApp->screens()) watch(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, watch](QScreen* screen) { watch(screen); refreshMonitors(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { refreshMonitors(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { refreshMonitors(); });
    refreshMonitors();
}
WidgetsModel::~WidgetsModel() = default;

bool WidgetsModel::validSettings(const QVariantMap& settings) const {
    const auto mode = settings.value("mode").toString();
    const auto style = settings.value("style").toString();
    const auto color = settings.value("color").toString();
    bool hx = false, vy = false;
    const int horizontal = settings.value("horizontalSpacing").toInt(&hx);
    const int vertical = settings.value("verticalSpacing").toInt(&vy);
    return (mode == "inline" || mode == "stacked") && QStringList{"empty", "blur-card", "blur-digits"}.contains(style)
        && QRegularExpression("^#[0-9A-Fa-f]{6}$").match(color).hasMatch()
        && hx && vy && horizontal >= -4 && horizontal <= 28 && vertical >= -24 && vertical <= 80;
}

QVariantList WidgetsModel::catalog() const {
    QVariantList result;
    for (const auto& entry : umbrella::widgets::catalog)
        result.append(QVariantMap{{"id", text(entry.id)}, {"title", text(entry.title)}, {"icon", text(entry.icon)},
            {"category", text(entry.category)}, {"available", text(entry.id) == "clock"}});
    return result;
}

void WidgetsModel::refreshMonitors() {
    const auto native = umbrella::platform::TaskbarRuntime::monitors();
    monitors_.clear();
    int number = 0;
    for (auto* screen : qGuiApp->screens()) {
        QString id = screen->name();
        for (const auto& entry : native) {
            const auto map = entry.toMap();
            if (map.value("device").toString() == screen->name()) { id = map.value("id").toString(); break; }
        }
        const auto geometry = screen->geometry();
        monitors_.append(QVariantMap{{"id", id}, {"name", screen->name()}, {"label", QStringLiteral("Монитор %1").arg(++number)},
            {"primary", screen == qGuiApp->primaryScreen()}, {"width", geometry.width()}, {"height", geometry.height()},
            {"left", geometry.left()}, {"top", geometry.top()}});
    }
    closeEditor();
    emit monitorsChanged(); emit instancesChanged();
}
QVariantList WidgetsModel::monitors() const { return monitors_; }
QString WidgetsModel::selectedMonitorLabel() const {
    for (const auto& monitor : monitors_) if (monitor.toMap().value("id") == monitorId_) return monitor.toMap().value("label").toString();
    return {};
}

QVariantList WidgetsModel::instances() const {
    QVariantList result;
    for (const auto& value : layout_) {
        auto record = value.toMap();
        for (const auto& monitorValue : monitors_) {
            const auto monitor = monitorValue.toMap();
            if (monitor.value("id") != record.value("screenId")) continue;
            const auto size = dimensions(record.value("settings").toMap());
            const int width = std::min(size.width(), monitor.value("width").toInt());
            const int height = std::min(size.height(), monitor.value("height").toInt());
            const double x = std::clamp(record.value("x").toDouble(), 0.0, std::max(0.0, monitor.value("width").toDouble() - width));
            const double y = std::clamp(record.value("y").toDouble(), 0.0, std::max(0.0, monitor.value("height").toDouble() - height));
            record.insert("width", width); record.insert("height", height);
            record.insert("globalX", monitor.value("left").toDouble() + x);
            record.insert("globalY", monitor.value("top").toDouble() + y);
            record.insert("screenWidth", monitor.value("width")); record.insert("screenHeight", monitor.value("height"));
            result.append(record); break;
        }
    }
    return result;
}

QVariantMap WidgetsModel::selectedClock() const {
    for (const auto& value : instances()) {
        const auto map = value.toMap();
        if (map.value("instanceId") == selectedInstanceId_ && map.value("screenId") == monitorId_) return map;
    }
    return {};
}

void WidgetsModel::openEditor(const QString& monitorId) {
    if (editing_ || monitors_.isEmpty()) return;
    const auto requested = monitorId.isEmpty() ? monitors_.first().toMap().value("id").toString() : monitorId;
    for (const auto& monitor : monitors_) if (monitor.toMap().value("id") == requested) {
        monitorId_ = requested; editing_ = true; selectedId_ = "clock"; selectedInstanceId_.clear();
        emit selectionChanged(); emit editChanged(); return;
    }
}
void WidgetsModel::closeEditor() {
    if (!editing_) return;
    editing_ = false; selectedId_.clear(); selectedInstanceId_.clear();
    emit selectionChanged(); emit editChanged();
}
void WidgetsModel::select(const QString& id) {
    if (!editing_ || !known(id) || selectedId_ == id) return;
    selectedId_ = id; selectedInstanceId_.clear(); emit selectionChanged();
}
void WidgetsModel::selectInstance(const QString& id) {
    if (!editing_) return;
    for (const auto& value : layout_) {
        const auto map = value.toMap();
        if (map.value("instanceId") == id && map.value("screenId") == monitorId_) {
            selectedInstanceId_ = id; selectedId_ = "clock"; emit selectionChanged(); return;
        }
    }
}

bool WidgetsModel::save() {
    const auto raw = QJsonDocument(QJsonArray::fromVariantList(layout_)).toJson(QJsonDocument::Compact);
    if (raw.size() > 65536) return false;
    settings_.setValue("layoutV2", raw); settings_.sync();
    if (settings_.status() != QSettings::NoError) { status_ = QStringLiteral("Не удалось сохранить часы."); emit statusChanged(); return false; }
    emit instancesChanged(); emit monitorsChanged(); emit selectionChanged(); return true;
}
bool WidgetsModel::addClock(const QString& mode) {
    if (!editing_ || (mode != "inline" && mode != "stacked") || layout_.size() >= 32) return false;
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QVariantMap settings{{"mode", mode}, {"style", "empty"}, {"color", "#FFFFFF"}, {"horizontalSpacing", 0}, {"verticalSpacing", 0}};
    int count = 0;
    for (const auto& value : layout_) if (value.toMap().value("screenId") == monitorId_) ++count;
    layout_.append(QVariantMap{{"instanceId", id}, {"definitionId", "clock"}, {"screenId", monitorId_},
        {"x", 96 + (count % 5) * 42}, {"y", 110 + (count % 5) * 42}, {"settings", settings}});
    if (!save()) { layout_.removeLast(); return false; }
    selectInstance(id); return true;
}
bool WidgetsModel::updateClock(const QVariantMap& settings) {
    if (!editing_ || !validSettings(settings)) return false;
    for (auto& value : layout_) {
        auto map = value.toMap();
        if (map.value("instanceId") != selectedInstanceId_ || map.value("screenId") != monitorId_) continue;
        map.insert("settings", settings); value = map; return save();
    }
    return false;
}
bool WidgetsModel::moveInstance(const QString& id, double globalX, double globalY) {
    if (!editing_ || !std::isfinite(globalX) || !std::isfinite(globalY)) return false;
    for (auto& value : layout_) {
        auto map = value.toMap();
        if (map.value("instanceId") != id || map.value("screenId") != monitorId_) continue;
        for (const auto& monitorValue : monitors_) {
            const auto monitor = monitorValue.toMap();
            if (monitor.value("id") != monitorId_) continue;
            const auto size = dimensions(map.value("settings").toMap());
            map.insert("x", std::clamp(globalX - monitor.value("left").toDouble(), 0.0, std::max(0.0, monitor.value("width").toDouble() - size.width())));
            map.insert("y", std::clamp(globalY - monitor.value("top").toDouble(), 0.0, std::max(0.0, monitor.value("height").toDouble() - size.height())));
            value = map; return save();
        }
    }
    return false;
}
bool WidgetsModel::removeInstance(const QString& id) {
    if (!editing_) return false;
    for (int i = 0; i < layout_.size(); ++i) if (layout_[i].toMap().value("instanceId") == id && layout_[i].toMap().value("screenId") == monitorId_) {
        layout_.removeAt(i); selectedInstanceId_.clear(); return save();
    }
    return false;
}

bool WidgetsModel::attachDesktop(QWindow* window, const QString& id) {
    if (!window || QGuiApplication::arguments().contains("--ui-smoke")) return false;
    for (const auto& value : instances()) {
        const auto instance = value.toMap();
        if (instance.value("instanceId") != id) continue;
        for (const auto& monitorValue : monitors_) {
            const auto monitor = monitorValue.toMap();
            if (monitor.value("id") != instance.value("screenId")) continue;
            for (auto* screen : qGuiApp->screens()) if (screen->name() == monitor.value("name"))
                return desktop_->place(window, screen, QRect(instance.value("globalX").toInt(), instance.value("globalY").toInt(), instance.value("width").toInt(), instance.value("height").toInt()));
        }
    }
    return false;
}
bool WidgetsModel::prepareWindow(QWindow* window, bool transparency, bool dark) {
    if (!window || !editing_) return false;
    QScreen* target = nullptr;
    for (const auto& monitorValue : monitors_) {
        const auto monitor = monitorValue.toMap();
        if (monitor.value("id") != monitorId_) continue;
        for (auto* screen : qGuiApp->screens()) if (screen->name() == monitor.value("name")) target = screen;
    }
    if (!target) { closeEditor(); return false; }
    if (window->screen() != target) window->setScreen(target);
    if (!window->isVisible()) { window->setWindowState(Qt::WindowNoState); window->setGeometry(target->geometry()); window->show(); }
    return umbrella::platform::applyWindowBackdrop(*window, transparency, dark, true);
}
