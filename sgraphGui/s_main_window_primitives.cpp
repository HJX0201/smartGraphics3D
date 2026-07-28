#include "s_main_window.h"
#include "s_occ_viewport.h"
#include "s_parameter_dialog.h"

#include <QDialog>
#include <QMessageBox>

namespace smartGraphics3D
{
namespace
{
QList<SParameterField> positionFields()
{
    return {{QStringLiteral("x"), QObject::tr("位置 X"), QStringLiteral("mm"),
             QObject::tr("创建后沿世界 X 轴移动；仅影响位置，不改变尺寸。"), 0.0, -1.0e9, 1.0e9, 3,
             true},
            {QStringLiteral("y"), QObject::tr("位置 Y"), QStringLiteral("mm"),
             QObject::tr("创建后沿世界 Y 轴移动；仅影响位置，不改变尺寸。"), 0.0, -1.0e9, 1.0e9, 3,
             true},
            {QStringLiteral("z"), QObject::tr("位置 Z"), QStringLiteral("mm"),
             QObject::tr("创建后沿世界 Z 轴移动；仅影响位置，不改变尺寸。"), 0.0, -1.0e9, 1.0e9, 3,
             true}};
}

SResult<SKernelShape> placeShape(const SIKernelService& kernel, const SResult<SKernelShape>& source,
                                 const QMap<QString, double>& values)
{
    if (!source)
    {
        return source;
    }
    STransformParameters transform;
    transform.translation =
        QVector3D(values.value(QStringLiteral("x")), values.value(QStringLiteral("y")),
                  values.value(QStringLiteral("z")));
    if (transform.translation.isNull())
    {
        return source;
    }
    return kernel.transform(source.value(), transform);
}

QString parameterSummary(const QList<SParameterField>& fields, const QMap<QString, double>& values)
{
    QStringList parts;
    for (const SParameterField& field : fields)
    {
        QString value = QString::number(values.value(field.key), 'f', field.decimals);
        if (!field.unit.isEmpty())
        {
            value += QStringLiteral(" ") + field.unit;
        }
        parts.push_back(QStringLiteral("%1=%2").arg(field.label, value));
    }
    return parts.join(QStringLiteral("; "));
}
} // namespace

void SMainWindow::createPrimitiveDialog(
    const QString& title, const QList<SParameterField>& fields,
    std::function<SResult<SKernelShape>(const QMap<QString, double>&)> generator)
{
    SParameterDialog dialog(
        title, fields,
        [this, &generator](const QMap<QString, double>& values)
        {
            const auto preview = generator(values);
            if (preview)
            {
                m_viewport->showPreview(preview.value());
            }
            else
            {
                m_viewport->clearPreview();
            }
        },
        this);
    const int result = dialog.exec();
    m_viewport->clearPreview();
    if (result != QDialog::Accepted)
    {
        return;
    }
    const auto shape = generator(dialog.values());
    if (!shape)
    {
        QMessageBox::critical(this, tr("创建失败"),
                              tr("%1\n\n可能原因：%2\n项目未被修改。")
                                  .arg(shape.message(), shape.details().isEmpty()
                                                            ? tr("参数超出几何可行范围")
                                                            : shape.details()));
        return;
    }
    addShape(shape.value(), title, SDataStage::Working, {},
             parameterSummary(fields, dialog.values()));
    m_viewport->fitAll();
}

void SMainWindow::createBox()
{
    QList<SParameterField> fields = {{QStringLiteral("length"), tr("长度"), QStringLiteral("mm"),
                                      tr("长方体沿 X 轴的尺寸。"), 100.0, 0.001, 1.0e9, 3, false},
                                     {QStringLiteral("width"), tr("宽度"), QStringLiteral("mm"),
                                      tr("长方体沿 Y 轴的尺寸。"), 80.0, 0.001, 1.0e9, 3, false},
                                     {QStringLiteral("height"), tr("高度"), QStringLiteral("mm"),
                                      tr("长方体沿 Z 轴的尺寸。"), 60.0, 0.001, 1.0e9, 3, false}};
    fields.append(positionFields());
    createPrimitiveDialog(tr("长方体"), fields,
                          [this](const QMap<QString, double>& values)
                          {
                              return placeShape(
                                  *m_kernel,
                                  m_kernel->makeBox({values.value(QStringLiteral("length")),
                                                     values.value(QStringLiteral("width")),
                                                     values.value(QStringLiteral("height"))}),
                                  values);
                          });
}

void SMainWindow::createCylinder()
{
    QList<SParameterField> fields = {{QStringLiteral("radius"), tr("半径"), QStringLiteral("mm"),
                                      tr("圆柱底面的半径。"), 30.0, 0.001, 1.0e9, 3, false},
                                     {QStringLiteral("height"), tr("高度"), QStringLiteral("mm"),
                                      tr("圆柱沿 Z 轴的高度。"), 80.0, 0.001, 1.0e9, 3, false}};
    fields.append(positionFields());
    createPrimitiveDialog(tr("圆柱"), fields,
                          [this](const QMap<QString, double>& values)
                          {
                              return placeShape(
                                  *m_kernel,
                                  m_kernel->makeCylinder({values.value(QStringLiteral("radius")),
                                                          values.value(QStringLiteral("height"))}),
                                  values);
                          });
}

void SMainWindow::createCone()
{
    QList<SParameterField> fields = {
        {QStringLiteral("bottomRadius"), tr("底半径"), QStringLiteral("mm"), tr("圆锥底面的半径。"),
         35.0, 0.001, 1.0e9, 3, false},
        {QStringLiteral("topRadius"), tr("顶半径"), QStringLiteral("mm"),
         tr("顶半径为零时生成尖锥。"), 10.0, 0.0, 1.0e9, 3, false},
        {QStringLiteral("height"), tr("高度"), QStringLiteral("mm"), tr("圆锥沿 Z 轴的高度。"),
         80.0, 0.001, 1.0e9, 3, false}};
    fields.append(positionFields());
    createPrimitiveDialog(tr("圆锥"), fields,
                          [this](const QMap<QString, double>& values)
                          {
                              return placeShape(
                                  *m_kernel,
                                  m_kernel->makeCone({values.value(QStringLiteral("bottomRadius")),
                                                      values.value(QStringLiteral("topRadius")),
                                                      values.value(QStringLiteral("height"))}),
                                  values);
                          });
}

void SMainWindow::createSphere()
{
    QList<SParameterField> fields = {{QStringLiteral("radius"), tr("半径"), QStringLiteral("mm"),
                                      tr("球体半径。"), 40.0, 0.001, 1.0e9, 3, false}};
    fields.append(positionFields());
    createPrimitiveDialog(
        tr("球体"), fields,
        [this](const QMap<QString, double>& values)
        {
            return placeShape(
                *m_kernel, m_kernel->makeSphere({values.value(QStringLiteral("radius"))}), values);
        });
}

void SMainWindow::createTorus()
{
    QList<SParameterField> fields = {
        {QStringLiteral("majorRadius"), tr("主半径"), QStringLiteral("mm"),
         tr("圆环中心到管中心的距离，必须大于管半径。"), 45.0, 0.001, 1.0e9, 3, false},
        {QStringLiteral("minorRadius"), tr("管半径"), QStringLiteral("mm"),
         tr("圆环截面管的半径。"), 12.0, 0.001, 1.0e9, 3, false}};
    fields.append(positionFields());
    createPrimitiveDialog(
        tr("圆环体"), fields,
        [this](const QMap<QString, double>& values)
        {
            return placeShape(*m_kernel,
                              m_kernel->makeTorus({values.value(QStringLiteral("majorRadius")),
                                                   values.value(QStringLiteral("minorRadius"))}),
                              values);
        });
}
} // namespace smartGraphics3D
