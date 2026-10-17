#include "ShellController.h"
#include "platform/windows/TaskbarVisibility.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QScreen>
#include <QTimer>
#include <QDir>
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
    enabled_ = settings_.value("shell/replacement", true).toBool();
    connect(&session_, &TaskbarSession::recoveryRequested, this, [this] {
        restore(); enabled_ = false;
        settings_.setValue("shell/replacement", false); settings_.sync();
        bars_.clear(); qApp->quit();
    });
    connect(&session_, &TaskbarSession::sessionEnding, this, &ShellController::restore);
    connect(&session_, &TaskbarSession::explorerRestarted, this, [this] {
        if (enabled_) QTimer::singleShot(0, this, [this] {
            if (active_) registerBars(); else rebuild();
        });
    });
    connect(&session_, &TaskbarSession::positionChanged, this, [this](quintptr window) {
        for (auto& bar : bars_) if (bar->native && bar->view->winId() == window)
            bar->native->position(56, bar->view->devicePixelRatio());
    });
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
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen*) { rebuild(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) { rebuild(); });
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
        settingsView_->setTitle(tr("Umbrella Shell — Настройки"));
        load(*settingsView_, this, "Settings"); settingsView_->resize(680, 440);
    }
    settingsView_->show(); settingsView_->raise(); settingsView_->requestActivate();
}
void ShellController::quit() { restore(); qApp->quit(); }
void ShellController::restore() {
    const bool wasActive = active_; active_ = false;
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
        connect(screen, &QScreen::geometryChanged, this, [this] { rebuild(); });
        connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this] { rebuild(); });
        auto bar = std::make_unique<Bar>();
        bar->view = std::make_unique<QQuickView>();
        auto* view = bar->view.get();
        view->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        view->setTitle("Umbrella Shell Bar — " + screen->name());
        view->setScreen(screen);
        const QRect rect = screen->geometry();
        view->setGeometry(rect.x(), rect.bottom() - 55, rect.width(), 56);
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
    status_ = session_.hotkeyAvailable()
        ? tr("Панель Umbrella работает. Ctrl+Alt+U — аварийный выход.")
        : tr("Панель Umbrella работает. Ctrl+Alt+U занято; используйте кнопку выхода или --restore-taskbar.");
    emit changed();
}

bool ShellController::registerBars() {
    for (auto& bar : bars_) {
        bar->native.reset();
        bar->native = std::make_unique<DesktopBar>(bar->view->winId());
        if (!bar->native->registered() || !bar->native->position(56, bar->view->devicePixelRatio())) {
            restore(); for (auto& item : bars_) item->native.reset();
            fail(tr("Не удалось зарезервировать место панели.")); return false;
        }
    }
    return true;
}
