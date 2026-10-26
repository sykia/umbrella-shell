#include "TaskbarSettings.h"
#include "core/taskbar/TrayOrder.h"
#include "core/taskbar/ComponentRegistry.h"
#include "core/taskbar/ResumeBudget.h"
#include "platform/windows/TaskbarState.h"
#include "platform/windows/TaskbarModuleSession.h"
#include "platform/windows/TaskbarRuntime.h"
#include <QTimer>
#include <QScopedValueRollback>
#include <QEventLoop>
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
bool legacyComponentConfiguration(const QVariantMap& configuration);
bool validDraft(const QVariantMap& draft);
QVariantMap readConfirmedComponents(QSettings& settings) {
    QVariantMap result;
    if(settings.contains("confirmedComponentsV2")) {
        const auto bytes=settings.value("confirmedComponentsV2").toByteArray();
        if(bytes.size()>65536)return {};
        const auto object=QJsonDocument::fromJson(bytes).object();
        if(object.size()>16)return {};
        for(auto i=object.begin();i!=object.end();++i) {
            const auto value=i.value().toObject().toVariantMap();
            if(i.key().isEmpty() || i.key().size()>256 || !validDraft(value))return {};
            result.insert(i.key(),value);
        }
    } else {
        const auto old=settings.value("confirmedComponentV1").toMap();
        const auto id=old["monitor"].toString();const auto value=old["configuration"].toMap();
        if(!id.isEmpty() && id.size()<=256 && validDraft(value))result.insert(id,value);
    }
    return result;
}
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
        && thickness >= 48 && thickness <= 96 && alpha >= 0 && alpha <= 100;
}
QString monitorId(const QScreen* screen) {
    for (const auto& entry : umbrella::platform::TaskbarRuntime::monitors()) {
        const auto monitor = entry.toMap();
        if (monitor.value("device").toString() == screen->name()) return monitor.value("id").toString();
    }
    return {}; // No persistent identity: do not guess another monitor's profile.
}
QStringList resumeOwners() {
    return umbrella::platform::TaskbarRuntime::panelOwners();
}
}

