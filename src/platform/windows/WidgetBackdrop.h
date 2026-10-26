#pragma once
#include <QImage>
#include <algorithm>
#include <QImageReader>
#include <QPainter>
#include <QUrl>
#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace umbrella::platform {
// One cached, bounded monitor image per URL/revision. Never captures our own
// windows or starts a frame timer; Windows settings events invalidate the URL.
inline QImage renderWidgetBackdrop(const QString& id, QSize* size) {
        const auto parts = id.section('/', 0, 0).split(',');
        const int left = parts.value(0).toInt(), top = parts.value(1).toInt();
        const int width = std::clamp(parts.value(2).toInt(), 100, 16384);
        const int height = std::clamp(parts.value(3).toInt(), 100, 16384);
        const auto target = QSize(width, height).scaled(768, 768, Qt::KeepAspectRatio);
        QImage image(target, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QString path;
        DESKTOP_WALLPAPER_POSITION position = DWPOS_FILL;
        RECT monitor{left, top, left + width, top + height};
        const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        {
            Microsoft::WRL::ComPtr<IDesktopWallpaper> desktop;
            if (SUCCEEDED(CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&desktop)))) {
                COLORREF color{};
                if (SUCCEEDED(desktop->GetBackgroundColor(&color))) image.fill(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
                desktop->GetPosition(&position);
                UINT count{};
                desktop->GetMonitorDevicePathCount(&count);
                for (UINT index = 0; index < count && index < 32; ++index) {
                    LPWSTR device{};
                    if (FAILED(desktop->GetMonitorDevicePathAt(index, &device))) continue;
                    RECT rect{};
                    const bool match = SUCCEEDED(desktop->GetMonitorRECT(device, &rect)) && rect.left == left && rect.top == top;
                    if (match) {
                        LPWSTR file{};
                        if (SUCCEEDED(desktop->GetWallpaper(device, &file))) { path = QString::fromWCharArray(file); CoTaskMemFree(file); }
                        monitor = rect;
                    }
                    CoTaskMemFree(device);
                    if (match) break;
                }
            }
        }
        if (SUCCEEDED(apartment)) CoUninitialize();
        if (path.isEmpty()) {
            wchar_t wallpaper[32768]{};
            if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, 32768, wallpaper, 0)) path = QString::fromWCharArray(wallpaper);
        }
        QImageReader reader(path);
        const auto original = reader.size();
        if (original.isValid() && qint64(original.width()) * original.height() <= 100000000) {
            reader.setAutoTransform(true);
            reader.setScaledSize(original.scaled(1536, 1536, Qt::KeepAspectRatio));
            const auto paper = reader.read();
            if (!paper.isNull()) {
                QPainter painter(&image);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                const QSizeF physical(monitor.right - monitor.left, monitor.bottom - monitor.top);
                const qreal sx = target.width() / physical.width(), sy = target.height() / physical.height();
                QRectF destination(QPointF(0,0), target);
                if (position == DWPOS_SPAN) {
                    const QRectF all(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
                    const auto fitted = QSizeF(original).scaled(all.size(), Qt::KeepAspectRatioByExpanding);
                    destination = QRectF((all.x() + (all.width()-fitted.width())/2 - monitor.left)*sx,
                        (all.y() + (all.height()-fitted.height())/2 - monitor.top)*sy, fitted.width()*sx, fitted.height()*sy);
                } else if (position != DWPOS_STRETCH && position != DWPOS_TILE) {
                    const auto fitted = position == DWPOS_CENTER ? QSizeF(original) : QSizeF(original).scaled(physical, position == DWPOS_FIT ? Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding);
                    destination = QRectF((physical.width()-fitted.width())/2*sx, (physical.height()-fitted.height())/2*sy, fitted.width()*sx, fitted.height()*sy);
                }
                if (position == DWPOS_TILE) {
                    const auto tile = QSizeF(original.width()*sx, original.height()*sy);
                    // Bound pathological tiny tiles; Qt's texture brush repeats in one draw.
                    const auto tileImage = paper.scaled(std::clamp(qRound(tile.width()),1,1536), std::clamp(qRound(tile.height()),1,1536), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    painter.fillRect(image.rect(), QBrush(tileImage));
                } else painter.drawImage(destination, paper);
            }
        }
        if (size) *size = image.size();
        return image;
}
}
