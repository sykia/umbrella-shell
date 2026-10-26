#include "ClockModel.h"
#include <QDateTime>

ClockModel::ClockModel(QObject* parent) : QObject(parent) {
    timer_.setSingleShot(true);
    timer_.setTimerType(Qt::VeryCoarseTimer);
    connect(&timer_, &QTimer::timeout, this, &ClockModel::refresh);
}

void ClockModel::setActive(bool active) {
    if (active_ == active) return;
    active_ = active;
    if (active_) refresh();
    else timer_.stop();
}

void ClockModel::refresh() {
    if (!active_) return;
    const auto now = QDateTime::currentDateTime();
    const auto next = now.addSecs(60 - now.time().second()).addMSecs(-now.time().msec());
    const auto nextValue = QVariantMap{{"hours", now.toString("HH")},
        {"minutes", now.toString("mm")}, {"inline", now.toString("HH:mm")}};
    if (time_ != nextValue) {
        time_ = nextValue;
        emit changed();
    }
    timer_.start(qMax<qint64>(250, now.msecsTo(next) + 20));
}
