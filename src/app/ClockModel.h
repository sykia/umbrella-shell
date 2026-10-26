#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class ClockModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap time READ time NOTIFY changed)
public:
    explicit ClockModel(QObject* parent = nullptr);
    QVariantMap time() const { return time_; }
    void setActive(bool active);
signals:
    void changed();
private:
    void refresh();
    QVariantMap time_;
    QTimer timer_;
    bool active_ = false;
};