TaskbarSettings::TaskbarSettings(QObject* parent) : QObject(parent) {
    const auto componentLost=[this](const QString& device) {
        if(appearanceTransaction_)return; // The transaction verifies its own replay/rollback.
        QString target;
        for(const auto& entry:layoutMonitors())if(entry.toMap()["device"].toString()==device)target=entry.toMap()["id"].toString();
        if(!activeComponents_.contains(target))return;
        ++resumeEpoch_; // Loss must not let an older stability timer replenish replay.
        activeComponents_.remove(target);restored_=false;
        appearanceComponents_.remove(target);legacyTrayComponents_.remove(target);componentCatalogue_.remove(device);
        if(activeComponentMonitor_==target){activeComponentConfiguration_.clear();activeComponentMonitor_.clear();}
        status_=QStringLiteral("Explorer восстановил штатные позиции. Подтверждённый порядок сохранён; автоматический повтор в этом сеансе отключён.");emit changed();
    };
    componentSession_.componentLost=componentLost;appearance_.setComponentLossHandler(componentLost);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this] { componentCatalogue_.clear();emit changed(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { componentCatalogue_.clear();emit changed(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { componentCatalogue_.clear();emit changed(); });
    const auto state = umbrella::platform::taskbarAutoHide();
    available_ = state.has_value();
    autoHide_ = state.value_or(false);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    confirmedComponents_=readConfirmedComponents(settings);
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
    if (!found || pending_ || appearanceTransaction_) return;
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
    if (pending_ || appearanceTransaction_) { QTimer::singleShot(300, this, [this, freshSession] { restoreSavedAppearance(freshSession); }); return true; }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    ++resumeEpoch_;
    const bool appearanceRequested = settings.value("resumeAppearance", settings.contains("edge")).toBool();
    const bool requested = appearanceRequested || settings.value("resumeComponents", appearanceRequested && !confirmedComponents_.isEmpty()).toBool();
    if (freshSession && requested) {
        restored_ = false;
        bool valid = false;
        const int previous = settings.value("recovery/unsettledAttempts", 0).toInt(&valid);
        const int next = valid ? umbrella::taskbar::nextResumeAttempt(previous) : 0;
        if (!next) {
            restored_ = false;
            status_ = QStringLiteral("Автовосстановление остановлено после двух незавершённых попыток. Сохранённые профили оставлены; примените проверенные параметры вручную.");
            emit changed();
            return true; // Terminal refusal: runtime must not schedule another replay.
        }
        settings.setValue("recovery/unsettledAttempts", next);
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            status_ = QStringLiteral("Не удалось записать защиту восстановления; Windows не изменена.");
            emit changed();
            return false;
        }
    }
    const bool result = restoreSavedAppearanceImpl(freshSession);
    if (result && restored_)armStableResume();
    return result;
}
void TaskbarSettings::armStableResume() {
        const auto epoch=++resumeEpoch_;
        const auto owners = resumeOwners();
        // A native ack alone does not prove Explorer survived. A new generation
        // invalidates this single-shot callback before another replay can start.
        QTimer::singleShot(60000, this, [this, epoch, owners] {
            if (resumeEpoch_ != epoch || !restored_ || owners.isEmpty() || resumeOwners() != owners) return;
            QSettings stable(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
            stable.setValue("recovery/unsettledAttempts", 0);
            stable.sync(); // Failure leaves the conservative persisted budget.
        });
}
bool TaskbarSettings::restoreSavedAppearanceImpl(bool freshSession) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    restored_ = false;
    const bool restoreAppearance=settings.value("resumeAppearance",settings.contains("edge")).toBool();
    const bool restoreComponents=settings.value("resumeComponents",restoreAppearance && !confirmedComponents_.isEmpty()).toBool();
    if(!restoreAppearance && !restoreComponents)return true;
    const auto monitors = appearanceMonitors();
    if (monitors.isEmpty()) return false;
    available_ = umbrella::platform::taskbarAutoHide().has_value();
    if (!available_) return false;
    QString error;
    if(freshSession) {
        if(!componentSession_.stop(error) || !appearance_.restore(error)){status_=error;emit changed();return false;}
        activeComponents_.clear();appearanceComponents_.clear();legacyTrayComponents_.clear();
        activeComponentConfiguration_.clear();activeComponentMonitor_.clear();
    }
    const auto confirmed=restoreComponents?confirmedComponents_:QVariantMap{};
    if(!restoreAppearance)return replayConfirmedComponents();
    if(!settings.contains("resumeComponents") && confirmed.size()==1 && legacyComponentConfiguration(confirmed.first().toMap()) && confirmed.first().toMap()["left"].toStringList().contains("taskview")) {
        const bool applied=replayConfirmedComponents();
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
    if (targets.isEmpty()) {
        if(!confirmed.isEmpty())return replayConfirmedComponents();
        status_ = QStringLiteral("Сохранённые параметры сейчас несовместимы; файл настроек не изменён."); emit changed(); return true;
    }
    std::stable_sort(targets.begin(), targets.end(), [](const Target& a, const Target& b) { return a.primary && !b.primary; });
    bool autoHide = settings.value("autoHide", autoHide_).toBool();
    for (const auto& target : targets) {
        const auto& value = target.value;
        if (value["edge"].toString() != "bottom") autoHide = false;
    }
    const bool componentsCompatible = !autoHide && std::all_of(targets.cbegin(), targets.cend(), [](const Target& target) {
        const auto edge = target.value["edge"].toString();
        return edge == "bottom" || edge == "top";
    });
    const auto previousHide = umbrella::platform::taskbarAutoHide();
    if (previousHide.value_or(false) && !autoHide) umbrella::platform::requestTaskbarAutoHide(false);
    const auto& first = targets.front().value;
    // The baseline session captures and journals all panels before individual overrides.
    bool ok = appearance_.apply(first["edge"].toString(), first["detached"].toVariant().toBool(), first["transparency"].toVariant().toInt(),
        first["widthPercent"].toVariant().toInt(), first["thickness"].toVariant().toInt(), first["islands"].toVariant().toBool(), error);
    for (const auto& target : targets) {
        if (!ok) break;
        auto value = target.value;
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
    if(componentsCompatible && !confirmed.isEmpty() && !replayConfirmedComponents()) {
        restored_=false;emit changed();return true; // Report partial restore without replaying a refused operation.
    }
    emit changed();
    return true;
}
void TaskbarSettings::applyAutoHide(bool enabled) {
    if (pending_ || appearanceTransaction_) return;
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
    if (pending_ || appearanceTransaction_) return;
    if (islands) detached = true;
    if (!detached) thickness = 48;
    if (thickness < 48 || thickness > 96 || (islands && !widthAvailable())) {
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
    if (edge != "bottom" && edge != "top" && edge != "left" && edge != "right") return;
    if (transparency < 0 || transparency > 100) return;
    const bool componentsCompatible = (edge == "bottom" || edge == "top") && !autoHide;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const QStringList keys{"appearanceProfilesV1", "edge", "detached", "transparency", "widthPercent", "thickness", "islands", "autoHide", "resumeAppearance", "resumeComponents", "confirmedComponentV1", "confirmedComponentsV2"};
    const auto currentProfiles = appearanceProfiles(settings);
    const auto fallbackProfile = legacyAppearance(settings);
    const auto target=selectedAppearanceMonitor_;
    QString device;bool primary=false;
    for(const auto& entry:appearanceMonitors())if(entry.toMap()["id"].toString()==target) {
        device=entry.toMap()["device"].toString();primary=entry.toMap()["primary"].toBool();
    }
    auto profiles=currentProfiles;
    if(device.isEmpty() || (!profiles.contains(target) && profiles.size()>=16)) {
        status_=QStringLiteral("Монитор недоступен или превышен предел сохранённых профилей.");emit changed();return;
    }
    const auto oldHide=umbrella::platform::taskbarAutoHide();
    if(!oldHide){status_=QStringLiteral("Не удалось прочитать состояние панели до изменения.");emit changed();return;}
    if(autoHide && !activeComponents_.isEmpty()) {
        status_=QStringLiteral("Автоскрытие пока несовместимо с активным переносом компонентов.");emit changed();return;
    }
    QVariantMap previous;
    for(const auto& key:keys)if(settings.contains(key))previous.insert(key,settings.value(key));
    const auto oldActive=activeComponents_;
    auto layouts=oldActive;
    const bool resumeComponents=settings.value("resumeComponents",settings.value("resumeAppearance",settings.contains("edge")).toBool() && !confirmedComponents_.isEmpty()).toBool();
    if(resumeComponents && confirmedComponents_.contains(target))layouts.insert(target,confirmedComponents_[target]);
    const bool hadAppearance=appearance_.nativeSessionActive();
    const bool migrating=!hadAppearance && componentSession_.running();
    QScopedValueRollback<bool> transaction(appearanceTransaction_,true);
    QString error;bool styleTouched=false,hideTouched=false;
    std::optional<umbrella::platform::TaskbarProfile> priorProfile;
    const auto changeHide=[&](bool requested) {
        if(!umbrella::platform::requestTaskbarAutoHide(requested))return false;
        pending_=true;
        QEventLoop wait;QTimer::singleShot(250,&wait,&QEventLoop::quit);wait.exec();pending_=false;
        const auto measured=umbrella::platform::taskbarAutoHide();
        return measured && *measured==requested;
    };
    const auto replay=[&](const QVariantMap& requested) {
        bool ok=true;
        for(auto i=requested.begin();i!=requested.end();++i) {
            if(applyNativeComponents(i.key(),i.value().toMap()))activeComponents_.insert(i.key(),i.value());
            else {activeComponents_.remove(i.key());ok=false;}
        }
        return ok;
    };
    const auto restore=[&] {
        bool ok=true;QString rollback;
        if(!hadAppearance) {
            ok=appearance_.restore(rollback);
            activeComponents_.clear();appearanceComponents_.clear();legacyTrayComponents_.clear();
            if(ok)ok=replay(oldActive);
        } else {
            if(styleTouched && priorProfile) {
                const auto& p=*priorProfile;
                if(!appearance_.placeComponents("slots "+device.toLatin1()+" reset\n",device,rollback))ok=false;
                if(!appearance_.applyMonitor(device,p.edge,p.detached,p.transparency,p.widthPercent,p.thickness,p.islands,rollback))ok=false;
            }
            if(oldActive.contains(target)) {
                if(!replay(QVariantMap{{target,oldActive[target]}}))ok=false;
            } else activeComponents_.remove(target);
        }
        if(hideTouched && !changeHide(*oldHide))ok=false;
        if(activeComponentMonitor_==target) {
            activeComponentConfiguration_=activeComponents_.value(target).toMap();
            if(activeComponentConfiguration_.isEmpty())activeComponentMonitor_.clear();
        }
        return ok;
    };
    const auto refuse=[&](const QString& reason) {
        const bool restored=restore();
        status_=restored?reason+QStringLiteral(" Предыдущая конфигурация восстановлена."):
            reason+QStringLiteral(" Откат не подтверждён; повторное применение не запускается.");
        emit changed();
    };
    if(migrating) {
        if(!componentSession_.stop(error)){status_=error;emit changed();return;}
        activeComponents_.clear();appearanceComponents_.clear();legacyTrayComponents_.clear();
    }
    if(!appearance_.prepare(error)){refuse(error);return;}
    priorProfile=appearance_.monitorProfile(device);
    if(!priorProfile) {refuse(QStringLiteral("Исходное оформление выбранной панели нельзя точно представить для отката."));return;}
    if (hadAppearance) {
        const auto toReset = componentsCompatible ? QStringList{target} : oldActive.keys();
        for (const auto& monitor : toReset)
            if (oldActive.contains(monitor) && !resetNativeComponents(monitor)) { refuse(status_); return; }
    }
    if(*oldHide!=autoHide) {
        hideTouched=true;
        if(!changeHide(autoHide)){refuse(QStringLiteral("Windows не подтвердила автоскрытие."));return;}
    }
    styleTouched=true;
    if(!appearance_.apply(edge,detached,transparency,widthPercent,thickness,islands,error)){refuse(error);return;}
    QVariantMap toReplay;
    if(componentsCompatible) {
        if(migrating)toReplay=layouts;
        else if(layouts.contains(target))toReplay.insert(target,layouts[target]);
    }
    if(!replay(toReplay)){refuse(status_.isEmpty()?QStringLiteral("Новая геометрия не вмещает подтверждённый перенос."):status_);return;}
    const QJsonObject value{{"edge",edge},{"detached",detached},{"transparency",transparency},
        {"widthPercent",widthPercent},{"thickness",thickness},{"islands",islands}};
    for (const auto& entry : appearanceMonitors()) profiles[entry.toMap()["id"].toString()]=value;
    settings.setValue("appearanceProfilesV1",QJsonDocument(profiles).toJson(QJsonDocument::Compact));
    if(primary)for(auto i=value.begin();i!=value.end();++i)settings.setValue(i.key(),i.value().toVariant());
    settings.setValue("autoHide",autoHide);settings.setValue("resumeAppearance",true);settings.sync();
    if(settings.status()!=QSettings::NoError) {
        for(const auto& key:keys)if(previous.contains(key))settings.setValue(key,previous[key]);else settings.remove(key);
        settings.sync();refuse(QStringLiteral("Не удалось сохранить настройки."));return;
    }
    edge_=edge;detached_=detached;transparency_=transparency;widthPercent_=widthPercent;thickness_=thickness;islands_=islands;autoHide_=autoHide;
    if(activeComponentMonitor_==target)activeComponentConfiguration_=activeComponents_.value(target).toMap();
    restored_=true;
    armStableResume();
    status_=setResumeAtLogin(true)?(componentsCompatible
        ? QStringLiteral("Оформление и подтверждённый порядок применены.")
        : QStringLiteral("Оформление применено; сохранённый порядок компонентов временно приостановлен.")):
        QStringLiteral("Изменения сохранены, но записать автозапуск не удалось.");
    emit changed();
}
bool TaskbarSettings::widthAvailable() const { return umbrella::platform::TaskbarModuleSession::available(); }
bool TaskbarSettings::sidesAvailable() const { return umbrella::platform::TaskbarModuleSession::layoutAvailable(); }
void TaskbarSettings::resetAppearance() {
    if (pending_ || appearanceTransaction_) return;
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
    if(appearanceTransaction_)return false;
    const auto owned=appearanceComponents_;
    bool ok=true;
    for(const auto& monitor:owned)if(!resetNativeComponents(monitor))ok=false;
    QString error;if(!componentSession_.stop(error)){status_=error;ok=false;}
    if(!ok){emit changed();return false;}
    activeComponents_.clear();confirmedComponents_.clear();appearanceComponents_.clear();legacyTrayComponents_.clear();
    activeComponentConfiguration_.clear();activeComponentMonitor_.clear();
    ok=persistConfirmedComponents();
    status_=ok?QStringLiteral("Исходные позиции компонентов восстановлены."):QStringLiteral("Не удалось сохранить сброс размещения компонентов.");
    emit changed();return ok;
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
// Native groups retain their original objects. Quick Settings is indivisible
// and Show Desktop remains at the physical edge.
std::optional<unsigned> componentTrayOrder(const QJsonArray& right) {
    if(right.size()!=6 || right.last()!=QStringLiteral("desktop"))return {};
    umbrella::taskbar::TrayOrder order{};unsigned count=0;
    for(qsizetype i=0;i<right.size()-1;++i) {
        if(count>=order.size())return {};
        const auto id=right[i].toString();
        if(id=="overflow")order[count++]=0;
        else if(id=="input")order[count++]=1;
        else if(id=="clock")order[count++]=3;
        else if(id=="network" && i+1<right.size()-1 && right[i+1]==QStringLiteral("volume")){order[count++]=2;++i;}
        else return {};
    }
    return count==4 ? umbrella::taskbar::trayOrderCode(order) : std::nullopt;
}
bool legacyComponentConfiguration(const QVariantMap& configuration) {
    if (!validDraft(configuration)) return false;
    const auto value = QJsonObject::fromVariantMap(configuration);
    const auto expected = QJsonDocument::fromJson(R"({
        "left":["widgets","taskview"],
        "center":["start","search","apps"],
        "right":["overflow","input","network","volume","clock","desktop"],
        "available":["trayapps","appoverflow","power","pen","keyboard","touchpad","emoji","studio","microphone","callmute","privacy","notifications","eject","vpn","chat","copilot","systemtray","remote"]
    })").object();
    if(value.value("available")!=expected.value("available"))return false;
    const bool taskViewLeft=value.value("left")==expected.value("left") && value.value("center")==expected.value("center");
    const bool taskViewDefault=value.value("left")==QJsonArray{"widgets"} && value.value("center")==QJsonArray{"start","search","taskview","apps"};
    return (taskViewLeft || taskViewDefault) && componentTrayOrder(value.value("right").toArray()).has_value();
}
bool supportedComponentConfiguration(const QVariantMap& configuration) {
    if(!validDraft(configuration))return false;
    const auto value=QJsonObject::fromVariantMap(configuration);
    static const auto catalogue=QStringLiteral("widgets start search taskview apps overflow input network volume clock desktop trayapps appoverflow power pen keyboard touchpad emoji studio microphone callmute privacy notifications eject vpn chat copilot systemtray remote").split(' ');
    static const QSet<QString> supported{"widgets","start","search","taskview","apps","overflow","input","network","volume","clock","desktop","trayapps","systemtray"};
    QSet<QString> ids;unsigned active=0;
    for(const auto zone:{"available","left","center","right"}) {
        const auto entries=value[zone].toArray();
        for(qsizetype i=0;i<entries.size();++i) {
            const auto id=entries[i].toString();if(!catalogue.contains(id))return false;ids.insert(id);
            if(!strcmp(zone,"available"))continue;
            if(!supported.contains(id))return false;++active;
            if(id=="network" && (i+1>=entries.size() || entries[i+1]!=QStringLiteral("volume")))return false;
            if(id=="volume" && (!i || entries[i-1]!=QStringLiteral("network")))return false;
            if(id=="desktop" && (strcmp(zone,"right") || i!=entries.size()-1))return false;
        }
    }
    return active && ids.size()==catalogue.size();
}
}
bool TaskbarSettings::persistConfirmedComponents() {
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    const auto bytes=QJsonDocument(QJsonObject::fromVariantMap(confirmedComponents_)).toJson(QJsonDocument::Compact);
    if(confirmedComponents_.size()>16 || bytes.size()>65536)return false;
    // Keep the last-record v1 mirror for older builds; v2 is authoritative.
    settings.setValue("confirmedComponentsV2",bytes);
    settings.setValue("resumeComponents",!confirmedComponents_.isEmpty());
    if(confirmedComponents_.isEmpty())settings.remove("confirmedComponentV1");
    else {
        const auto id=confirmedComponents_.contains(activeComponentMonitor_)?activeComponentMonitor_:confirmedComponents_.firstKey();
        settings.setValue("confirmedComponentV1",QVariantMap{{"monitor",id},{"configuration",confirmedComponents_[id]}});
    }
    settings.sync();return settings.status()==QSettings::NoError;
}
bool TaskbarSettings::replayConfirmedComponents() {
    const auto records=confirmedComponents_;bool ok=true;QStringList unavailable;
    for(const auto& entry:layoutMonitors()) {
        const auto id=entry.toMap()["id"].toString();const auto configuration=records.value(id).toMap();
        if(!configuration.isEmpty() && !applyComponentConfiguration(id,configuration)) {
            if(appearance_.nativeSessionActive())unavailable.append(entry.toMap()["label"].toString());
            else ok=false;
        }
    }
    restored_=appearance_.nativeSessionActive() || !activeComponents_.isEmpty();
    if(!unavailable.isEmpty())status_=QStringLiteral("Оформление восстановлено. Сохранённый порядок временно несовместим с режимом панели: ")+unavailable.join(", ");
    return ok && unavailable.isEmpty();
}
QStringList TaskbarSettings::componentIds(const QString& monitor,bool refresh) {
    QString device;bool primary=false;
    for(const auto& entry:layoutMonitors())if(entry.toMap()["id"].toString()==monitor){device=entry.toMap()["device"].toString();primary=entry.toMap()["primary"].toBool();}
    if(device.isEmpty())return {};
    if(refresh || !componentCatalogue_.contains(device)) {
        QHash<QString,QStringList> retainedHidden;
        for(const auto& entry:layoutMonitors()) {
            const auto current=entry.toMap();
            for(const auto& value:confirmedComponents_.value(current["id"].toString()).toMap()["available"].toList()) {
                auto id=value.toString();if(id=="volume")continue;if(id=="network")id="quicksettings";
                if(umbrella::taskbar::componentFromId(id.toStdWString())!=umbrella::taskbar::Component::Unknown)
                    retainedHidden[current["device"].toString()].append(id);
            }
        }
        QString error;const auto snapshot=umbrella::platform::TaskbarModuleSession::discoverComponents(error,retainedHidden);
        if(snapshot.isEmpty()){status_=error;return {};}
        componentCatalogue_=snapshot;
    }
    auto ids=componentCatalogue_.value(device);
    // Secondary Show Desktop is a noninteractive edge stack on the verified
    // build. Preserve its native slot, never advertise a draggable substitute.
    if(!primary)ids.removeAll("desktop");
    return ids;
}
bool TaskbarSettings::applyNativeComponents(const QString& monitor,const QVariantMap& configuration) {
    bool primary=false;QString device;
    for(const auto& entry:layoutMonitors())if(entry.toMap()["id"].toString()==monitor){primary=entry.toMap()["primary"].toBool();device=entry.toMap()["device"].toString();}
    if(pending_ || device.isEmpty()){status_=QStringLiteral("Монитор недоступен или выполняется другое изменение.");return false;}
    QByteArray command="slots "+device.toLatin1()+' '+QByteArray::number(configuration["gap"].toInt());
    int zoneIndex=0;unsigned count=0;
    for(const auto zone:{"left","center","right"}) {
        for(const auto& entry:configuration[zone].toList()) {
            auto id=entry.toString();if(id=="volume" || (!primary && id=="desktop"))continue;if(id=="network")id="quicksettings";
            command+=' '+id.toLatin1()+':'+QByteArray::number(zoneIndex);++count;
        }
        ++zoneIndex;
    }
    const auto detected=componentIds(monitor);
    for(const auto& entry:configuration["available"].toList()) {
        auto id=entry.toString();
        if(!detected.contains(id) || id=="desktop" || id=="volume")continue;
        if(id=="network")id="quicksettings";
        if(umbrella::taskbar::componentFromId(id.toStdWString())==umbrella::taskbar::Component::Unknown)continue;
        command+=' '+id.toLatin1()+":3";++count;
    }
    if(!count){status_=QStringLiteral("Нет доступных исходных компонентов для переноса.");return false;}
    command+='\n';QString error;
    bool ok=appearance_.nativeSessionActive()?appearance_.placeComponents(command,device,error):componentSession_.placeComponents(command,device,error);
    legacyTrayComponents_.remove(monitor);
    // Preserve the existing four-group adapter for styled primary taskbars.
    // Its edit is scoped to this HWND and cannot reset another monitor.
    if(!ok && primary && appearance_.nativeSessionActive() && legacyComponentConfiguration(configuration) && !configuration["left"].toStringList().contains("taskview")) {
        const auto order=componentTrayOrder(QJsonArray::fromVariantList(configuration["right"].toList()));
        ok=order && appearance_.reorderTray(device,*order,error);
        if(ok)legacyTrayComponents_.insert(monitor);
    }
    if(ok){if(appearance_.nativeSessionActive())appearanceComponents_.insert(monitor);else appearanceComponents_.remove(monitor);}
    else status_=error;
    return ok;
}
bool TaskbarSettings::resetNativeComponents(const QString& monitor) {
    QString device,error;
    for(const auto& entry:layoutMonitors())if(entry.toMap()["id"].toString()==monitor)device=entry.toMap()["device"].toString();
    if(device.isEmpty()){status_=QStringLiteral("Монитор недоступен для подтверждения сброса.");return false;}
    const auto command="slots "+device.toLatin1()+" reset\n";
    const bool ok=legacyTrayComponents_.contains(monitor)?appearance_.reorderTray(device,0,error):
        appearanceComponents_.contains(monitor)?appearance_.placeComponents(command,device,error):componentSession_.placeComponents(command,device,error);
    if(!ok){status_=error;return false;}
    appearanceComponents_.remove(monitor);legacyTrayComponents_.remove(monitor);return true;
}
bool TaskbarSettings::applyComponentConfiguration(const QString& monitor,const QVariantMap& configuration) {
    if(appearanceTransaction_)return false;
    if(!supportedComponentConfiguration(configuration)){status_=QStringLiteral("Недопустимая конфигурация компонентов.");emit changed();return false;}
    auto proposed=confirmedComponents_;proposed.insert(monitor,configuration);
    if(proposed.size()>16 || QJsonDocument(QJsonObject::fromVariantMap(proposed)).toJson(QJsonDocument::Compact).size()>65536)return false;
    const auto previous=activeComponents_.value(monitor).toMap();
    const auto previousConfirmed=confirmedComponents_;
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    const QStringList keys{"confirmedComponentsV2","confirmedComponentV1","componentDraftsV1","resumeAppearance","resumeComponents"};
    QVariantMap previousSettings;
    for(const auto& key:keys)if(settings.contains(key))previousSettings.insert(key,settings.value(key));
    if(!applyNativeComponents(monitor,configuration)) {
        const auto error=status_;
        const bool preserved=appearance_.nativeSessionActive()?appearance_.previousPlacementPreserved():componentSession_.previousPlacementPreserved();
        if(!previous.isEmpty() && !preserved && !applyNativeComponents(monitor,previous)) {
            activeComponents_.remove(monitor);
            if(activeComponentMonitor_==monitor){activeComponentMonitor_.clear();activeComponentConfiguration_.clear();}
            status_=QStringLiteral("Перемещение и восстановление прежней позиции не подтверждены. Прежний порядок сохранён; повтор в этом сеансе отключён.");
        } else status_=error;
        emit changed();return false;
    }
    activeComponents_.insert(monitor,configuration);confirmedComponents_=proposed;
    activeComponentConfiguration_=configuration;activeComponentMonitor_=monitor;
    bool saved=persistConfirmedComponents() && saveComponentDraft(monitor,configuration);
    // Saving component-only placement must not resurrect an old disabled
    // islands/size profile at the next launch.
    settings.setValue("resumeComponents",true);
    settings.sync();saved=saved && settings.status()==QSettings::NoError;
    if(!saved) {
        bool rolledBack=previous.isEmpty()?resetNativeComponents(monitor):applyNativeComponents(monitor,previous);
        if(previous.isEmpty() || !rolledBack)activeComponents_.remove(monitor);else activeComponents_.insert(monitor,previous);
        confirmedComponents_=previousConfirmed;
        for(const auto& key:keys)if(previousSettings.contains(key))settings.setValue(key,previousSettings[key]);else settings.remove(key);
        settings.sync();
        activeComponentConfiguration_=activeComponents_.value(monitor).toMap();
        activeComponentMonitor_=activeComponentConfiguration_.isEmpty()?QString():monitor;
        status_=rolledBack?QStringLiteral("Не удалось сохранить перенос; прежние позиции восстановлены."):
            QStringLiteral("Ошибка сохранения; откат позиций не подтверждён.");
        emit changed();return false;
    }
    status_=setResumeAtLogin(true)?QString():QStringLiteral("Расположение сохранено, но записать автозапуск не удалось.");
    emit changed();return true;
}
bool TaskbarSettings::restoreComponentConfiguration(const QString& monitor) {
    bool found = false;
    for (const auto& entry : layoutMonitors()) if (entry.toMap().value("id").toString() == monitor) found = true;
    if (!found || pending_ || appearanceTransaction_) return false;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const bool confirmedTarget=confirmedComponents_.contains(monitor);
    if (activeComponents_.contains(monitor) && !resetNativeComponents(monitor)) {
        qWarning().noquote() << "Component restore:" << status_;
        status_ = QStringLiteral("Windows не подтвердила восстановление."); emit changed(); return false;
    }
    auto all = readDrafts(settings); all.remove(monitor);
    QString label;
    for (const auto& entry : layoutMonitors()) if (entry.toMap()["id"].toString() == monitor) label = entry.toMap()["label"].toString();
    int matchingScreens = 0;
    for (const auto screen : QGuiApplication::screens()) if (screen->name() == label) ++matchingScreens;
    if (matchingScreens == 1) for (const auto screen : QGuiApplication::screens()) if (screen->name() == label) all.remove(monitorId(screen));
    activeComponents_.remove(monitor);confirmedComponents_.remove(monitor);
    if(activeComponentMonitor_==monitor){activeComponentMonitor_.clear();activeComponentConfiguration_.clear();}
    const bool persisted=persistConfirmedComponents();
    settings.setValue("componentDraftsV1",QJsonDocument(all).toJson(QJsonDocument::Compact));
    settings.sync();
    const bool resume=settings.value("resumeAppearance",false).toBool() || !confirmedComponents_.isEmpty();
    const bool saved=persisted && settings.status()==QSettings::NoError && (!confirmedTarget || setResumeAtLogin(resume));
    status_ = saved ? QString() : QStringLiteral("Исходный вид возвращён, но сохранённые настройки не удалены.");
    emit changed(); return saved;
}
