#pragma once
#include <QObject>
#include <QTimer>
namespace umbrella::platform {
class PanelActions final : public QObject {
    Q_OBJECT
public:
    explicit PanelActions(QObject* parent = nullptr);
    QString layout() const { return layout_; }
    void setEnabled(bool enabled);
    bool cycleLayout();
    bool openSound();
    bool moveDesktop(int direction);
    bool showDesktops();
signals:
    void layoutChanged();
private:
    void refresh();
    QTimer timer_;
    quintptr target_ = 0;
    QString layout_ = "—";
};
}
