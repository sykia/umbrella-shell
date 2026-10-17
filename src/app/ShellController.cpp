#include "ShellController.h"
#include "platform/windows/TaskbarVisibility.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QScreen>
#include <QTimer>
#include <QDir>
#include <QFont>
#include <QPalette>
#include "core/PanelConfiguration.h"
using namespace umbrella::platform;
namespace {
QString executable() { return QDir::toNativeSeparators(QCoreApplication::applicationFilePath()); }
void load(QQuickView& view, ShellController* controller, const QString& name) {
    view.rootContext()->setContextProperty("shell", controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/qt/qml/Umbrella/" + name + ".qml"));
}
}
ShellController::ShellController(QObject* parent) : QObject(parent),
    settings_(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Shell") {
    const auto applyAppearance = [this] {
        QGuiApplication::setFont(QFont(preferences_.fontFamily()));
        const auto theme = preferences_.theme();
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(theme.value("background").toString()));
        palette.setColor(QPalette::Base, QColor(theme.value("surface").toString()));
        palette.setColor(QPalette::Button, QColor(theme.value("raised").toString()));
        for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) palette.setColor(role, QColor(theme.value("ink").toString()));
        palette.setColor(QPalette::Highlight, QColor(theme.value("accent").toString()));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        QGuiApplication::setPalette(palette);
    };
    connect(&preferences_, &Preferences::appearanceChanged, this, applyAppearance);
    applyAppearance();
    const auto savedEdge = settings_.value("panel/edge", "top").toString();
    if (umbrella::core::validPanelEdge(savedEdge.toStdString())) edge_ = savedEdge;
    bool validCount = false;
    const int savedCount = settings_.value("panel/workspaceCount", 5).toInt(&validCount);
    if (validCount && umbrella::core::validWorkspaceCount(savedCount)) workspaceCount_ = savedCount;
    workspaces_.setCount(workspaceCount_);
    for (auto it = zones_.begin(); it != zones_.end(); ++it) {
        bool valid = false; const int zone = settings_.value("panel/modules/" + it.key(), it.value()).toInt(&valid);
        if (valid && umbrella::core::validZone(zone)) it.value() = zone;
    }
    connect(&workspaces_, &MonitorWorkspaces::changed, this, &ShellController::changed);
    connect(&workspaces_, &MonitorWorkspaces::failed, this, &ShellController::fail);
    connect(&actions_, &PanelActions::layoutChanged, this, &ShellController::changed);
    enabled_ = settings_.value("shell/replacement", true).toBool();
    connect(&session_, &TaskbarSession::recoveryRequested, this, [this] {
        restore(); enabled_ = false;
        settings_.setValue("shell/replacement", false); settings_.sync();
        bars_.clear(); qApp->quit();
    });
    connect(&session_, &TaskbarSession::suppressionFailed, this, [this](const QString& reason) {
        restore(); enabled_ = false; bars_.clear();
        settings_.setValue("shell/replacement", false); settings_.sync(); fail(reason);
    });
    connect(&session_, &TaskbarSession::sessionEnding, this, &ShellController::restore);
    // TaskbarCreated is broadcast to every top-level window, including settings.
    // Coalesce one Explorer restart instead of registering every bar N times.
    explorerRefresh_.setSingleShot(true);
    explorerRefresh_.setInterval(100);
    connect(&explorerRefresh_, &QTimer::timeout, this, [this] {
        if (enabled_ && !rebuilding_) {
            if (active_) registerBars(); else rebuild();
        }
    });
    connect(&session_, &TaskbarSession::explorerRestarted, this, [this] {
        if (enabled_) explorerRefresh_.start();
    });
    connect(&session_, &TaskbarSession::positionChanged, this, [this](quintptr window) {
        for (auto& bar : bars_) if (bar->native && bar->view->winId() == window)
            bar->native->position(36, bar->view->devicePixelRatio(), nativeEdge());
    }, Qt::QueuedConnection);
    topologyRefresh_.setSingleShot(true);
    topologyRefresh_.setInterval(150);
    connect(&topologyRefresh_, &QTimer::timeout, this, &ShellController::rebuild);
}
ShellController::~ShellController() { restore(); bars_.clear(); }
bool ShellController::start() {
    if (!session_.acquire()) return false;
    if (settings_.value("recovery/pending", false).toBool()) {
        if (!restoreTaskbarWindows() || !setAutoHide(settings_.value("recovery/autoHide", false).toBool())) {
            enabled_ = false; fail(tr("Не удалось восстановить состояние предыдущего запуска."));
        } else { settings_.remove("recovery"); settings_.sync(); }
    }
    if (!session_.hotkeyAvailable()) {
        fail(tr("Ctrl+Alt+U занято. Используйте кнопку восстановления или --restore-taskbar."));
    }
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen*) { topologyRefresh_.start(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) { topologyRefresh_.start(); });
    if (enabled_) rebuild(); else openSettings();
    return true;
}
bool ShellController::startup() const { return startupEnabled(executable().toStdWString()); }
void ShellController::setStartup(bool enabled) {
    if (!umbrella::platform::setStartup(enabled, executable().toStdWString())) fail(tr("Не удалось изменить автозапуск."));
    emit changed();
}
void ShellController::setReplacement(bool enabled) {
    if (enabled_ == enabled) return;
    settings_.setValue("shell/replacement", enabled); settings_.sync();
    if (settings_.status() != QSettings::NoError) { fail(tr("Не удалось сохранить настройки.")); return; }
    enabled_ = enabled;
    if (enabled) rebuild(); else { restore(); bars_.clear(); status_ = tr("Панель Windows включена."); }
    emit changed();
}
void ShellController::fail(const QString& error) { status_ = error; emit changed(); openSettings(); }
void ShellController::openSettings() {
    if (!settingsView_) {
        settingsView_ = std::make_unique<QQuickView>();
        settingsView_->setFlags(Qt::Window | Qt::FramelessWindowHint);
        settingsView_->setColor(Qt::transparent);
        settingsView_->setMinimumSize(QSize(680, 440)); settingsView_->setMaximumSize(QSize(680, 440));
        settingsView_->setTitle(tr("Umbrella Shell — Настройки"));
        load(*settingsView_, this, "Settings"); settingsView_->resize(680, 440);
    }
    settingsView_->show(); settingsView_->raise(); settingsView_->requestActivate();
}
void ShellController::quit() { restore(); qApp->quit(); }
void ShellController::restore() {
    workspaces_.stop();
    actions_.setEnabled(false);
    const bool wasActive = active_; active_ = false;
    // Release our work-area reservations before restoring Explorer's own area.
    for (auto& bar : bars_) bar->native.reset();
    if (session_.restore()) {
        if (wasActive) { settings_.remove("recovery"); settings_.sync(); }
    } else status_ = tr("Windows не подтвердила восстановление. Выполните --restore-taskbar.");
}
void ShellController::rebuild() {
    if (rebuilding_ || !enabled_) return;
    rebuilding_ = true;
    restore(); bars_.clear();
    for (auto* screen : QGuiApplication::screens()) {
        // Unique connections are not available for lambdas: reconnect this receiver only.
        disconnect(screen, nullptr, this, nullptr);
        connect(screen, &QScreen::geometryChanged, this, [this] { topologyRefresh_.start(); });
        connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this] { topologyRefresh_.start(); });
        auto bar = std::make_unique<Bar>();
        bar->view = std::make_unique<QQuickView>();
        auto* view = bar->view.get();
        view->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        view->setTitle("Umbrella Shell Bar — " + screen->name());
        view->setScreen(screen);
        const QRect rect = screen->geometry();
        if (edge_ == "top") view->setGeometry(rect.x(), rect.y(), rect.width(), 36);
        else if (edge_ == "left") view->setGeometry(rect.x(), rect.y(), 36, rect.height());
        else if (edge_ == "right") view->setGeometry(rect.right() - 35, rect.y(), 36, rect.height());
        else view->setGeometry(rect.x(), rect.bottom() - 35, rect.width(), 36);
        view->rootContext()->setContextProperty("panelId", QString::number(view->winId()));
        load(*view, this, "Bar");
        if (view->status() == QQuickView::Error) { rebuilding_ = false; bars_.clear(); fail(tr("Ошибка загрузки QML панели.")); return; }
        Bar* state = bar.get();
        connect(view, &QQuickWindow::frameSwapped, view, [this, state] {
            if (!state->painted) { state->painted = true; activate(); }
        }, Qt::QueuedConnection);
        connect(view, &QQuickWindow::sceneGraphError, this, [this](QQuickWindow::SceneGraphError, const QString& message) {
            restore(); fail(tr("Ошибка графики: ") + message);
        });
        view->show();
        bars_.push_back(std::move(bar));
    }
    rebuilding_ = false;
}
void ShellController::activate() {
    if (rebuilding_ || active_ || !enabled_ || bars_.empty()) return;
    for (const auto& bar : bars_) if (!bar->painted) return;
    if (!session_.prepare()) {
        restore(); fail(tr("Процесс восстановления не готов. Панель Windows сохранена.")); return;
    }
    settings_.setValue("recovery/autoHide", session_.originalAutoHide());
    settings_.setValue("recovery/pending", true); settings_.sync();
    if (settings_.status() != QSettings::NoError) { restore(); fail(tr("Не удалось сохранить журнал восстановления.")); return; }
    active_ = true;
    if (!session_.hide()) { restore(); fail(tr("Не удалось подтвердить скрытие панелей Windows.")); return; }
    if (!registerBars()) return;
    actions_.setEnabled(zones_.value("keyboard").toInt() >= 0);
    if (zones_.value("workspaces").toInt() >= 0 && !workspaces_.start(session_.workspaceSnapshotName())) {
        restore(); fail(tr("Не удалось подготовить восстановление пространств.")); return;
    }
    status_ = session_.hotkeyAvailable()
        ? tr("Панель Umbrella работает. Ctrl+Alt+U — аварийный выход.")
        : tr("Панель Umbrella работает. Ctrl+Alt+U занято; используйте кнопку выхода или --restore-taskbar.");
    emit changed();
}

