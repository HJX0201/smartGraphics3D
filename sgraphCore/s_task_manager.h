#pragma once

#include "s_result.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUuid>
#include <atomic>
#include <functional>
#include <memory>

namespace smartGraphics3D
{
class STaskContext
{
  public:
    bool isCancellationRequested() const;
    void reportProgress(int progress, const QString& step = {}) const;

  private:
    std::shared_ptr<std::atomic_bool> m_cancelled;
    std::function<void(int, const QString&)> m_progress;

    friend class STaskManager;
};

struct STaskInfo
{
    QUuid id;
    QString name;
    QString step;
    int progress = 0;
    bool cancellable = true;
    QDateTime started_at;
};

class STaskManager final : public QObject
{
    Q_OBJECT

  public:
    explicit STaskManager(QObject* parent = nullptr);

    QUuid run(QString name, std::function<SResult<void>()> work);
    QUuid run(QString name, std::function<SResult<void>(const STaskContext&)> work,
              std::function<void(const SResult<void>&)> completion = {});
    void cancel(const QUuid& id);
    QUuid retry(const QUuid& id);
    int runningTaskCount() const;
    bool isTaskRunning(const QUuid& id) const;
    bool canRetry(const QUuid& id) const;

  signals:
    void taskStarted(const smartGraphics3D::STaskInfo& info);
    void taskProgress(const QUuid& id, int progress, const QString& step);
    void taskFinished(const QUuid& id, bool success, const QString& message);
    void taskFailed(const QUuid& id, int error_code, const QString& message,
                    const QString& details);
    void runningTaskCountChanged(int count);

  private:
    struct STaskState;
    struct STaskDefinition;
    void startNextTask();

    QList<std::shared_ptr<STaskState>> m_tasks;
    QHash<QUuid, std::shared_ptr<STaskDefinition>> m_retryable_tasks;
};
} // namespace smartGraphics3D

Q_DECLARE_METATYPE(smartGraphics3D::STaskInfo)
