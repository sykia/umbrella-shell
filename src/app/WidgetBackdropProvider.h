#pragma once
#include <QQuickImageProvider>
#include "platform/windows/WidgetBackdrop.h"

// App owns the QML provider; the Windows adapter has no Qt Quick dependency.
class WidgetBackdropProvider final : public QQuickImageProvider {
public:
    WidgetBackdropProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        return umbrella::platform::renderWidgetBackdrop(id, size);
    }
};