bool ShellController::registerBars() {
    for (auto& bar : bars_) {
        bar->native.reset();
        bar->native = std::make_unique<DesktopBar>(bar->view->winId());
        if (!bar->native->registered() || !bar->native->position(36, bar->view->devicePixelRatio(), nativeEdge())) {
            restore(); for (auto& item : bars_) item->native.reset();
            fail(tr("Не удалось зарезервировать место панели.")); return false;
        }
    }
    return true;
}

BarEdge ShellController::nativeEdge() const {
    if (edge_ == "top") return BarEdge::Top;
    if (edge_ == "left") return BarEdge::Left;
    if (edge_ == "right") return BarEdge::Right;
    return BarEdge::Bottom;
}
void ShellController::setPanelEdge(const QString& edge) {
    if (edge == edge_ || !umbrella::core::validPanelEdge(edge.toStdString())) return;
    settings_.setValue("panel/edge", edge); settings_.sync();
    if (settings_.status() != QSettings::NoError) { fail(tr("Не удалось сохранить расположение.")); return; }
    edge_ = edge; emit changed(); if (enabled_) rebuild();
}
void ShellController::setModuleZone(const QString& module, int zone) {
    if (!umbrella::core::validModule(module.toStdString()) || !umbrella::core::validZone(zone)) return;
    settings_.setValue("panel/modules/" + module, zone); settings_.sync();
    if (settings_.status() != QSettings::NoError) { fail(tr("Не удалось сохранить конфигурацию.")); return; }
    zones_[module] = zone;
    if (active_ && module == "workspaces") {
        if (zone < 0) workspaces_.stop();
        else if (!workspaces_.running() && !workspaces_.start(session_.workspaceSnapshotName())) fail(tr("Не удалось включить пространства."));
    }
    actions_.setEnabled(active_ && zones_.value("keyboard").toInt() >= 0); emit changed();
}
void ShellController::toggleSettings() {
    if (settingsView_ && settingsView_->isVisible()) settingsView_->hide(); else openSettings();
}
void ShellController::setWorkspaceCount(int count) {
    if (count == workspaceCount_ || !umbrella::core::validWorkspaceCount(count)) return;
    const int previous = workspaceCount_;
    settings_.setValue("panel/workspaceCount", count); settings_.sync();
    if (settings_.status() != QSettings::NoError) { fail(tr("Не удалось сохранить количество пространств.")); return; }
    if (!workspaces_.setCount(count)) {
        workspaces_.setCount(previous);
        settings_.setValue("panel/workspaceCount", previous); settings_.sync();
        fail(tr("Не удалось вернуть окна перед изменением количества пространств.")); return;
    }
    workspaceCount_ = count; emit changed();
}
void ShellController::cycleKeyboard() { if (!actions_.cycleLayout()) fail(tr("Не удалось сменить раскладку активного окна. Проверьте установленные языки и права окна.")); }
void ShellController::openSound() { if (!actions_.openSound()) fail(tr("Не удалось открыть параметры звука Windows.")); }
void ShellController::moveDesktop(int direction) { if (!actions_.moveDesktop(direction)) fail(tr("Отпустите клавиши-модификаторы и повторите переключение рабочего стола.")); }
void ShellController::showDesktops() { if (!actions_.showDesktops()) fail(tr("Не удалось открыть представление рабочих столов.")); }

QVariantList ShellController::workspaceModel(const QString& panel) const {
    bool ok = false; const auto value = panel.toULongLong(&ok);
    return ok ? workspaces_.model(static_cast<quintptr>(value)) : QVariantList{};
}
void ShellController::selectWorkspace(const QString& panel, int index) {
    bool ok = false; const auto value = panel.toULongLong(&ok);
    if (!ok || !workspaces_.select(static_cast<quintptr>(value), index)) fail(tr("Не удалось переключить пространство монитора."));
}
