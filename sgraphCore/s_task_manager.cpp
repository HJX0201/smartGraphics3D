#include "s_task_manager.h"

#include <QtConcurrent>
#include <algorithm>
#include <atomic>

namespace smartGraphics3D
{
struct STaskManager::STaskDefinition
{
    QString name;
    std::function<SResult<void>(const STaskContext&)> work;
    std::function<void(const SResult<void>&)> completion;
};

struct STaskManager::STaskState
{
    STaskInfo info;
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<STaskDefinition> definition;
    QFutureWatcher<SResult<void>> watcher;
    bool started = false;
};

bool STaskContext::isCancellationRequested() const
{
    return m_cancelled && m_cancelled->load();
}

void STaskContext::reportProgress(int progress, const QString& step) const
{
    if (m_progress)
    {
        m_progress(qBound(0, progress, 100), step);
    }
}

STaskManager::STaskManager(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<STaskInfo>();
}

QUuid STaskManager::run(QString name, std::function<SResult<void>()> work)
{
    return run(std::move(name),
               [work = std::move(work)](const STaskContext&)
               {
                   return work();
               },
               {});
}

QUuid STaskManager::run(QString name, std::function<SResult<void>(const STaskContext&)> work,
                        std::function<void(const SResult<void>&)> completion)
{
    auto definition = std::make_shared<STaskDefinition>();
    definition->name = name;
    definition->work = std::move(work);
    definition->completion = std::move(completion);
    auto state = std::make_shared<STaskState>();
    state->info.id = QUuid::createUuid();
    state->info.name = std::move(name);
    state->info.step = tr("等待队列");
    state->info.started_at = QDateTime::currentDateTimeUtc();
    state->definition = definition;
    m_tasks.push_back(state);

    connect(&state->watcher, &QFutureWatcher<SResult<void>>::finished, this,
            [this, state]()
            {
                const SResult<void> result = state->watcher.result();
                if (state->definition->completion)
                {
                    state->definition->completion(result);
                }
                if (!result && result.errorCode() != SErrorCode::Cancelled)
                {
                    m_retryable_tasks.insert(state->info.id, state->definition);
                    emit taskFailed(state->info.id, static_cast<int>(result.errorCode()),
                                    result.message(), result.details());
                }
                emit taskFinished(state->info.id, result.isSuccess(),
                                  result.isSuccess() ? tr("已完成") : result.message());
                m_tasks.removeAll(state);
                emit runningTaskCountChanged(m_tasks.size());
                startNextTask();
            });

    emit taskStarted(state->info);
    emit runningTaskCountChanged(m_tasks.size());
    startNextTask();
    return state->info.id;
}

void STaskManager::startNextTask()
{
    const bool has_active_task = std::any_of(m_tasks.cbegin(), m_tasks.cend(),
                                             [](const std::shared_ptr<STaskState>& task)
                                             {
                                                 return task->started;
                                             });
    if (has_active_task)
    {
        return;
    }

    while (!m_tasks.isEmpty())
    {
        const std::shared_ptr<STaskState> state = m_tasks.front();
        if (state->cancelled->load())
        {
            emit taskFinished(state->info.id, false, tr("任务已取消"));
            m_tasks.pop_front();
            emit runningTaskCountChanged(m_tasks.size());
            continue;
        }

        state->started = true;
        state->info.started_at = QDateTime::currentDateTimeUtc();
        state->info.step = tr("正在执行");
        emit taskProgress(state->info.id, 0, state->info.step);

        const std::shared_ptr<STaskDefinition> definition = state->definition;
        STaskContext context;
        context.m_cancelled = state->cancelled;
        const QPointer<STaskManager> manager(this);
        const QUuid task_id = state->info.id;
        context.m_progress = [manager, task_id](int progress, const QString& step)
        {
            if (!manager)
            {
                return;
            }
            QMetaObject::invokeMethod(
                manager,
                [manager, task_id, progress, step]()
                {
                    if (manager)
                    {
                        emit manager->taskProgress(task_id, progress, step);
                    }
                },
                Qt::QueuedConnection);
        };
        state->watcher.setFuture(QtConcurrent::run(
            [state, definition, context]()
            {
                if (state->cancelled->load())
                {
                    return SResult<void>::failure(SErrorCode::Cancelled, QObject::tr("任务已取消"));
                }
                context.reportProgress(0, QObject::tr("正在执行"));
                SResult<void> result = definition->work(context);
                if (state->cancelled->load())
                {
                    return SResult<void>::failure(SErrorCode::Cancelled,
                                                  QObject::tr("任务已取消，结果未提交"));
                }
                context.reportProgress(100, QObject::tr("已完成"));
                return result;
            }));
        return;
    }
}

void STaskManager::cancel(const QUuid& id)
{
    for (qsizetype index = 0; index < m_tasks.size(); ++index)
    {
        const std::shared_ptr<STaskState> task = m_tasks.at(index);
        if (task->info.id != id)
        {
            continue;
        }
        task->cancelled->store(true);
        if (!task->started)
        {
            emit taskFinished(task->info.id, false, tr("任务已取消"));
            m_tasks.removeAt(index);
            emit runningTaskCountChanged(m_tasks.size());
            startNextTask();
        }
        return;
    }
}

/*
 * Tasks intentionally use one execution slot. CAD operations may contend for
 * OCCT resources and their document results must be committed in submission
 * order, so queued work is never started concurrently.
 */
QUuid STaskManager::retry(const QUuid& id)
{
    const auto iterator = m_retryable_tasks.find(id);
    if (iterator == m_retryable_tasks.end())
    {
        return {};
    }
    const std::shared_ptr<STaskDefinition> definition = iterator.value();
    m_retryable_tasks.erase(iterator);
    return run(definition->name, definition->work, definition->completion);
}

int STaskManager::runningTaskCount() const
{
    return m_tasks.size();
}

bool STaskManager::isTaskRunning(const QUuid& id) const
{
    return std::any_of(m_tasks.cbegin(), m_tasks.cend(),
                       [&id](const std::shared_ptr<STaskState>& task)
                       {
                           return task->info.id == id;
                       });
}

bool STaskManager::canRetry(const QUuid& id) const
{
    return m_retryable_tasks.contains(id);
}
} // namespace smartGraphics3D
