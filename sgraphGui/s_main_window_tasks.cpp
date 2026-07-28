#include "s_main_window.h"

#include <QCheckBox>
#include <QDateTime>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace smartGraphics3D
{
void SMainWindow::connectTaskSignals()
{
    connect(&m_task_manager, &STaskManager::runningTaskCountChanged, this,
            [this](int count)
            {
                m_task_label->setText(count == 0 ? tr("无后台任务")
                                                 : tr("%1 个后台任务").arg(count));
            });
    connect(&m_task_manager, &STaskManager::taskStarted, this,
            [this](const STaskInfo& info)
            {
                const int row = m_task_table->rowCount();
                m_task_table->insertRow(row);
                auto* name_item = new QTableWidgetItem(info.name);
                name_item->setData(Qt::UserRole, info.id.toString(QUuid::WithoutBraces));
                name_item->setData(Qt::UserRole + 1, info.started_at);
                m_task_table->setItem(row, 0, name_item);
                m_task_table->setItem(row, 1, new QTableWidgetItem(info.step));
                m_task_table->setItem(row, 2, new QTableWidgetItem(tr("0%")));
                m_task_table->setItem(row, 3, new QTableWidgetItem(tr("0.0 秒")));
                m_task_table->setItem(row, 4,
                                      new QTableWidgetItem(info.step == tr("等待队列")
                                                               ? tr("等待（双击可取消）")
                                                               : tr("运行（双击可取消）")));
                auto* cancel_button = new QPushButton(tr("取消"), m_task_table);
                m_task_table->setCellWidget(row, 5, cancel_button);
                connect(cancel_button, &QPushButton::clicked, this,
                        [this, id = info.id, cancel_button]()
                        {
                            m_task_manager.cancel(id);
                            cancel_button->setEnabled(false);
                            cancel_button->setText(tr("正在取消"));
                        });
                m_bottom_tabs->setCurrentIndex(1);
            });
    connect(&m_task_manager, &STaskManager::taskProgress, this,
            [this](const QUuid& id, int progress, const QString& step)
            {
                for (int row = 0; row < m_task_table->rowCount(); ++row)
                {
                    QTableWidgetItem* item = m_task_table->item(row, 0);
                    if (!item || QUuid(item->data(Qt::UserRole).toString()) != id)
                    {
                        continue;
                    }
                    m_task_table->item(row, 1)->setText(step);
                    if (step == tr("正在执行"))
                    {
                        m_task_table->item(row, 4)->setText(tr("运行（双击可取消）"));
                    }
                    m_task_table->item(row, 2)->setText(tr("%1%").arg(progress));
                    const QDateTime started = item->data(Qt::UserRole + 1).toDateTime();
                    m_task_table->item(row, 3)->setText(tr("%1 秒").arg(
                        started.msecsTo(QDateTime::currentDateTimeUtc()) / 1000.0, 0, 'f', 1));
                    break;
                }
            });
    connect(&m_task_manager, &STaskManager::taskFinished, this,
            [this](const QUuid& id, bool success, const QString& message)
            {
                for (int row = 0; row < m_task_table->rowCount(); ++row)
                {
                    QTableWidgetItem* item = m_task_table->item(row, 0);
                    if (item && QUuid(item->data(Qt::UserRole).toString()) == id)
                    {
                        m_task_table->item(row, 4)->setText(success ? tr("成功") : message);
                        if (success)
                        {
                            auto* done = new QLabel(tr("完成"), m_task_table);
                            done->setAlignment(Qt::AlignCenter);
                            m_task_table->setCellWidget(row, 5, done);
                        }
                        else if (m_task_manager.canRetry(id))
                        {
                            auto* retry = new QPushButton(tr("重试"), m_task_table);
                            m_task_table->setCellWidget(row, 5, retry);
                            connect(retry, &QPushButton::clicked, this,
                                    [this, id, row, retry]()
                                    {
                                        const QUuid new_id = m_task_manager.retry(id);
                                        if (!new_id.isNull())
                                        {
                                            retry->setEnabled(false);
                                            retry->setText(tr("已重试"));
                                            m_task_table->item(row, 4)->setText(
                                                tr("已创建重试任务"));
                                        }
                                    });
                        }
                        break;
                    }
                }
            });
    connect(&m_task_manager, &STaskManager::taskFailed, this,
            [this](const QUuid& id, int error_code, const QString& message, const QString& details)
            {
                for (int row = 0; row < m_task_table->rowCount(); ++row)
                {
                    QTableWidgetItem* item = m_task_table->item(row, 0);
                    if (!item || QUuid(item->data(Qt::UserRole).toString()) != id)
                    {
                        continue;
                    }
                    const QString detail_text =
                        details.isEmpty() ? tr("没有更多诊断信息") : details;
                    m_task_table->item(row, 4)->setToolTip(
                        tr("错误码：%1\n%2\n%3").arg(error_code).arg(message, detail_text));
                    appendLog(QStringLiteral("ERROR"),
                              tr("后台任务失败（错误码 %1）：%2").arg(error_code).arg(message),
                              detail_text, static_cast<SErrorCode>(error_code));
                    break;
                }
            });
    connect(m_task_table, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int)
            {
                QTableWidgetItem* item = m_task_table->item(row, 0);
                if (!item)
                {
                    return;
                }
                const QUuid id(item->data(Qt::UserRole).toString());
                if (!id.isNull())
                {
                    m_task_manager.cancel(id);
                    m_task_table->item(row, 4)->setText(tr("正在取消"));
                }
            });
}

