#pragma once
#include <QObject>
#include <QVariantMap>
#include <QAbstractNativeEventFilter>

namespace umbrella::platform {
class SystemTheme final : public QObject, public QAbstractNativeEventFilter {
    int backdropRevision_ = 0;
    bool changeQueued_ = false;
    Q_OBJECT
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
public:
    explicit SystemTheme(QObject* parent = nullptr);
    ~SystemTheme() override;
    QVariantMap theme() const;
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
signals:
    void themeChanged();
};
}
