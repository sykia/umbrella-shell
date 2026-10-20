#include "TaskbarSettings.h"
#include "platform/windows/TaskbarState.h"
#include "platform/windows/TaskbarModuleSession.h"
#include "platform/windows/TaskbarRuntime.h"
#include <QTimer>
#include <QSettings>
#include <QGuiApplication>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <QDir>
#include <QDebug>
#include <algorithm>

namespace {
bool setResumeAtLogin(bool enabled) {
    return umbrella::platform::TaskbarRuntime::setLoginResume(enabled);
}
QJsonObject appearanceProfiles(QSettings& settings) {
    const auto bytes = settings.value("appearanceProfilesV1").toByteArray();
    if (bytes.size() > 65536) return {};
    const auto value = QJsonDocument::fromJson(bytes).object();
    return value.size() <= 16 ? value : QJsonObject{};
}
QJsonObject legacyAppearance(QSettings& settings) {
    QJsonObject value;
    for (const auto key : {"edge", "detached", "transparency", "widthPercent", "thickness", "islands"})
        if (settings.contains(key)) value.insert(key, QJsonValue::fromVariant(settings.value(key)));
    return value;
}
bool validAppearance(const QJsonObject& value) {
    const auto edge = value["edge"].toString();
    const int width = value["widthPercent"].toVariant().toInt(), thickness = value["thickness"].toVariant().toInt();
    const int alpha = value["transparency"].toVariant().toInt();
    return (edge == "bottom" || edge == "top" || edge == "left" || edge == "right") && width >= 40 && width <= 100
        && thickness >= (value["islands"].toVariant().toBool() ? 64 : 48) && thickness <= 96 && alpha >= 0 && alpha <= 100;
}
QString monitorId(const QScreen* screen) {
    for (const auto& entry : umbrella::platform::TaskbarRuntime::monitors()) {
        const auto monitor = entry.toMap();
        if (monitor.value("device").toString() == screen->name()) return monitor.value("id").toString();
    }
    return {}; // No persistent identity: do not guess another monitor's profile.
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
    for (const auto& entry : appearanceMonitors()) {
        const auto monitor = entry.toMap();
        if (monitor["primary"].toBool()) { selectAppearanceMonitor(monitor["id"].toString()); break; }
    }
}
QVariantList TaskbarSettings::appearanceMonitors() const { return umbrella::platform::TaskbarRuntime::monitors(); }
void TaskbarSettings::selectAppearanceMonitor(const QString& id) {
    bool found = false;
    for (const auto& entry : appearanceMonitors()) if (entry.toMap()["id"].toString() == id) found = true;
    if (!found || pending_) return;
    selectedAppearanceMonitor_ = id;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const auto profiles = appearanceProfiles(settings);
    const auto value = profiles.contains(id) ? profiles[id].toObject() : legacyAppearance(settings);
    if (validAppearance(value)) {
        edge_ = value["edge"].toString(); islands_ = value["islands"].toVariant().toBool();
        detached_ = value["detached"].toVariant().toBool() || islands_;
        transparency_ = value["transparency"].toVariant().toInt(); widthPercent_ = value["widthPercent"].toVariant().toInt();
        thickness_ = detached_ ? value["thickness"].toVariant().toInt() : 48;
    }
    emit changed();
}
bool TaskbarSettings::restoreSavedAppearance(bool freshSession) {
    if (pending_) { QTimer::singleShot(300, this, [this, freshSession] { restoreSavedAppearance(freshSession); }); return true; }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    restored_ = false;
    if (!settings.value("resumeAppearance", settings.contains("edge")).toBool()) return true;
    const auto monitors = appearanceMonitors();
    if (monitors.isEmpty()) return false;
    available_ = umbrella::platform::taskbarAutoHide().has_value();
    if (!available_) return false;
    QString error;
    if (freshSession && !appearance_.restore(error)) { status_ = error; emit changed(); return false; }
    const auto confirmed = settings.value("confirmedComponentV1").toMap();
    if (!confirmed.isEmpty()) {
        if (!componentSession_.stop(error)) { status_ = error; emit changed(); return false; }
        const bool applied = applyComponentConfiguration(confirmed["monitor"].toString(), confirmed["configuration"].toMap());
        if (!applied) {
            status_ = QStringLiteral("Сохранённое размещение Task View сейчас несовместимо; настройки сохранены."); emit changed();
        }
        restored_ = !activeComponentConfiguration_.isEmpty();
        // A compatible Explorer can expose its bridge before Task View exists.
        // Let the runtime's bounded startup retries cover that transient state.
        return applied || !sidesAvailable();
    }
    const auto profiles = appearanceProfiles(settings);
    const auto fallback = legacyAppearance(settings);
    QStringList skipped;
    struct Target { QString device; QJsonObject value; bool primary; };
    QList<Target> targets;
    for (const auto& entry : monitors) {
        const auto monitor = entry.toMap();
        auto value = profiles.contains(monitor["id"].toString()) ? profiles[monitor["id"].toString()].toObject() : fallback;
        if (!validAppearance(value)) { skipped.append(monitor["label"].toString()); continue; }
        auto edge = value["edge"].toString();
        if ((edge == "left" || edge == "right") && !sidesAvailable()) { value["edge"] = "bottom"; skipped.append(QStringLiteral("боковое расположение")); }
        if (!widthAvailable()) { skipped.append(QStringLiteral("нативный модуль")); continue; }
        targets.append({monitor["device"].toString(), value, monitor["primary"].toBool()});
    }
    if (targets.isEmpty()) { status_ = QStringLiteral("Сохранённые параметры сейчас несовместимы; файл настроек не изменён."); emit changed(); return true; }
    std::stable_sort(targets.begin(), targets.end(), [](const Target& a, const Target& b) { return a.primary && !b.primary; });
    bool autoHide = settings.value("autoHide", autoHide_).toBool();
    for (const auto& target : targets) {
        const auto& value = target.value;
        if (value["edge"].toString() != "bottom" || value["islands"].toVariant().toBool() || value["thickness"].toVariant().toInt() != 48
            || (value["detached"].toVariant().toBool() && value["widthPercent"].toVariant().toInt() != 100)) autoHide = false;
    }
    const auto previousHide = umbrella::platform::taskbarAutoHide();
    if (previousHide.value_or(false) && !autoHide) umbrella::platform::requestTaskbarAutoHide(false);
    const auto& first = targets.front().value;
    // The baseline session captures and journals all panels before individual overrides.
    bool ok = appearance_.apply(first["edge"].toString(), first["detached"].toVariant().toBool(), first["transparency"].toVariant().toInt(),
        first["widthPercent"].toVariant().toInt(), first["thickness"].toVariant().toInt(), first["islands"].toVariant().toBool(), error);
    for (const auto& target : targets) {
        if (!ok) break;
        auto value = target.value;
        const auto edge = value["edge"].toString();
        const auto baseEdge = first["edge"].toString();
        if (edge != baseEdge && (edge == "left" || edge == "right" || baseEdge == "left" || baseEdge == "right")) {
            value["edge"] = baseEdge; skipped.append(QStringLiteral("разные боковые стороны"));
        }
        ok = appearance_.applyMonitor(target.device, value["edge"].toString(), value["detached"].toVariant().toBool(), value["transparency"].toVariant().toInt(),
            value["widthPercent"].toVariant().toInt(), value["thickness"].toVariant().toInt(), value["islands"].toVariant().toBool(), error);
    }
    if (!ok) {
        QString ignored; appearance_.restore(ignored);
        if (previousHide) umbrella::platform::requestTaskbarAutoHide(*previousHide);
        status_ = error; emit changed(); return false;
    }
    restored_ = true;
    applyAutoHide(autoHide);
    if (!skipped.isEmpty()) status_ = QStringLiteral("Пропущены несовместимые параметры: ") + skipped.join(", ") + QStringLiteral(". Выбор сохранён.");
    if (!setResumeAtLogin(true)) status_ = QStringLiteral("Оформление восстановлено, но записать автозапуск не удалось.");
    emit changed();
    return true;
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
        restored_ = false;
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
        if (!state || *state != enabled) restored_ = false;
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
    if (edge != "bottom" && edge != "top" && edge != "left" && edge != "right") return;
    if (transparency < 0 || transparency > 100) return;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const QStringList keys{"appearanceProfilesV1", "edge", "detached", "transparency", "widthPercent", "thickness", "islands", "autoHide", "resumeAppearance", "confirmedComponentV1"};
    QVariantMap previous;
    for (const auto& key : keys) if (settings.contains(key)) previous.insert(key, settings.value(key));
    if (!resetComponentLayout()) return;
    edge_ = edge; detached_ = detached; transparency_ = transparency; widthPercent_ = widthPercent; thickness_ = thickness; islands_ = islands;
    auto profiles = appearanceProfiles(settings);
    const auto old = legacyAppearance(settings);
    const auto monitors = appearanceMonitors();
    if (selectedAppearanceMonitor_.isEmpty()) { status_ = QStringLiteral("Монитор недоступен."); emit changed(); return; }
    for (const auto& entry : monitors) {
        const auto monitor = entry.toMap(); const auto id = monitor["id"].toString();
        if (!profiles.contains(id) && profiles.size() < 16 && validAppearance(old)) profiles[id] = old;
    }
    if (!profiles.contains(selectedAppearanceMonitor_) && profiles.size() >= 16) return;
    const QJsonObject value{{"edge", edge_}, {"detached", detached_}, {"transparency", transparency_},
        {"widthPercent", widthPercent_}, {"thickness", thickness_}, {"islands", islands_}};
    profiles[selectedAppearanceMonitor_] = value;
    settings.setValue("appearanceProfilesV1", QJsonDocument(profiles).toJson(QJsonDocument::Compact));
    // Keep the legacy fallback tied to the primary panel, never the last edited secondary.
    for (const auto& entry : monitors) if (entry.toMap()["primary"].toBool() && entry.toMap()["id"].toString() == selectedAppearanceMonitor_)
        for (auto it = value.begin(); it != value.end(); ++it) settings.setValue(it.key(), it.value().toVariant());
    settings.setValue("autoHide", autoHide);
    settings.setValue("resumeAppearance", true);
    settings.sync();
    const bool saved = settings.status() == QSettings::NoError;
    const bool applied = saved && restoreSavedAppearance(false);
    if (!applied) {
        const auto failure = saved ? status_ : QStringLiteral("Не удалось сохранить настройки; изменение отменено.");
        for (const auto& key : keys) {
            if (previous.contains(key)) settings.setValue(key, previous[key]); else settings.remove(key);
        }
        settings.sync();
        selectAppearanceMonitor(selectedAppearanceMonitor_);
        status_ = settings.status() == QSettings::NoError ? failure : QStringLiteral("Ошибка записи настроек; проверьте доступ к пользовательскому INI.");
        emit changed();
    }
}
bool TaskbarSettings::widthAvailable() const { return umbrella::platform::TaskbarModuleSession::available(); }
bool TaskbarSettings::sidesAvailable() const { return umbrella::platform::TaskbarModuleSession::layoutAvailable(); }
void TaskbarSettings::resetAppearance() {
    if (pending_) return;
    if (!resetComponentLayout()) return;
    QString error;
    if (appearance_.restore(error)) {
        restored_ = false;
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
        settings.setValue("resumeAppearance", false);
        settings.sync();
        const bool disabled = setResumeAtLogin(false);
        status_ = settings.status() == QSettings::NoError && disabled
            ? QStringLiteral("Исходное оформление восстановлено; восстановление при входе отключено.")
            : QStringLiteral("Оформление восстановлено, но сохранить отключение автозапуска не удалось.");
    } else status_ = error;
    emit changed();
}

QVariantList TaskbarSettings::layoutMonitors() const {
    return umbrella::platform::TaskbarRuntime::monitors();
}

bool TaskbarSettings::resetComponentLayout() {
    QString error; const bool ok = componentSession_.stop(error);
    status_ = ok ? QStringLiteral("Исходная позиция Task View восстановлена; остальные компоненты не изменялись.") : error;
    if (ok) {
        activeComponentConfiguration_.clear(); activeComponentMonitor_.clear();
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
        settings.remove("confirmedComponentV1"); settings.sync();
        if (settings.status() != QSettings::NoError) { status_ = QStringLiteral("Не удалось сохранить сброс размещения компонентов."); emit changed(); return false; }
    }
    emit changed(); return ok;
}
bool TaskbarSettings::placeTaskViewAfterWidgets(const QString& monitor, int gap) {
    bool primary = false;
    for (const auto& entry : layoutMonitors()) if (entry.toMap()["id"].toString() == monitor) primary = entry.toMap()["primary"].toBool();
    if (pending_ || !primary) { status_ = QStringLiteral("Применение пока поддерживается только на основном мониторе."); emit changed(); return false; }
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
    const auto all = readDrafts(settings);
    auto draft = all.value(monitor).toObject().toVariantMap();
    // Preserve old QScreen-keyed drafts, without ever treating them as confirmed.
    if (draft.isEmpty()) {
        QString label;
        for (const auto& entry : layoutMonitors()) if (entry.toMap()["id"].toString() == monitor) label = entry.toMap()["label"].toString();
        int matches = 0;
        for (const auto screen : QGuiApplication::screens()) if (screen->name() == label) ++matches;
        if (matches == 1) for (const auto screen : QGuiApplication::screens()) if (screen->name() == label) draft = all.value(monitorId(screen)).toObject().toVariantMap();
    }
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

namespace {
// Only the measured placement of original Task View is accepted. Arbitrary old
// drafts must never be saved as a native-confirmed layout.
bool supportedComponentConfiguration(const QVariantMap& configuration) {
    if (!validDraft(configuration)) return false;
    const auto value = QJsonObject::fromVariantMap(configuration);
    const auto expected = QJsonDocument::fromJson(R"({
        "left":["widgets","taskview"],
        "center":["start","search","apps"],
        "right":["overflow","input","network","volume","clock","desktop"],
        "available":["trayapps","appoverflow","power","pen","keyboard","touchpad","emoji","studio","microphone","callmute","privacy","notifications","eject","vpn","chat","copilot","systemtray","remote"]
    })").object();
    for (const auto key : {"left", "center", "right", "available"})
        if (value.value(key) != expected.value(key)) return false;
    return true;
}
}
bool TaskbarSettings::applyComponentConfiguration(const QString& monitor, const QVariantMap& configuration) {
    if (!supportedComponentConfiguration(configuration)) return false;
    const auto previous = activeComponentConfiguration_;
    const auto previousMonitor = activeComponentMonitor_;
    if (!placeTaskViewAfterWidgets(monitor, configuration.value("gap").toInt())) {
        qWarning().noquote() << "Component placement:" << status_;
        // Rejection can close the host. Re-establish the last confirmed layout.
        bool recovered = previous.isEmpty();
        if (!previous.isEmpty()) recovered = placeTaskViewAfterWidgets(previousMonitor, previous.value("gap").toInt());
        if (!recovered) {
            activeComponentConfiguration_.clear(); activeComponentMonitor_.clear();
            // Never replay a record whose native rollback could not be confirmed.
            // Keep the historical draft and unrelated appearance settings intact.
            QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
            settings.remove("confirmedComponentV1"); settings.sync();
            status_ = settings.status() == QSettings::NoError
                ? QStringLiteral("Перемещение и восстановление прежней позиции не подтверждены. Повторное применение отключено.")
                : QStringLiteral("Восстановление позиции и удаление записи автоприменения не подтверждены.");
        } else status_ = QStringLiteral("Не удалось переместить компонент. Прежняя подтверждённая позиция сохранена.");
        emit changed(); return false;
    }
    activeComponentConfiguration_ = configuration;
    activeComponentMonitor_ = monitor;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    settings.setValue("confirmedComponentV1", QVariantMap{{"monitor", monitor}, {"configuration", configuration}});
    settings.setValue("resumeAppearance", true); settings.sync();
    const bool saved = saveComponentDraft(monitor, configuration) && settings.status() == QSettings::NoError && setResumeAtLogin(true);
    status_ = saved ? QString() : QStringLiteral("Расположение применено, но сохранить его не удалось.");
    emit changed(); return true;
}
bool TaskbarSettings::restoreComponentConfiguration(const QString& monitor) {
    bool found = false;
    for (const auto& entry : layoutMonitors()) if (entry.toMap().value("id").toString() == monitor) found = true;
    if (!found || pending_) return false;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const bool confirmedTarget = activeComponentMonitor_ == monitor || settings.value("confirmedComponentV1").toMap()["monitor"].toString() == monitor;
    if (activeComponentMonitor_ == monitor && !resetComponentLayout()) {
        qWarning().noquote() << "Component restore:" << status_;
        status_ = QStringLiteral("Windows не подтвердила восстановление."); emit changed(); return false;
    }
    auto all = readDrafts(settings); all.remove(monitor);
    QString label;
    for (const auto& entry : layoutMonitors()) if (entry.toMap()["id"].toString() == monitor) label = entry.toMap()["label"].toString();
    int matchingScreens = 0;
    for (const auto screen : QGuiApplication::screens()) if (screen->name() == label) ++matchingScreens;
    if (matchingScreens == 1) for (const auto screen : QGuiApplication::screens()) if (screen->name() == label) all.remove(monitorId(screen));
    if (settings.value("confirmedComponentV1").toMap().value("monitor").toString() == monitor)
        settings.remove("confirmedComponentV1");
    settings.setValue("componentDraftsV1", QJsonDocument(all).toJson(QJsonDocument::Compact));
    if (confirmedTarget) settings.setValue("resumeAppearance", false);
    settings.sync();
    const bool saved = settings.status() == QSettings::NoError && (!confirmedTarget || setResumeAtLogin(false));
    status_ = saved ? QString() : QStringLiteral("Исходный вид возвращён, но сохранённые настройки не удалены.");
    emit changed(); return saved;
}
