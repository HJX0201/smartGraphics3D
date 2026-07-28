#include "s_main_window.h"

#include <QInputDialog>
#include <QMessageBox>

namespace smartGraphics3D
{
namespace
{
SCopyMode requestArrayCopyMode(QWidget* parent, const QString& title, bool& accepted)
{
    const QStringList modes = {QObject::tr("普通副本（独立显示）"), QObject::tr("共享显示实例")};
    const QString selected =
        QInputDialog::getItem(parent, title, QObject::tr("生成方式"), modes, 0, false, &accepted);
    return selected == modes.at(1) ? SCopyMode::SharedPresentation
                                   : SCopyMode::IndependentPresentation;
}
} // namespace

void SMainWindow::runLinearArray()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("线性阵列"), tr("请选择一个实体。"));
        return;
    }
    bool accepted = false;
    const SCopyMode copy_mode = requestArrayCopyMode(this, tr("线性阵列"), accepted);
    if (!accepted)
    {
        return;
    }
    const int count =
        QInputDialog::getInt(this, tr("线性阵列"), tr("数量"), 3, 2, 1000, 1, &accepted);
    if (!accepted)
    {
        return;
    }
    const double spacing = QInputDialog::getDouble(this, tr("线性阵列"), tr("X 方向间距 (mm)"),
                                                   100.0, -1.0e9, 1.0e9, 3, &accepted);
    if (!accepted)
    {
        return;
    }

    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("线性阵列失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape source = materialized.value();
    QList<QMatrix4x4> instance_transforms;
    for (int index = 1; index < count; ++index)
    {
        QMatrix4x4 transform;
        transform.translate(static_cast<float>(spacing * index), 0.0F, 0.0F);
        instance_transforms.push_back(transform);
    }
    runMultiShapeTask(
        tr("线性阵列"), object->id, object->name + tr(" 阵列"),
        tr("数量=%1; X间距=%2 mm").arg(count).arg(spacing, 0, 'g', 12),
        [source, count, spacing](const STaskContext& context)
        {
            const auto kernel = createKernelService();
            QList<SKernelShape> shapes;
            for (int index = 1; index < count; ++index)
            {
                if (context.isCancellationRequested())
                {
                    return SResult<QList<SKernelShape>>::failure(SErrorCode::Cancelled,
                                                                 QObject::tr("线性阵列已取消"));
                }
                STransformParameters parameters;
                parameters.translation = QVector3D(spacing * index, 0.0, 0.0);
                const auto result = kernel->transform(source, parameters);
                if (!result)
                {
                    return SResult<QList<SKernelShape>>::failure(
                        result.errorCode(), result.message(), result.details());
                }
                shapes.push_back(result.value());
                context.reportProgress(10 + 70 * index / count,
                                       QObject::tr("生成实例 %1/%2").arg(index).arg(count - 1));
            }
            return SResult<QList<SKernelShape>>::success(std::move(shapes));
        },
        copy_mode, instance_transforms);
}

void SMainWindow::runPolarArray()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("圆周阵列"), tr("请选择一个实体。"));
        return;
    }
    bool accepted = false;
    const SCopyMode copy_mode = requestArrayCopyMode(this, tr("圆周阵列"), accepted);
    if (!accepted)
    {
        return;
    }
    const int count =
        QInputDialog::getInt(this, tr("圆周阵列"), tr("数量"), 6, 2, 1000, 1, &accepted);
    if (!accepted)
    {
        return;
    }

    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("圆周阵列失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape source = materialized.value();
    QList<QMatrix4x4> instance_transforms;
    for (int index = 1; index < count; ++index)
    {
        QMatrix4x4 transform;
        transform.rotate(
            static_cast<float>(360.0 * static_cast<double>(index) / static_cast<double>(count)),
            0.0F, 0.0F, 1.0F);
        instance_transforms.push_back(transform);
    }
    runMultiShapeTask(
        tr("圆周阵列"), object->id, object->name + tr(" 圆周"),
        tr("数量=%1; 总角度=360°").arg(count),
        [source, count](const STaskContext& context)
        {
            const auto kernel = createKernelService();
            QList<SKernelShape> shapes;
            for (int index = 1; index < count; ++index)
            {
                if (context.isCancellationRequested())
                {
                    return SResult<QList<SKernelShape>>::failure(SErrorCode::Cancelled,
                                                                 QObject::tr("圆周阵列已取消"));
                }
                STransformParameters parameters;
                parameters.rotation_degrees =
                    360.0 * static_cast<double>(index) / static_cast<double>(count);
                const auto result = kernel->transform(source, parameters);
                if (!result)
                {
                    return SResult<QList<SKernelShape>>::failure(
                        result.errorCode(), result.message(), result.details());
                }
                shapes.push_back(result.value());
                context.reportProgress(10 + 70 * index / count,
                                       QObject::tr("生成实例 %1/%2").arg(index).arg(count - 1));
            }
            return SResult<QList<SKernelShape>>::success(std::move(shapes));
        },
        copy_mode, instance_transforms);
}
} // namespace smartGraphics3D
