#include "TaskbarSettings.h"
#include "platform/windows/TaskbarState.h"
#include "platform/windows/TaskbarModuleSession.h"
#include <QTimer>
#include <QSettings>
#include <QGuiApplication>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>

namespace {
QString monitorId(const QScreen* screen) {
    return screen->name() + (screen->serialNumber().isEmpty() ? QString() : "/" + screen->serialNumber());
}
}

TaskbarSettings::TaskbarSettings(QObject* parent) : QObject(parent) {
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this] { emit changed(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { emit changed(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { emit changed(); });
    const auto state = umbrella::platform::taskbarAutoHide();
    available_ = state.has_value();
    autoHide_ = state.value_or(false);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const auto edge = settings.value("edge", "bottom").toString();
    if (edge == "top" || edge == "bottom" || edge == "left" || edge == "right") edge_ = edge;
    detached_ = settings.value("detached", false).toBool();
    bool ok = false;
    const auto transparency = settings.value("transparency", 35).toInt(&ok);
    if (ok && transparency >= 0 && transparency <= 100) transparency_ = transparency;
    const int savedWidth = settings.value("widthPercent", 100).toInt();
    if (savedWidth >= 40 && savedWidth <= 100) widthPercent_ = savedWidth;
    const int savedThickness = settings.value("thickness", 48).toInt();
    if (savedThickness >= 48 && savedThickness <= 96) thickness_ = savedThickness;
    islands_ = settings.value("islands", false).toBool();
    if (islands_) detached_ = true;
    if (!detached_) thickness_ = 48;
}
void TaskbarSettings::applyAutoHide(bool enabled) {
    if (pending_) return;
    const auto current = umbrella::platform::taskbarAutoHide();
    if (current && *current == enabled) {
        available_ = true;
        autoHide_ = enabled;
        status_ = QStringLiteral("Изменения применены.");
        emit changed();
        return;
    }
    if (!umbrella::platform::requestTaskbarAutoHide(enabled)) {
        available_ = false;
        status_ = QStringLiteral("Панель Windows недоступна. Изменение не применено.");
        emit changed();
        return;
    }
    pending_ = true;
    status_ = QStringLiteral("Проверяем состояние Windows…");
    emit changed();
    QTimer::singleShot(250, this, [this, enabled] {
        const auto state = umbrella::platform::taskbarAutoHide();
        available_ = state.has_value();
        if (state) autoHide_ = *state;
        pending_ = false;
        status_ = state && *state == enabled
            ? QStringLiteral("Автоскрытие применено к панели Windows.")
            : QStringLiteral("Windows не подтвердила изменение. Проверьте параметры панели.");
        emit changed();
    });
}

void TaskbarSettings::apply(const QString& edge, bool detached, int transparency, bool autoHide, int widthPercent, int thickness, bool islands) {
    if (pending_) return;
    if (islands) detached = true;
    if (!detached) thickness = 48;
    if (thickness < (islands ? 64 : 48) || thickness > 96 || (islands && !widthAvailable())) {
        status_ = QStringLiteral("Недопустимый размер или модуль островков недоступен.");
        emit changed(); return;
    }
    if ((edge == "left" || edge == "right") && !sidesAvailable()) {
        status_ = QStringLiteral("Боковая раскладка не проверена для этой сборки Windows.");
        emit changed(); return;
    }
    if (widthPercent < 40 || widthPercent > 100 || (detached && widthPercent != 100 && !widthAvailable())) {
        status_ = QStringLiteral("Собственный модуль изменения ширины недоступен.");
        emit changed(); return;
    }
    if (autoHide && edge != "bottom") {
        status_ = QStringLiteral("На этой сборке автоскрытие доступно только снизу.");
        emit changed(); return;
    }
    if (autoHide && ((detached && widthPercent != 100) || thickness != 48 || islands)) {
        status_ = QStringLiteral("Для автоскрытия нужна полная панель стандартной толщины без островков.");
        emit changed(); return;
    }
    if (!resetComponentLayout()) return;
    const auto previous = umbrella::platform::taskbarAutoHide();
    if (!previous) {
        status_ = QStringLiteral("Панель Windows недоступна."); emit changed(); return;
    }
    // Disable native autohide before applying a layout that keeps the panel visible.
    if (*previous && !autoHide) {
        if (!umbrella::platform::requestTaskbarAutoHide(false)) {
            status_ = QStringLiteral("Не удалось отключить автоскрытие перед переносом.");
            emit changed(); return;
        }
    }
    QString error;
    if (!appearance_.apply(edge, detached, transparency, widthPercent, thickness, islands, error)) {
        if (*previous && !autoHide) umbrella::platform::requestTaskbarAutoHide(true);
        status_ = error; emit changed(); return;
    }
    edge_ = edge; detached_ = detached; transparency_ = transparency; widthPercent_ = widthPercent; thickness_ = thickness; islands_ = islands;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    settings.setValue("edge", edge_);
    settings.setValue("detached", detached_);
    settings.setValue("transparency", transparency_);
    settings.setValue("widthPercent", widthPercent_);
    settings.setValue("thickness", thickness_);
    settings.setValue("islands", islands_);
    settings.sync();
    const bool saved = settings.status() == QSettings::NoError;
    applyAutoHide(autoHide);
    if (!saved) {
        status_ = QStringLiteral("Оформление применено, но сохранить выбранные значения не удалось.");
        emit changed();
    }
}
bool TaskbarSettings::widthAvailable() const { return umbrella::platform::TaskbarModuleSession::available(); }
bool TaskbarSettings::sidesAvailable() const { return umbrella::platform::TaskbarModuleSession::layoutAvailable(); }
void TaskbarSettings::resetAppearance() {
    if (pending_) return;
    if (!resetComponentLayout()) return;
    QString error;
    status_ = appearance_.restore(error)
        ? QStringLiteral("Исходное расположение и оформление восстановлены.") : error;
    emit changed();
}

QVariantList TaskbarSettings::layoutMonitors() const {
    QVariantList result;
    for (auto screen : QGuiApplication::screens()) result.append(QVariantMap{
        {"id", monitorId(screen)}, {"label", screen->name() + (screen == QGuiApplication::primaryScreen() ? QStringLiteral(" · основной") : QString())},
        {"primary", screen == QGuiApplication::primaryScreen()}, {"width", screen->geometry().width()}});
    return result;
}
bool TaskbarSettings::resetComponentLayout() {
    QString error; const bool ok = componentSession_.stop(error);
    status_ = ok ? QStringLiteral("Исходная позиция Task View восстановлена; остальные компоненты не изменялись.") : error;
    emit changed(); return ok;
}
bool TaskbarSettings::placeTaskViewAfterWidgets(const QString& monitor, int gap) {
    auto primary = QGuiApplication::primaryScreen();
    if (pending_ || !primary || monitorId(primary) != monitor) { status_ = QStringLiteral("Применение пока поддерживается только на основном мониторе."); emit changed(); return false; }
    QString error;
    // Existing styling owns its own recovery session. Do not replace it implicitly.
    const bool ok = componentSession_.positionTaskView(gap, error, true);
    status_ = ok ? QStringLiteral("Применена только позиция Task View. Логический порядок Explorer и остальные компоненты не изменены.") : error;
    emit changed(); return ok;
}

namespace {
QJsonObject readDrafts(QSettings& settings) {
    const auto bytes=settings.value("componentDraftsV1").toByteArray();
    if (bytes.size()>65536) return {};
    const auto document=QJsonDocument::fromJson(bytes);
    return document.isObject() ? document.object() : QJsonObject{};
}
bool validDraft(const QVariantMap& draft) {
    const auto value=QJsonObject::fromVariantMap(draft);
    if (value.size()!=6 || value.value("version").toInt()!=1) return false;
    const auto gap=value.value("gap");
    if (!gap.isDouble() || gap.toDouble()!=gap.toInt(-1) || gap.toInt()<0 || gap.toInt()>96) return false;
    QSet<QString> ids;
    static const QRegularExpression idPattern(QStringLiteral("^[a-z][a-z0-9_-]{0,47}$"));
    for (const auto zone : {"available","left","center","right"}) {
        if (!value.value(zone).isArray()) return false;
        for (const auto entry : value.value(zone).toArray()) {
            const auto id=entry.toString();
            if (!entry.isString() || !idPattern.match(id).hasMatch() || ids.contains(id) || ids.size()>=64) return false;
            ids.insert(id);
        }
    }
    return !ids.isEmpty(); // QML additionally validates against the current component catalogue.
}
}
QVariantMap TaskbarSettings::componentDraft(const QString& monitor) const {
    if (monitor.isEmpty() || monitor.size()>256) return {};
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    const auto draft=readDrafts(settings).value(monitor).toObject().toVariantMap();
    return validDraft(draft) ? draft : QVariantMap{};
}
bool TaskbarSettings::saveComponentDraft(const QString& monitor, const QVariantMap& draft) {
    if (monitor.isEmpty() || monitor.size()>256 || !validDraft(draft)) return false;
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    auto all=readDrafts(settings);
    if (!all.contains(monitor) && all.size()>=16) return false;
    all.insert(monitor,QJsonObject::fromVariantMap(draft));
    const auto bytes=QJsonDocument(all).toJson(QJsonDocument::Compact);
    if (bytes.size()>65536) return false;
    settings.setValue("componentDraftsV1",bytes); settings.sync();
    return settings.status()==QSettings::NoError;
}
