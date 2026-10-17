#include "Preferences.h"
#include "platform/windows/DesktopIntegration.h"
#include <QFontDatabase>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QJsonDocument>
#include <algorithm>
namespace {
QVariantMap palette(const QString& name, const QString& accent, bool dark) {
    return {{"name", name}, {"accent", QColor(accent)}, {"dark", dark},
        {"background", dark ? "#191b22" : "#f7f2f3"}, {"surface", dark ? "#252833" : "#ece4e6"},
        {"raised", dark ? "#343846" : "#e4d5d8"}, {"ink", dark ? "#f0e9ed" : "#4c4145"},
        {"muted", dark ? "#a9a5b2" : "#93828a"}};
}
}
Preferences::Preferences(QObject* parent) : QObject(parent), settings_(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Preferences") {
    const auto fontFile = settings_.value("theme/fontFile").toString();
    if (!fontFile.isEmpty()) QFontDatabase::addApplicationFont(fontFile);
    wallpaper_ = QUrl::fromLocalFile(QString::fromStdWString(umbrella::platform::desktopWallpaper()));
    days_ = QJsonDocument::fromJson(settings_.value("activity/data").toByteArray()).toVariant().toMap();
    const auto cutoff = QDate::currentDate().addDays(-89);
    for (auto it = days_.begin(); it != days_.end();) {
        const auto date = QDate::fromString(it.key(), Qt::ISODate);
        if (!date.isValid() || date < cutoff || date > QDate::currentDate()) it = days_.erase(it); else ++it;
    }
    timer_.setInterval(5000); timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, &Preferences::sample);
    tracking_ = settings_.value("activity/enabled", false).toBool();
    if (tracking_) { elapsed_.start(); timer_.start(); }
}
Preferences::~Preferences() { flush(); }
void Preferences::failure(const QString& error) { error_ = error; emit changed(); }
void Preferences::save(const QString& key, const QVariant& value) {
    settings_.setValue(key, value); settings_.sync();
    if (settings_.status() != QSettings::NoError) failure(tr("Не удалось сохранить настройки оформления/активности."));
    if (key.startsWith("theme/") || key.startsWith("themes/")) emit appearanceChanged();
    emit changed();
}
QVariantList Preferences::themes() const {
    QVariantList result{palette("Umbrella", "#d9263b", false), palette("Obsidian", "#eb4d61", true),
        palette("Ocean", "#2b9bbd", true), palette("Forest", "#408a67", false), palette("Graphite", "#858da8", true)};
    const auto custom = settings_.value("themes/custom").toList();
    for (const auto& item : custom) result << item;
    return result;
}
QVariantMap Preferences::theme() const {
    const auto name = settings_.value("theme/name", "Umbrella").toString();
    for (const auto& item : themes()) if (item.toMap().value("name").toString() == name) return item.toMap();
    return palette("Umbrella", "#d9263b", false);
}
void Preferences::selectTheme(const QString& name) {
    for (const auto& item : themes()) if (item.toMap().value("name").toString() == name) { save("theme/name", name); return; }
}
void Preferences::addTheme(const QString& name, const QColor& accent, bool dark) {
    const auto title = name.trimmed().left(40);
    if (title.isEmpty() || !accent.isValid()) { failure(tr("Введите название и выберите цвет.")); return; }
    for (const auto& item : themes()) if (item.toMap().value("name").toString() == title) { failure(tr("Тема с таким именем уже существует.")); return; }
    auto custom = settings_.value("themes/custom").toList();
    if (custom.size() >= 20) { failure(tr("Можно сохранить до 20 собственных тем.")); return; }
    custom << palette(title, accent.name(), dark); save("themes/custom", custom); selectTheme(title);
}
QString Preferences::fontFamily() const { return settings_.value("theme/font", "Segoe UI").toString(); }
QStringList Preferences::fonts() const { return QFontDatabase::families(); }
void Preferences::setFontFamily(const QString& value) { if (fonts().contains(value)) save("theme/font", value); }
int Preferences::cornerRadius() const { return std::clamp(settings_.value("theme/radius", 16).toInt(), 0, 28); }
void Preferences::setCornerRadius(int value) { if (value >= 0 && value <= 28) save("theme/radius", value); }
void Preferences::loadFont(const QUrl& url) {
    const int id = QFontDatabase::addApplicationFont(url.toLocalFile());
    if (id < 0) { failure(tr("Не удалось загрузить шрифт.")); return; }
    const auto names = QFontDatabase::applicationFontFamilies(id);
    if (!names.isEmpty()) { save("theme/fontFile", url.toLocalFile()); setFontFamily(names.first()); }
}
QUrl Preferences::wallpaperFolder() const { return QUrl::fromLocalFile(settings_.value("theme/wallpaperFolder").toString()); }
void Preferences::setWallpaperFolder(const QUrl& url) {
    if (!url.isLocalFile() || !QDir(url.toLocalFile()).exists()) { failure(tr("Папка не найдена.")); return; }
    save("theme/wallpaperFolder", url.toLocalFile());
}
QVariantList Preferences::wallpapers() const {
    QVariantList result; const QDir dir(wallpaperFolder().toLocalFile());
    if (wallpaperFolder().isEmpty()) return result;
    for (const auto& entry : dir.entryInfoList({"*.png", "*.jpg", "*.jpeg", "*.bmp"}, QDir::Files, QDir::Name)) {
        if (result.size() >= 200) break;
        result << QVariantMap{{"name", entry.fileName()}, {"url", QUrl::fromLocalFile(entry.absoluteFilePath())}};
    }
    return result;
}
void Preferences::setWallpaper(const QUrl& url) {
    const QFileInfo file(url.toLocalFile());
    if (!url.isLocalFile() || !file.isFile() || !QImageReader(file.absoluteFilePath()).canRead()) { failure(tr("Выберите локальное изображение PNG, JPEG или BMP.")); return; }
    if (!umbrella::platform::applyDesktopWallpaper(file.absoluteFilePath().toStdWString())) { failure(tr("Windows отклонила изменение обоев.")); return; }
    wallpaper_ = QUrl::fromLocalFile(file.absoluteFilePath()); emit changed();
}
void Preferences::setTracking(bool enabled) {
    if (enabled == tracking_) return;
    tracking_ = enabled; lastApp_.clear(); elapsed_.restart();
    if (enabled) timer_.start(); else { timer_.stop(); flush(); }
    save("activity/enabled", enabled);
}
void Preferences::sample() {
    const auto now = QDate::currentDate();
    const auto app = umbrella::platform::userRecentlyActive() ? QString::fromStdWString(umbrella::platform::activeApplication()) : QString();
    const qint64 duration = elapsed_.restart();
    if (!app.isEmpty() && app == lastApp_ && lastDate_ == now && duration > 0 && duration <= 10000) {
        auto day = days_.value(now.toString(Qt::ISODate)).toMap();
        if (day.contains(app) || day.size() < 100) day[app] = day.value(app).toLongLong() + duration / 1000;
        days_[now.toString(Qt::ISODate)] = day;
        while (days_.size() > 90) days_.erase(days_.begin());
        unsaved_ += static_cast<int>(duration / 1000);
        if (unsaved_ >= 60) flush();
        ++revision_; emit activityChanged();
    }
    lastApp_ = app; lastDate_ = now;
}
void Preferences::flush() {
    if (!unsaved_) return;
    settings_.setValue("activity/data", QJsonDocument::fromVariant(days_).toJson(QJsonDocument::Compact)); settings_.sync();
    if (settings_.status() == QSettings::NoError) unsaved_ = 0;
    else failure(tr("Не удалось записать локальную статистику."));
}
void Preferences::clearActivity() { days_.clear(); unsaved_ = 0; lastApp_.clear(); settings_.remove("activity/data"); settings_.sync(); ++revision_; emit activityChanged(); }
QVariantMap Preferences::activity(int offset) const {
    offset = std::clamp(offset, -89, 0);
    const auto date = QDate::currentDate().addDays(offset);
    auto total = [this](const QDate& d) { qint64 value = 0; for (const auto& item : days_.value(d.toString(Qt::ISODate)).toMap()) value += item.toLongLong(); return value; };
    const auto today = total(date);
    QVariantList apps, week, calendar;
    const auto day = days_.value(date.toString(Qt::ISODate)).toMap();
    for (auto it = day.begin(); it != day.end(); ++it) apps << QVariantMap{{"name", it.key()}, {"seconds", it.value()}};
    std::sort(apps.begin(), apps.end(), [](const auto& a, const auto& b) { return a.toMap().value("seconds").toLongLong() > b.toMap().value("seconds").toLongLong(); });
    qint64 sum = 0; const auto monday = date.addDays(1 - date.dayOfWeek());
    for (int i = 0; i < 7; ++i) { const auto d = monday.addDays(i); const auto value = total(d); sum += value; week << QVariantMap{{"date", d.toString("dd.MM")}, {"seconds", value}, {"selected", d == date}}; }
    const QDate first(date.year(), date.month(), 1);
    for (int i = 1; i < first.dayOfWeek(); ++i) calendar << QVariantMap{{"day", 0}, {"seconds", 0}, {"offset", 1}};
    for (int i = 1; i <= date.daysInMonth(); ++i) { const QDate d(date.year(), date.month(), i); calendar << QVariantMap{{"day", i}, {"seconds", total(d)}, {"offset", QDate::currentDate().daysTo(d)}, {"selected", d == date}}; }
    return {{"date", date.toString("dd.MM.yyyy")}, {"month", date.toString("MM.yyyy")}, {"total", today}, {"average", sum / 7},
        {"difference", today - total(date.addDays(-1))}, {"apps", apps}, {"week", week}, {"calendar", calendar}};
}
