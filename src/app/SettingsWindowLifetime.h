#pragma once
#include <QCloseEvent>
#include <QGuiApplication>
#include <QObject>
#include <QWindow>

// The app keeps ownership of its taskbar session when only its editor is closed.
class SettingsWindowLifetime final : public QObject {
    QWindow& window_;
    bool hideOnClose_;
protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (hideOnClose_ && object == &window_ && event->type() == QEvent::Close) {
            static_cast<QCloseEvent*>(event)->ignore();
            window_.hide();
            return true;
        }
        return QObject::eventFilter(object, event);
    }
public:
    SettingsWindowLifetime(QGuiApplication& app, QWindow& window, bool hideOnClose)
        : window_(window), hideOnClose_(hideOnClose) {
        if (hideOnClose_) app.setQuitOnLastWindowClosed(false);
        window_.installEventFilter(this);
    }
    ~SettingsWindowLifetime() override { window_.removeEventFilter(this); }
};
