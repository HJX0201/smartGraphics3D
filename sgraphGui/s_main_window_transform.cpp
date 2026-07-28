#include "s_main_window.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QtGlobal>

namespace smartGraphics3D
{
namespace
{
bool requestDouble(QWidget* parent, const QString& title, const QString& label, double initial,
                   double minimum, double maximum, double& value, int decimals = 3)
{
    bool accepted = false;
    const double candidate = QInputDialog::getDouble(parent, title, label, initial, minimum,
                                                     maximum, decimals, &accepted);
    if (accepted)
    {
        value = candidate;
    }
    return accepted;
}
} // namespace

SResult<SKernelShape> SMainWindow::materializedShape(const SSceneObject& object) const
{
    return m_kernel->materialize(object.shape, object.transform);
}

void SMainWindow::runTransform()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("精确变换"), tr("请选择一个实体。"));
        return;
    }

    STransformParameters parameters;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!requestDouble(this, tr("精确变换"), tr("移动 X (mm)"), x, -1.0e9, 1.0e9, x) ||
        !requestDouble(this, tr("精确变换"), tr("移动 Y (mm)"), y, -1.0e9, 1.0e9, y) ||
        !requestDouble(this, tr("精确变换"), tr("移动 Z (mm)"), z, -1.0e9, 1.0e9, z) ||
        !requestDouble(this, tr("精确变换"), tr("绕 Z 轴旋转 (°)"), parameters.rotation_degrees,
                       -360000.0, 360000.0, parameters.rotation_degrees) ||
        !requestDouble(this, tr("精确变换"), tr("统一缩放"), parameters.uniform_scale, 0.000001,
                       1.0e6, parameters.uniform_scale, 6))
    {
        return;
    }
    parameters.translation = QVector3D(x, y, z);
    const QString summary = tr("移动=(%1, %2, %3) mm; 绕Z=%4°; 缩放=%5")
                                .arg(x, 0, 'g', 12)
                                .arg(y, 0, 'g', 12)
                                .arg(z, 0, 'g', 12)
                                .arg(parameters.rotation_degrees, 0, 'g', 12)
                                .arg(parameters.uniform_scale, 0, 'g', 12);

    const bool shared = m_document.presentationGroupMemberCount(object->presentation_group_id) > 1;
    if (shared && qFuzzyCompare(parameters.uniform_scale, 1.0))
    {
        QMatrix4x4 delta;
        delta.translate(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        delta.rotate(static_cast<float>(parameters.rotation_degrees), 0.0F, 0.0F, 1.0F);
        const QMatrix4x4 transformed = delta * object->transform;
        const auto preview = m_kernel->materialize(object->shape, transformed);
        if (!preview)
        {
            showFailure(
                tr("实例变换失败"),
                SResult<void>::failure(preview.errorCode(), preview.message(), preview.details()));
            return;
        }
        bool replace_inputs = false;
        if (!confirmShapePreview(preview.value(), tr("实例变换"), {}, replace_inputs))
        {
            return;
        }
        const auto result = m_document.setObjectTransform(object->id, transformed, tr("实例变换"));
        if (!result)
        {
            showFailure(tr("实例变换失败"), result);
        }
        else
        {
            appendLog(QStringLiteral("INFO"), tr("实例变换已提交"), summary);
        }
        return;
    }

    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("精确变换失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SObjectId id = object->id;
    const QString result_name = object->name + tr(" 变换");
    runShapeTask(tr("精确变换"), {id}, result_name, summary,
                 [shape = materialized.value(), parameters](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->transform(shape, parameters);
                 });
}
} // namespace smartGraphics3D
