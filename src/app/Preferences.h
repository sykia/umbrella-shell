#pragma once
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QElapsedTimer>
#include <QDate>
#include <QColor>
#include <QUrl>
#include <QVariantList>
class Preferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY changed)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY changed)
    Q_PROPERTY(QUrl wallpaper READ wallpaper NOTIFY changed)
    Q_PROPERTY(QUrl wallpaperFolder READ wallpaperFolder NOTIFY changed)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY changed)
    Q_PROPERTY(QStringList fonts READ fonts NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes NOTIFY changed)
    Q_PROPERTY(bool tracking READ tracking WRITE setTracking NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int activityRevision READ activityRevision NOTIFY activityChanged)
public:
    explicit Preferences(QObject* parent = nullptr);
    ~Preferences() override;
    QVariantMap theme() const;
    QString fontFamily() const;
    void setFontFamily(const QString& value);
    int cornerRadius() const;
    void setCornerRadius(int value);
    QUrl wallpaper() const { return wallpaper_; }
    QUrl wallpaperFolder() const;
    QVariantList wallpapers() const;
    QStringList fonts() const;
    QVariantList themes() const;
    Q_INVOKABLE void selectTheme(const QString& name);
    Q_INVOKABLE void addTheme(const QString& name, const QColor& accent, bool dark);
    Q_INVOKABLE void setWallpaper(const QUrl& path);
    Q_INVOKABLE void setWallpaperFolder(const QUrl& path);
    Q_INVOKABLE void loadFont(const QUrl& path);
    bool tracking() const { return tracking_; }
    void setTracking(bool enabled);
    Q_INVOKABLE QVariantMap activity(int offset) const;
    Q_INVOKABLE void clearActivity();
    QString error() const { return error_; }
    int activityRevision() const { return revision_; }
signals:
    void changed();
    void appearanceChanged();
    void activityChanged();
private:
    void sample();
    void flush();
    void save(const QString& key, const QVariant& value);
    void failure(const QString& error);
    QSettings settings_;
    QTimer timer_;
    QElapsedTimer elapsed_;
    QUrl wallpaper_;
    QVariantMap days_;
    QString lastApp_, error_;
    QDate lastDate_;
    int unsaved_ = 0;
    int revision_ = 0;
    bool tracking_ = false;
};
