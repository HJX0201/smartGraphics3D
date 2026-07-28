#include "s_main_window.h"

#include <QDateTime>
#include <QInputDialog>
#include <QJsonObject>
#include <QMessageBox>
#include <cmath>

namespace smartGraphics3D
{
namespace
{
QString selectionName(SSelectionMode mode)
{
    switch (mode)
    {
    case SSelectionMode::Vertex:
        return QObject::tr("点");
    case SSelectionMode::Edge:
        return QObject::tr("边");
    case SSelectionMode::Face:
        return QObject::tr("面");
    case SSelectionMode::Solid:
        return QObject::tr("实体");
    case SSelectionMode::Object:
        return QObject::tr("对象");
    }
    return QObject::tr("几何");
}

SSceneObject measurementObject(const QString& name, const QString& kind,
                               const QList<SObjectId>& sources, QJsonObject properties)
{
    SSceneObject measurement;
    measurement.name = name;
    measurement.type = SObjectType::Measurement;
    measurement.stage = SDataStage::Working;
    measurement.source = QObject::tr("精确测量");
    measurement.derived_from = sources;
    properties.insert(QStringLiteral("kind"), kind);
    properties.insert(QStringLiteral("precision"), 3);
    properties.insert(QStringLiteral("createdAt"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!properties.contains(QStringLiteral("annotationX")))
    {
        properties.insert(QStringLiteral("annotationX"), 0.0);
        properties.insert(QStringLiteral("annotationY"), 0.0);
        properties.insert(QStringLiteral("annotationZ"), 0.0);
    }
    measurement.custom_properties = std::move(properties);
    return measurement;
}

double areaScale(const SUnitSystem& units)
{
    const double scale = units.fromMillimeters(1.0);
    return scale * scale;
}
} // namespace

void SMainWindow::measureSubSelection()
{
    if (m_sub_selections.size() != 1 || m_sub_selections.front().sub_shape_index < 1)
    {
        QMessageBox::information(this, tr("点/边/面测量"),
                                 tr("请先切换到点、边或面选择模式，并在视口中选择一个子形状。"));
        return;
    }
    const SSelection selection = m_sub_selections.front();
    const SSceneObject* object = m_document.findObject(selection.object_id);
    if (!object)
    {
        return;
    }
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        QMessageBox::critical(this, tr("测量失败"), materialized.message());
        return;
    }
    const auto result =
        m_kernel->measureSubShape(materialized.value(), selection.mode, selection.sub_shape_index);
    if (!result)
    {
        QMessageBox::critical(this, tr("测量失败"), result.message());
        return;
    }

