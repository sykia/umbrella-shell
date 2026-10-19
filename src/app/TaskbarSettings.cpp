#include "TaskbarSettings.h"
#include "platform/windows/TaskbarState.h"
#include "platform/windows/TaskbarModuleSession.h"
#include <QTimer>
#include <QSettings>

TaskbarSettings::TaskbarSettings(QObject* parent) : QObject(parent) {
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
    QString error;
    status_ = appearance_.restore(error)
        ? QStringLiteral("Исходное расположение и оформление восстановлены.") : error;
    emit changed();
}