void SMainWindow::runShapeTask(QString task_name, QList<SObjectId> inputs, QString result_name,
                               QString parameter_summary,
                               std::function<SResult<SKernelShape>(const STaskContext&)> work,
                               bool preserve_appearance)
{
    auto result = std::make_shared<SResult<SKernelShape>>();
    m_task_manager.run(
        task_name,
        [result, work = std::move(work)](const STaskContext& context)
        {
            context.reportProgress(10, QObject::tr("准备几何"));
            if (context.isCancellationRequested())
            {
                return SResult<void>::failure(SErrorCode::Cancelled, QObject::tr("任务已取消"));
            }
            *result = work(context);
            if (!*result)
            {
                return SResult<void>::failure(result->errorCode(), result->message(),
                                              result->details());
            }
            context.reportProgress(90, QObject::tr("等待提交"));
            return SResult<void>::success();
        },
        [this, result, task_name, result_name, parameter_summary, inputs,
         preserve_appearance](const SResult<void>& completion)
        {
            if (!completion)
            {
                appendLog(QStringLiteral("ERROR"),
                          tr("%1 未提交：%2").arg(task_name, completion.message()),
                          completion.details());
                return;
            }
            bool replace_inputs = false;
            if (!confirmShapePreview(result->value(), task_name, inputs, replace_inputs))
            {
                appendLog(QStringLiteral("INFO"),
                          tr("%1 的临时结果已取消，项目未修改").arg(task_name));
                return;
            }
            SSceneObject object;
            object.name = result_name;
            object.shape = result->value();
            object.source = task_name;
            const SSceneObject* source =
                inputs.isEmpty() ? nullptr : m_document.findObject(inputs.front());
            if (source)
            {
                object.display = source->display;
                if (preserve_appearance && source->imported_appearance.valid)
                {
                    const auto source_metrics = m_kernel->measure(source->shape);
                    const auto result_metrics = m_kernel->measure(object.shape);
                    if (source_metrics && result_metrics &&
                        source_metrics.value().face_count == result_metrics.value().face_count)
                    {
                        object.imported_appearance = source->imported_appearance;
                        object.use_imported_appearance = source->use_imported_appearance;
                    }
                    else
                    {
                        object.display.color = source->imported_appearance.fallback_style.color;
                        object.display.transparency =
                            source->imported_appearance.fallback_style.transparency;
                        appendLog(QStringLiteral("WARNING"),
                                  tr("%1 未能安全映射面颜色，已退回主颜色").arg(task_name));
                    }
                }
                else if (!preserve_appearance && source->imported_appearance.valid)
                {
                    if (source->use_imported_appearance)
                    {
                        object.display.color = source->imported_appearance.fallback_style.color;
                        object.display.transparency =
                            source->imported_appearance.fallback_style.transparency;
                    }
                    appendLog(QStringLiteral("INFO"),
                              tr("%1 改变了拓扑，面颜色已退回主颜色").arg(task_name));
                }
            }
            const auto committed = m_document.addDerivedObject(inputs, std::move(object), task_name,
                                                               replace_inputs, parameter_summary);
            if (!committed)
            {
                showFailure(task_name,
                            SResult<void>::failure(committed.errorCode(), committed.message(),
                                                   committed.details()));
                return;
            }
            m_viewport->selectObject(committed.value());
            appendLog(QStringLiteral("INFO"), tr("%1 已完成并提交").arg(task_name));
        });
}