    const SSubShapeMetrics& value = result.value();
    const SUnitSystem& units = m_document.unitSystem();
    QJsonObject properties;
    properties.insert(QStringLiteral("sourceObject"), object->name);
    properties.insert(QStringLiteral("subShapeIndex"), selection.sub_shape_index);
    properties.insert(QStringLiteral("unit"), units.lengthSuffix());
    QStringList lines;
    if (value.has_point)
    {
        properties.insert(QStringLiteral("x"), units.fromMillimeters(value.point.x()));
        properties.insert(QStringLiteral("y"), units.fromMillimeters(value.point.y()));
        properties.insert(QStringLiteral("z"), units.fromMillimeters(value.point.z()));
        properties.insert(QStringLiteral("annotationX"), value.point.x());
        properties.insert(QStringLiteral("annotationY"), value.point.y());
        properties.insert(QStringLiteral("annotationZ"), value.point.z());
        lines << tr("坐标：(%1, %2, %3) %4")
                     .arg(units.fromMillimeters(value.point.x()), 0, 'f', 3)
                     .arg(units.fromMillimeters(value.point.y()), 0, 'f', 3)
                     .arg(units.fromMillimeters(value.point.z()), 0, 'f', 3)
                     .arg(units.lengthSuffix());
    }
    else
    {
        properties.insert(QStringLiteral("annotationX"), value.center.x());
        properties.insert(QStringLiteral("annotationY"), value.center.y());
        properties.insert(QStringLiteral("annotationZ"), value.center.z());
    }
    if (value.length > 0.0)
    {
        properties.insert(QStringLiteral("length"), units.fromMillimeters(value.length));
        lines << tr("长度/周长：%1 %2")
                     .arg(units.fromMillimeters(value.length), 0, 'f', 3)
                     .arg(units.lengthSuffix());
    }
    if (value.area > 0.0)
    {
        properties.insert(QStringLiteral("area"), value.area * areaScale(units));
        properties.insert(QStringLiteral("areaUnit"), units.lengthSuffix() + QStringLiteral("²"));
        lines << tr("面积：%1 %2²")
                     .arg(value.area * areaScale(units), 0, 'f', 3)
                     .arg(units.lengthSuffix());
    }
    if (value.has_radius)
    {
        properties.insert(QStringLiteral("radius"), units.fromMillimeters(value.radius));
        properties.insert(QStringLiteral("diameter"), units.fromMillimeters(value.radius * 2.0));
        lines << tr("半径：%1 %2；直径：%3 %2")
                     .arg(units.fromMillimeters(value.radius), 0, 'f', 3)
                     .arg(units.lengthSuffix())
                     .arg(units.fromMillimeters(value.radius * 2.0), 0, 'f', 3);
    }
    const QString kind = selectionName(selection.mode) + tr("测量");
    const auto added =
        m_document.addObject(measurementObject(object->name + QStringLiteral(" ") + kind, kind,
                                               {object->id}, properties),
                             tr("创建%1").arg(kind));
    if (!added)
    {
        QMessageBox::critical(this, tr("保存测量失败"), added.message());
        return;
    }
    const QString message = lines.isEmpty() ? tr("已记录几何属性") : lines.join('\n');
    appendLog(QStringLiteral("MEASURE"), kind + QStringLiteral("：") + message);
    QMessageBox::information(this, kind, message);
}

void SMainWindow::measureDistance()
{
    if (m_sub_selections.size() != 2)
    {
        QMessageBox::information(this, tr("距离测量"),
                                 tr("请在视口中按住 Ctrl 选择两个点、边、面或实体。"));
        return;
    }
    const SSelection first = m_sub_selections.at(0);
    const SSelection second = m_sub_selections.at(1);
    const SSceneObject* first_object = m_document.findObject(first.object_id);
    const SSceneObject* second_object = m_document.findObject(second.object_id);
    if (!first_object || !second_object)
    {
        return;
    }
    const auto first_shape = materializedShape(*first_object);
    const auto second_shape = materializedShape(*second_object);
    if (!first_shape || !second_shape)
    {
        QMessageBox::critical(this, tr("距离测量失败"),
                              !first_shape ? first_shape.message() : second_shape.message());
        return;
    }
    const auto result =
        m_kernel->distanceBetween(first_shape.value(), first.mode, first.sub_shape_index,
                                  second_shape.value(), second.mode, second.sub_shape_index);
    if (!result)
    {
        QMessageBox::critical(this, tr("距离测量失败"), result.message());
        return;
    }
    const SUnitSystem& units = m_document.unitSystem();
    const double value = units.fromMillimeters(result.value());
    QJsonObject properties;
    properties.insert(QStringLiteral("sourceObject"),
                      first_object->name + QStringLiteral(" ↔ ") + second_object->name);
    properties.insert(QStringLiteral("value"), value);
    properties.insert(QStringLiteral("unit"), units.lengthSuffix());
    const auto added =
        m_document.addObject(measurementObject(tr("距离 %1").arg(value, 0, 'f', 3), tr("最短距离"),
                                               {first.object_id, second.object_id}, properties),
                             tr("创建距离测量"));
    if (!added)
    {
        QMessageBox::critical(this, tr("保存测量失败"), added.message());
        return;
    }
    QMessageBox::information(this, tr("距离测量"),
                             tr("最短距离：%1 %2").arg(value, 0, 'f', 3).arg(units.lengthSuffix()));
}

void SMainWindow::measureAngle()
{
    if (m_sub_selections.size() != 2)
    {
        QMessageBox::information(this, tr("角度测量"),
                                 tr("请在视口中按住 Ctrl 选择两条直线边或两个平面。"));
        return;
    }
    const SSelection first = m_sub_selections.at(0);
    const SSelection second = m_sub_selections.at(1);
    const SSceneObject* first_object = m_document.findObject(first.object_id);
    const SSceneObject* second_object = m_document.findObject(second.object_id);
    if (!first_object || !second_object)
    {
        return;
    }
    const auto first_shape = materializedShape(*first_object);
    const auto second_shape = materializedShape(*second_object);
    if (!first_shape || !second_shape)
    {
        QMessageBox::critical(this, tr("角度测量失败"),
                              !first_shape ? first_shape.message() : second_shape.message());
        return;
    }
    const auto result =
        m_kernel->angleBetween(first_shape.value(), first.mode, first.sub_shape_index,
                               second_shape.value(), second.mode, second.sub_shape_index);
    if (!result)
    {
        QMessageBox::critical(this, tr("角度测量失败"), result.message());
        return;
    }
    const double value = m_document.unitSystem().fromDegrees(result.value());
    QJsonObject properties;
    properties.insert(QStringLiteral("sourceObject"),
                      first_object->name + QStringLiteral(" ↔ ") + second_object->name);
    properties.insert(QStringLiteral("value"), value);
    properties.insert(QStringLiteral("unit"), m_document.unitSystem().angleSuffix());
    const auto added =
        m_document.addObject(measurementObject(tr("角度 %1").arg(value, 0, 'f', 3), tr("角度"),
                                               {first.object_id, second.object_id}, properties),
                             tr("创建角度测量"));
    if (!added)
    {
        QMessageBox::critical(this, tr("保存测量失败"), added.message());
        return;
    }
    QMessageBox::information(
        this, tr("角度测量"),
        tr("角度：%1 %2").arg(value, 0, 'f', 3).arg(m_document.unitSystem().angleSuffix()));
}

void SMainWindow::createSection()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object || object->shape.isNull())
    {
        QMessageBox::information(this, tr("生成截面"), tr("请选择一个实体。"));
        return;
    }
    bool accepted = false;
    const QString axis = QInputDialog::getItem(this, tr("生成截面"), tr("截面法向"),
                                               {tr("X 轴"), tr("Y 轴"), tr("Z 轴"), tr("任意法向")},
                                               2, false, &accepted);
    if (!accepted)
    {
        return;
    }
    SSectionParameters parameters;
    if (axis == tr("X 轴"))
    {
        parameters.normal = QVector3D(1.0F, 0.0F, 0.0F);
    }
    else if (axis == tr("Y 轴"))
    {
        parameters.normal = QVector3D(0.0F, 1.0F, 0.0F);
    }
    else if (axis == tr("任意法向"))
    {
        double x =
            QInputDialog::getDouble(this, tr("法向"), tr("X"), 1.0, -1.0e6, 1.0e6, 6, &accepted);
        if (!accepted)
        {
            return;
        }
        double y =
            QInputDialog::getDouble(this, tr("法向"), tr("Y"), 1.0, -1.0e6, 1.0e6, 6, &accepted);
        if (!accepted)
        {
            return;
        }
        double z =
            QInputDialog::getDouble(this, tr("法向"), tr("Z"), 1.0, -1.0e6, 1.0e6, 6, &accepted);
        if (!accepted)
        {
            return;
        }
        parameters.normal = QVector3D(x, y, z);
    }
    const double offset = QInputDialog::getDouble(
        this, tr("截面偏移"), tr("沿法向偏移 (%1)").arg(m_document.unitSystem().lengthSuffix()),
        0.0, -1.0e9, 1.0e9, 3, &accepted);
    if (!accepted)
    {
        return;
    }
    if (QMessageBox::question(this, tr("翻转法向"), tr("是否翻转截面法向？"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        parameters.normal = -parameters.normal;
    }
    parameters.normal.normalize();
    parameters.origin =
        parameters.normal * static_cast<float>(m_document.unitSystem().toMillimeters(offset));
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("生成截面失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape shape = materialized.value();
    const SObjectId id = object->id;
    const QString result_name = object->name + tr(" 截面");
    runShapeTask(tr("生成截面"), {id}, result_name,
                 tr("法向=(%1, %2, %3); 偏移=%4 %5")
                     .arg(parameters.normal.x(), 0, 'g', 8)
                     .arg(parameters.normal.y(), 0, 'g', 8)
                     .arg(parameters.normal.z(), 0, 'g', 8)
                     .arg(offset, 0, 'g', 12)
                     .arg(m_document.unitSystem().lengthSuffix()),
                 [shape, parameters](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->section(shape, parameters);
                 });
}
} // namespace smartGraphics3D
