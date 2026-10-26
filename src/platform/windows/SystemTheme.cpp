#include "SystemTheme.h"
#include <QCoreApplication>
#include <QColor>
#include <QTimer>
#include <Windows.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.h>

namespace umbrella::platform {
SystemTheme::SystemTheme(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}
SystemTheme::~SystemTheme() {
    QCoreApplication::instance()->removeNativeEventFilter(this);
}
QVariantMap SystemTheme::theme() const {
    QColor background(Qt::white), foreground(Qt::black), accent(0,120,215);
    bool animations = true, transparency = false;
    try {
        const winrt::Windows::UI::ViewManagement::UISettings settings;
        const auto convert = [](auto c) { return QColor(c.R,c.G,c.B,c.A); };
        using Type = winrt::Windows::UI::ViewManagement::UIColorType;
        background = convert(settings.GetColorValue(Type::Background)); foreground = convert(settings.GetColorValue(Type::Foreground));
        accent = convert(settings.GetColorValue(Type::Accent)); animations = settings.AnimationsEnabled();
        transparency = settings.AdvancedEffectsEnabled();
    } catch (const winrt::hresult_error&) { }
    HIGHCONTRASTW high{sizeof(HIGHCONTRASTW)};
    const bool contrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0) && (high.dwFlags & HCF_HIGHCONTRASTON);
    if (contrast) {
        auto color = [](int index) { auto c = GetSysColor(index); return QColor(GetRValue(c),GetGValue(c),GetBValue(c)); };
        background = color(COLOR_WINDOW); foreground = color(COLOR_WINDOWTEXT); accent = color(COLOR_HIGHLIGHT);
    }
    return {{"backdropRevision",backdropRevision_},{"background",background},{"foreground",foreground},{"accent",accent},{"dark",background.lightness() < 128},
        {"contrast",contrast},{"animations",animations && !contrast},{"transparency",transparency && !contrast}};
}
bool SystemTheme::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    const auto msg = static_cast<MSG*>(message);
    if ((msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED || msg->message == WM_DWMCOLORIZATIONCOLORCHANGED) && !changeQueued_) {
        changeQueued_ = true;
        QTimer::singleShot(0, this, [this] { changeQueued_ = false; ++backdropRevision_; emit themeChanged(); });
    }
    return false;
}
}