void SMainWindow::runMultiShapeTask(
    QString task_name, SObjectId input, QString result_prefix, QString parameter_summary,
    std::function<SResult<QList<SKernelShape>>(const STaskContext&)> work, SCopyMode copy_mode,
    QList<QMatrix4x4> instance_transforms)
{
    auto shapes = std::make_shared<SResult<QList<SKernelShape>>>();
    auto preview = std::make_shared<SResult<SKernelShape>>();
    m_task_manager.run(
        task_name,
        [shapes, preview, work = std::move(work)](const STaskContext& context)
        {
            context.reportProgress(10, QObject::tr("生成阵列实例"));
            *shapes = work(context);
            if (!*shapes)
            {
                return SResult<void>::failure(shapes->errorCode(), shapes->message(),
                                              shapes->details());
            }
            if (context.isCancellationRequested())
            {
                return SResult<void>::failure(SErrorCode::Cancelled, QObject::tr("阵列任务已取消"));
            }
            context.reportProgress(85, QObject::tr("构建临时预览"));
            const auto kernel = createKernelService();
            *preview = kernel->makeCompound(shapes->value());
            if (!*preview)
            {
                return SResult<void>::failure(preview->errorCode(), preview->message(),
                                              preview->details());
            }
            return SResult<void>::success();
        },
        [this, shapes, preview, task_name, input, result_prefix, parameter_summary, copy_mode,
         instance_transforms](const SResult<void>& completion)
        {
            if (!completion)
            {
                return;
            }
            bool replace_inputs = false;
            if (!confirmShapePreview(preview->value(), task_name, {input}, replace_inputs))
            {
                appendLog(QStringLiteral("INFO"),
                          tr("%1 的临时结果已取消，项目未修改").arg(task_name));
                return;
            }
            QList<SSceneObject> results;
            int index = 2;
            bool appearance_mapping_failed = false;
            const SSceneObject* source = m_document.findObject(input);
            if (copy_mode == SCopyMode::SharedPresentation &&
                (!source || instance_transforms.size() != shapes->value().size()))
            {
                showFailure(tr("%1提交失败").arg(task_name),
                            SResult<void>::failure(SErrorCode::Conflict,
                                                   tr("实例阵列源对象或变换数据已失效")));
                return;
            }
            for (int result_index = 0; result_index < shapes->value().size(); ++result_index)
            {
                SSceneObject object =
                    copy_mode == SCopyMode::SharedPresentation ? *source : SSceneObject();
                object.id = SObjectId::createUuid();
                object.name = tr("%1 %2").arg(result_prefix).arg(index++);
                object.shape = copy_mode == SCopyMode::SharedPresentation
                                   ? source->shape
                                   : shapes->value().at(result_index);
                object.source = task_name;
                if (source)
                {
                    object.display = source->display;
                    object.imported_appearance = source->imported_appearance;
                    object.use_imported_appearance = source->use_imported_appearance;
                    if (copy_mode == SCopyMode::IndependentPresentation &&
                        source->imported_appearance.valid)
                    {
                        const auto source_metrics = m_kernel->measure(source->shape);
                        const auto result_metrics = m_kernel->measure(object.shape);
                        if (!source_metrics || !result_metrics ||
                            source_metrics.value().face_count != result_metrics.value().face_count)
                        {
                            object.imported_appearance = {};
                            object.use_imported_appearance = false;
                            if (source->use_imported_appearance)
                            {
                                object.display.color =
                                    source->imported_appearance.fallback_style.color;
                                object.display.transparency =
                                    source->imported_appearance.fallback_style.transparency;
                            }
                            appearance_mapping_failed = true;
                        }
                    }
                }
                object.locked = false;
                object.external_reference = false;
                object.external_path.clear();
                if (copy_mode == SCopyMode::SharedPresentation)
                {
                    object.transform = instance_transforms.at(result_index) * source->transform;
                }
                results.push_back(std::move(object));
            }
            if (appearance_mapping_failed)
            {
                appendLog(QStringLiteral("WARNING"),
                          tr("%1 的部分结果未能安全映射面颜色，已退回主颜色").arg(task_name));
            }
            const auto committed = m_document.addDerivedObjects(
                {input}, std::move(results), task_name, replace_inputs, parameter_summary);
            if (!committed)
            {
                showFailure(tr("%1提交失败").arg(task_name),
                            SResult<void>::failure(committed.errorCode(), committed.message(),
                                                   committed.details()));
            }
        });
}

bool SMainWindow::confirmShapePreview(const SKernelShape& shape, const QString& operation,
                                      const QList<SObjectId>& inputs, bool& replace_inputs)
{
    for (SOccViewport* viewport : m_viewports)
    {
        viewport->showPreview(shape);
    }

    QMessageBox box(QMessageBox::Question, tr("%1预览").arg(operation),
                    tr("蓝色半透明几何是临时结果。\n确认应用后才会写入项目；取消不会改变文档。"),
                    QMessageBox::Apply | QMessageBox::Cancel, this);
    box.setDefaultButton(QMessageBox::Apply);
    box.button(QMessageBox::Apply)->setText(tr("应用"));
    box.button(QMessageBox::Cancel)->setText(tr("取消"));
    auto* replace = new QCheckBox(tr("替换输入工作对象"), &box);
    bool can_replace = !inputs.isEmpty();
    for (const SObjectId& id : inputs)
    {
        const SSceneObject* object = m_document.findObject(id);
        can_replace = can_replace && object && !object->locked;
    }
    replace->setEnabled(can_replace);
    replace->setToolTip(can_replace ? tr("启用后移除输入工作对象；默认保留并隐藏输入")
                                    : tr("原始导入或锁定对象受保护，不能替换"));
    box.setCheckBox(replace);
    const bool accepted = box.exec() == QMessageBox::Apply;
    replace_inputs = accepted && replace->isChecked();
    for (SOccViewport* viewport : m_viewports)
    {
        viewport->clearPreview();
    }
    return accepted;
}
} // namespace smartGraphics3D
