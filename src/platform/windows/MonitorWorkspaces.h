#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>
#include <string>
namespace umbrella::platform {
class WorkspaceSnapshot {
public:
    WorkspaceSnapshot();
    ~WorkspaceSnapshot();
    bool open(const std::wstring& name, bool create);
    bool restore();
    struct Impl;
    std::unique_ptr<Impl> impl;
};
class MonitorWorkspaces final : public QObject {
    Q_OBJECT
public:
    explicit MonitorWorkspaces(QObject* parent = nullptr, quint32 scopeProcess = 0);
    ~MonitorWorkspaces() override;
    bool start(const QString& snapshot);
    bool stop();
    bool running() const;
    bool setCount(int count);
    QVariantList model(quintptr panel) const;
    bool select(quintptr panel, int index);
    void refresh();
signals:
    void changed();
    void failed(const QString& message);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
