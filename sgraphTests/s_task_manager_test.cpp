#include "s_coordinate_system.h"
#include "s_task_manager.h"

#include <QSemaphore>
#include <QSignalSpy>
#include <QtTest>
#include <atomic>

namespace smartGraphics3D
{
class STaskManagerTest final : public QObject
{
    Q_OBJECT

  private slots:
    void reportsProgressAndCompletion()
    {
        STaskManager manager;
        QSignalSpy progress_spy(&manager, &STaskManager::taskProgress);
        QSignalSpy finished_spy(&manager, &STaskManager::taskFinished);
        manager.run(QStringLiteral("测试任务"),
                    [](const STaskContext& context)
                    {
                        context.reportProgress(50, QStringLiteral("处理中"));
                        return SResult<void>::success();
                    });
        QTRY_COMPARE_WITH_TIMEOUT(finished_spy.count(), 1, 3000);
        QVERIFY(progress_spy.count() >= 1);
        QCOMPARE(manager.runningTaskCount(), 0);
        QCOMPARE(finished_spy.front().at(1).toBool(), true);
    }

    void cancellationPreventsSuccessfulCompletion()
    {
        STaskManager manager;
        QSemaphore gate;
        QSignalSpy finished_spy(&manager, &STaskManager::taskFinished);
        const QUuid id = manager.run(QStringLiteral("可取消任务"),
                                     [&gate](const STaskContext& context)
                                     {
                                         while (!context.isCancellationRequested())
                                         {
                                             gate.tryAcquire(1, 20);
                                         }
                                         return SResult<void>::failure(
                                             SErrorCode::Cancelled, QStringLiteral("任务已取消"));
                                     });
        manager.cancel(id);
        QTRY_COMPARE_WITH_TIMEOUT(finished_spy.count(), 1, 3000);
        QCOMPARE(finished_spy.front().at(1).toBool(), false);
        QCOMPARE(manager.runningTaskCount(), 0);
    }

    void failedTaskCanBeRetried()
    {
        STaskManager manager;
        int attempts = 0;
        QSignalSpy finished_spy(&manager, &STaskManager::taskFinished);
        const QUuid first =
            manager.run(QStringLiteral("重试任务"),
                        [&attempts](const STaskContext&)
                        {
                            ++attempts;
                            return attempts == 1
                                       ? SResult<void>::failure(SErrorCode::InternalFailure,
                                                                QStringLiteral("首次失败"),
                                                                QStringLiteral("可重试诊断"))
                                       : SResult<void>::success();
                        });
        QTRY_COMPARE_WITH_TIMEOUT(finished_spy.count(), 1, 3000);
        QVERIFY(manager.canRetry(first));
        const QUuid second = manager.retry(first);
        QVERIFY(!second.isNull());
        QTRY_COMPARE_WITH_TIMEOUT(finished_spy.count(), 2, 3000);
        QCOMPARE(attempts, 2);
        QVERIFY(!manager.canRetry(first));
    }

    void executesTasksInSubmissionOrder()
    {
        STaskManager manager;
        QSemaphore first_started;
        QSemaphore release_first;
        std::atomic_bool second_started = false;
        QSignalSpy finished_spy(&manager, &STaskManager::taskFinished);

        manager.run(QStringLiteral("第一个任务"),
                    [&first_started, &release_first](const STaskContext&)
                    {
                        first_started.release();
                        release_first.acquire();
                        return SResult<void>::success();
                    });
        manager.run(QStringLiteral("第二个任务"),
                    [&second_started](const STaskContext&)
                    {
                        second_started.store(true);
                        return SResult<void>::success();
                    });

        QVERIFY(first_started.tryAcquire(1, 3000));
        QTest::qWait(100);
        QVERIFY(!second_started.load());
        QCOMPARE(manager.runningTaskCount(), 2);

        release_first.release();
        QTRY_COMPARE_WITH_TIMEOUT(finished_spy.count(), 2, 3000);
        QVERIFY(second_started.load());
        QCOMPARE(manager.runningTaskCount(), 0);
    }

    void resolvesDirectedCoordinateTransforms()
    {
        SCoordinateSystem world;
        world.name = QStringLiteral("世界");
        SCoordinateSystem local;
        local.name = QStringLiteral("工件");
        local.parent_id = world.id;
        local.transform_to_parent.translate(10.0F, 0.0F, 0.0F);
        SCoordinateSystem feature;
        feature.name = QStringLiteral("特征");
        feature.parent_id = local.id;
        feature.transform_to_parent.translate(0.0F, 5.0F, 0.0F);
        const std::vector<SCoordinateSystem> systems = {world, local, feature};

        const auto to_world = SCoordinateSystemService::transform(systems, feature.id, world.id);
        QVERIFY(to_world);
        QCOMPARE(to_world.value().map(QVector3D()), QVector3D(10.0F, 5.0F, 0.0F));
        const auto to_feature = SCoordinateSystemService::transform(systems, world.id, feature.id);
        QVERIFY(to_feature);
        QCOMPARE(to_feature.value().map(QVector3D(10.0F, 5.0F, 0.0F)), QVector3D());
        QCOMPARE(SCoordinateSystemService::directionLabel(systems, feature.id, world.id),
                 QStringLiteral("特征 → 世界"));
    }
};
} // namespace smartGraphics3D

QTEST_GUILESS_MAIN(smartGraphics3D::STaskManagerTest)
#include "s_task_manager_test.moc"
