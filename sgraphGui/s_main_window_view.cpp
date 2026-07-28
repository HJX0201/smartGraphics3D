#include "s_clip_plane_dialog.h"
#include "s_main_window.h"
#include "s_occ_viewport.h"
#include "s_version.h"

#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSaveFile>
#include <QSysInfo>
#include <QTextStream>

namespace smartGraphics3D
{
namespace
{
SLengthUnit lengthUnitFromIndex(int index)
{
    switch (index)
    {
    case 1:
        return SLengthUnit::Centimeter;
    case 2:
        return SLengthUnit::Meter;
    case 3:
        return SLengthUnit::Inch;
    default:
        return SLengthUnit::Millimeter;
    }
}

int lengthUnitIndex(SLengthUnit unit)
{
    return static_cast<int>(unit);
}

SAngleUnit angleUnitFromIndex(int index)
{
    return index == 1 ? SAngleUnit::Radian : SAngleUnit::Degree;
}

int angleUnitIndex(SAngleUnit unit)
{
    return static_cast<int>(unit);
}

const SCoordinateSystem* coordinateSystemById(const S3dDocument& document, const QUuid& id)
{
    for (const SCoordinateSystem& coordinate_system : document.coordinateSystems())
    {
        if (coordinate_system.id == id)
        {
            return &coordinate_system;
        }
    }
    return nullptr;
}

QString measurementUnit(const QJsonObject& properties, const QString& key)
{
    if (key.compare(QStringLiteral("surfaceArea"), Qt::CaseInsensitive) == 0 ||
        key.compare(QStringLiteral("area"), Qt::CaseInsensitive) == 0)
    {
        return properties.value(QStringLiteral("areaUnit")).toString();
    }
    if (key.compare(QStringLiteral("volume"), Qt::CaseInsensitive) == 0)
    {
        return properties.value(QStringLiteral("volumeUnit")).toString();
    }
    return properties.value(QStringLiteral("unit")).toString();
}

bool isMeasurementExportValue(const QString& key)
{
    return key != QStringLiteral("precision") && key != QStringLiteral("subShapeIndex") &&
           !key.startsWith(QStringLiteral("annotation"));
}
} // namespace

void SMainWindow::setViewportLayout(int layout_mode)
{
    while (m_viewports.size() > 1)
    {
        SOccViewport* viewport = m_viewports.takeLast();
        m_viewport_layout->removeWidget(viewport);
        viewport->deleteLater();
    }

    int count = layout_mode == 4 ? 4 : (layout_mode == 1 ? 1 : 2);
    for (int index = 1; index < count; ++index)
    {
        auto* viewport = new SOccViewport(m_viewport_container);
        viewport->setDocument(&m_document);
        viewport->setPerspective(m_viewport->isPerspective());
        viewport->setFreeRotation(m_viewport->isFreeRotation());
        viewport->setDisplayMode(m_viewport->displayMode());
        viewport->setSelectionMode(m_viewport->selectionMode());
        viewport->setClipPlanes(m_clip_planes);
        connect(viewport, &SOccViewport::selectionChanged, this,
                [this, viewport](const QList<SObjectId>& ids)
                {
                    if (m_synchronizing_selection)
                    {
                        return;
                    }
                    m_synchronizing_selection = true;
                    selectTreeItems(ids);
                    if (m_sync_selections)
                    {
                        for (SOccViewport* target : m_viewports)
                        {
                            if (target != viewport)
                            {
                                target->setSelectedObjects(ids);
                            }
                        }
                    }
                    m_synchronizing_selection = false;
                    refreshProperties();
                });
        connect(viewport, &SOccViewport::subSelectionChanged, this,
                [this](const QList<SSelection>& selections)
                {
                    m_sub_selections = selections;
                });
        connect(viewport, &SOccViewport::cameraChanged, this,
                [this, viewport]()
                {
                    if (!m_sync_cameras || m_synchronizing_camera)
                    {
                        return;
                    }
                    m_synchronizing_camera = true;
                    for (SOccViewport* target : m_viewports)
                    {
                        if (target != viewport)
                        {
                            target->copyCameraFrom(*viewport);
                        }
                    }
                    m_synchronizing_camera = false;
                });
        m_viewports.push_back(viewport);
    }

    m_viewport_layout->addWidget(m_viewport, 0, 0);
    if (layout_mode == 2)
    {
        m_viewport_layout->addWidget(m_viewports.at(1), 0, 1);
        m_viewports.at(1)->setStandardView(SStandardView::Front);
    }
    else if (layout_mode == 3)
    {
        m_viewport_layout->addWidget(m_viewports.at(1), 1, 0);
        m_viewports.at(1)->setStandardView(SStandardView::Top);
    }
    else if (layout_mode == 4)
    {
        m_viewport_layout->addWidget(m_viewports.at(1), 0, 1);
        m_viewport_layout->addWidget(m_viewports.at(2), 1, 0);
        m_viewport_layout->addWidget(m_viewports.at(3), 1, 1);
        m_viewport->setStandardView(SStandardView::Isometric);
        m_viewports.at(1)->setStandardView(SStandardView::Front);
        m_viewports.at(2)->setStandardView(SStandardView::Top);
        m_viewports.at(3)->setStandardView(SStandardView::Right);
    }

    m_viewport_layout->setRowStretch(0, 1);
    m_viewport_layout->setRowStretch(1, layout_mode == 3 || layout_mode == 4 ? 1 : 0);
    m_viewport_layout->setColumnStretch(0, 1);
    m_viewport_layout->setColumnStretch(1, layout_mode == 2 || layout_mode == 4 ? 1 : 0);
}

void SMainWindow::setLengthUnit()
{
    const QStringList length_names = {tr("毫米 (mm)"), tr("厘米 (cm)"), tr("米 (m)"),
                                      tr("英寸 (in)")};
    bool accepted = false;
    const QString selected_length = QInputDialog::getItem(
        this, tr("项目单位"), tr("长度单位"), length_names,
        lengthUnitIndex(m_document.unitSystem().lengthUnit()), false, &accepted);
    if (!accepted)
    {
        return;
    }
    const QStringList angle_names = {tr("度 (°)"), tr("弧度 (rad)")};
    const QString selected_angle = QInputDialog::getItem(
        this, tr("项目单位"), tr("角度单位"), angle_names,
        angleUnitIndex(m_document.unitSystem().angleUnit()), false, &accepted);
    if (!accepted)
    {
        return;
    }
    const auto result =
        m_document.setUnits(lengthUnitFromIndex(length_names.indexOf(selected_length)),
                            angleUnitFromIndex(angle_names.indexOf(selected_angle)));
    if (!result)
    {
        showFailure(tr("修改单位失败"), result);
        return;
    }
    appendLog(
        QStringLiteral("INFO"),
        tr("项目单位已切换为 %1、%2；几何内部仍使用毫米和度，不会缩放模型。")
            .arg(m_document.unitSystem().lengthSuffix(), m_document.unitSystem().angleSuffix()));
    refreshProperties();
}

void SMainWindow::createUserCoordinateSystem()
{
    const auto& coordinate_systems = m_document.coordinateSystems();
    if (coordinate_systems.empty())
    {
        return;
    }
    QStringList parent_names;
    for (const SCoordinateSystem& coordinate_system : coordinate_systems)
    {
        parent_names.push_back(coordinate_system.name);
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("创建用户坐标系"), tr("坐标系名称"),
                                               QLineEdit::Normal, tr("用户坐标系"), &accepted);
    if (!accepted || name.trimmed().isEmpty())
    {
        return;
    }
    const QString parent_name = QInputDialog::getItem(this, tr("创建用户坐标系"), tr("父坐标系"),
                                                      parent_names, 0, false, &accepted);
    if (!accepted)
    {
        return;
    }
    const SUnitSystem& units = m_document.unitSystem();
    const QList<SParameterField> fields = {
        {QStringLiteral("x"), tr("平移 X"), units.lengthSuffix(), tr("相对父坐标系的 X 平移"), 0.0,
         -1.0e9, 1.0e9, 3, false},
        {QStringLiteral("y"), tr("平移 Y"), units.lengthSuffix(), tr("相对父坐标系的 Y 平移"), 0.0,
         -1.0e9, 1.0e9, 3, false},
        {QStringLiteral("z"), tr("平移 Z"), units.lengthSuffix(), tr("相对父坐标系的 Z 平移"), 0.0,
         -1.0e9, 1.0e9, 3, false},
        {QStringLiteral("rx"), tr("绕 X 旋转"), units.angleSuffix(),
         tr("按 X、Y、Z 顺序应用的旋转角"), 0.0, -360000.0, 360000.0, 3, true},
        {QStringLiteral("ry"), tr("绕 Y 旋转"), units.angleSuffix(),
         tr("按 X、Y、Z 顺序应用的旋转角"), 0.0, -360000.0, 360000.0, 3, true},
        {QStringLiteral("rz"), tr("绕 Z 旋转"), units.angleSuffix(),
         tr("按 X、Y、Z 顺序应用的旋转角"), 0.0, -360000.0, 360000.0, 3, true}};
    SParameterDialog dialog(tr("用户坐标系参数"), fields, {}, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    const QMap<QString, double> values = dialog.values();
    QMatrix4x4 transform;
    transform.translate(static_cast<float>(units.toMillimeters(values.value(QStringLiteral("x")))),
                        static_cast<float>(units.toMillimeters(values.value(QStringLiteral("y")))),
                        static_cast<float>(units.toMillimeters(values.value(QStringLiteral("z")))));
    transform.rotate(static_cast<float>(units.toDegrees(values.value(QStringLiteral("rx")))), 1.0F,
                     0.0F, 0.0F);
    transform.rotate(static_cast<float>(units.toDegrees(values.value(QStringLiteral("ry")))), 0.0F,
                     1.0F, 0.0F);
    transform.rotate(static_cast<float>(units.toDegrees(values.value(QStringLiteral("rz")))), 0.0F,
                     0.0F, 1.0F);

    SCoordinateSystem coordinate_system;
    coordinate_system.name = name.trimmed();
    coordinate_system.parent_id =
        coordinate_systems.at(static_cast<std::size_t>(parent_names.indexOf(parent_name))).id;
    coordinate_system.transform_to_parent = transform;
    coordinate_system.source = tr("用户定义");
    coordinate_system.calibrated_at = QDateTime::currentDateTimeUtc();
    const auto result = m_document.addCoordinateSystem(std::move(coordinate_system));
    if (!result)
    {
        showFailure(tr("创建坐标系失败"),
                    SResult<void>::failure(result.errorCode(), result.message(), result.details()));
        return;
    }
    appendLog(QStringLiteral("INFO"), tr("已创建用户坐标系：%1").arg(name.trimmed()));
}

void SMainWindow::createObjectCoordinateSystem()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("从对象创建坐标系"), tr("请先选择一个对象。"));
        return;
    }
    const SObjectId object_id = object->id;
    const QString object_name = object->name;
    SCoordinateSystem coordinate_system;
    coordinate_system.name = object_name + tr(" 坐标系");
    coordinate_system.parent_id = object->coordinate_system_id;
    if (coordinate_system.parent_id.isNull() && !m_document.coordinateSystems().empty())
    {
        coordinate_system.parent_id = m_document.coordinateSystems().front().id;
    }
    coordinate_system.transform_to_parent = object->transform;
    coordinate_system.source = tr("对象：%1").arg(object_name);
    coordinate_system.calibrated_at = QDateTime::currentDateTimeUtc();
    const auto result = m_document.addCoordinateSystem(std::move(coordinate_system));
    if (!result)
    {
        showFailure(tr("创建对象坐标系失败"),
                    SResult<void>::failure(result.errorCode(), result.message(), result.details()));
        return;
    }
    appendLog(QStringLiteral("INFO"),
              tr("已从对象 %1 创建坐标系；来源对象 ID：%2")
                  .arg(object_name, object_id.toString(QUuid::WithoutBraces)));
}

void SMainWindow::showCoordinateSystems()
{
    QStringList lines;
    const auto& systems = m_document.coordinateSystems();
    for (const SCoordinateSystem& coordinate_system : systems)
    {
        const SCoordinateSystem* parent =
            coordinateSystemById(m_document, coordinate_system.parent_id);
        const QString direction = parent ? tr("%1 → %2").arg(coordinate_system.name, parent->name)
                                         : coordinate_system.name;
        lines.push_back(tr("%1\n  来源：%2；有效：%3；误差：%4")
                            .arg(direction, coordinate_system.source,
                                 coordinate_system.valid ? tr("是") : tr("否"))
                            .arg(coordinate_system.error, 0, 'g', 8));
    }
    QMessageBox::information(this, tr("项目坐标系"),
                             lines.isEmpty() ? tr("项目中没有坐标系。") : lines.join('\n'));
}

void SMainWindow::configureClipPlanes()
{
    const QList<SClipPlane> original = m_clip_planes;
    SClipPlaneDialog dialog(
        m_document.unitSystem(), m_clip_planes,
        [this](const QList<SClipPlane>& preview)
        {
            for (SOccViewport* viewport : m_viewports)
            {
                viewport->setClipPlanes(preview);
            }
        },
        this);
    if (dialog.exec() != QDialog::Accepted)
    {
        for (SOccViewport* viewport : m_viewports)
        {
            viewport->setClipPlanes(original);
        }
        return;
    }
    m_clip_planes = dialog.planes();
    appendLog(QStringLiteral("INFO"), m_clip_planes.isEmpty()
                                          ? tr("动态剖切已关闭")
                                          : tr("已启用 %1 个动态剖切面").arg(m_clip_planes.size()));
}

void SMainWindow::measureSelection()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object || object->shape.isNull())
    {
        QMessageBox::information(this, tr("实体统计"), tr("请选择一个实体。"));
        return;
    }
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        return;
    }
    const auto result = m_kernel->measure(materialized.value());
    if (!result)
    {
        QMessageBox::critical(this, tr("测量失败"), result.message());
        return;
    }
    const SShapeMetrics& value = result.value();
    const SUnitSystem& units = m_document.unitSystem();
    const double length_scale = units.fromMillimeters(1.0);
    const double area_scale = length_scale * length_scale;
    const double volume_scale = area_scale * length_scale;
    const QString area_unit = units.lengthSuffix() + QStringLiteral("²");
    const QString volume_unit = units.lengthSuffix() + QStringLiteral("³");
    const QString text =
        tr("对象：%1\n包围盒：%2 × %3 × %4 %5\n表面积：%6 %7\n体积：%8 %9\n"
           "重心：(%10, %11, %12) %5\n拓扑：%13 个实体，%14 个面，%15 条边，%16 个顶点")
            .arg(object->name)
            .arg(units.fromMillimeters(value.maximum.x() - value.minimum.x()), 0, 'f', 3)
            .arg(units.fromMillimeters(value.maximum.y() - value.minimum.y()), 0, 'f', 3)
            .arg(units.fromMillimeters(value.maximum.z() - value.minimum.z()), 0, 'f', 3)
            .arg(units.lengthSuffix())
            .arg(value.surface_area * area_scale, 0, 'f', 3)
            .arg(area_unit)
            .arg(value.volume * volume_scale, 0, 'f', 3)
            .arg(volume_unit)
            .arg(units.fromMillimeters(value.center_of_mass.x()), 0, 'f', 3)
            .arg(units.fromMillimeters(value.center_of_mass.y()), 0, 'f', 3)
            .arg(units.fromMillimeters(value.center_of_mass.z()), 0, 'f', 3)
            .arg(value.solid_count)
            .arg(value.face_count)
            .arg(value.edge_count)
            .arg(value.vertex_count);

    SSceneObject measurement;
    measurement.name = object->name + tr(" 实体统计");
    measurement.type = SObjectType::Measurement;
    measurement.stage = SDataStage::Working;
    measurement.source = tr("实体测量");
    measurement.derived_from = {object->id};
    QJsonObject& properties = measurement.custom_properties;
    properties.insert(QStringLiteral("sourceObject"), object->name);
    properties.insert(QStringLiteral("kind"), tr("实体统计"));
    properties.insert(QStringLiteral("precision"), 3);
    properties.insert(QStringLiteral("unit"), units.lengthSuffix());
    properties.insert(QStringLiteral("areaUnit"), area_unit);
    properties.insert(QStringLiteral("volumeUnit"), volume_unit);
    properties.insert(QStringLiteral("width"),
                      units.fromMillimeters(value.maximum.x() - value.minimum.x()));
    properties.insert(QStringLiteral("height"),
                      units.fromMillimeters(value.maximum.y() - value.minimum.y()));
    properties.insert(QStringLiteral("depth"),
                      units.fromMillimeters(value.maximum.z() - value.minimum.z()));
    properties.insert(QStringLiteral("surfaceArea"), value.surface_area * area_scale);
    properties.insert(QStringLiteral("volume"), value.volume * volume_scale);
    properties.insert(QStringLiteral("centerX"), units.fromMillimeters(value.center_of_mass.x()));
    properties.insert(QStringLiteral("centerY"), units.fromMillimeters(value.center_of_mass.y()));
    properties.insert(QStringLiteral("centerZ"), units.fromMillimeters(value.center_of_mass.z()));
    properties.insert(QStringLiteral("annotationX"), value.center_of_mass.x());
    properties.insert(QStringLiteral("annotationY"), value.center_of_mass.y());
    properties.insert(QStringLiteral("annotationZ"), value.center_of_mass.z());
    properties.insert(QStringLiteral("createdAt"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const auto added = m_document.addObject(std::move(measurement), tr("创建实体测量"));
    if (!added)
    {
        QMessageBox::critical(this, tr("保存测量失败"), added.message());
        return;
    }
    appendLog(QStringLiteral("MEASURE"), text);
    QMessageBox::information(this, tr("实体统计"), text);
}

void SMainWindow::exportMeasurements()
{
    const QString filter = tr("CSV 表格 (*.csv);;JSON 数据 (*.json)");
    QString path = QFileDialog::getSaveFileName(this, tr("导出测量结果"), {}, filter);
    if (path.isEmpty())
    {
        return;
    }
    const bool json = path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive);
    if (!json && !path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
    {
        path += QStringLiteral(".csv");
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, tr("导出测量失败"), file.errorString());
        return;
    }

    if (json)
    {
        QJsonArray measurements;
        for (const SSceneObject& object : m_document.objects())
        {
            if (object.type != SObjectType::Measurement)
            {
                continue;
            }
            QJsonObject item = object.custom_properties;
            item.insert(QStringLiteral("id"), object.id.toString(QUuid::WithoutBraces));
            item.insert(QStringLiteral("name"), object.name);
            item.insert(QStringLiteral("valid"), !object.quality_warning);
            item.insert(QStringLiteral("createdAt"), object.created_at.toString(Qt::ISODateWithMs));
            measurements.push_back(item);
        }
        file.write(QJsonDocument(measurements).toJson(QJsonDocument::Indented));
    }
    else
    {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << QString::fromUtf8("\xEF\xBB\xBF");
        stream << "id,name,source,kind,value,unit,valid,createdAt\n";
        for (const SSceneObject& object : m_document.objects())
        {
            if (object.type != SObjectType::Measurement)
            {
                continue;
            }
            const QJsonObject values = object.custom_properties;
            for (auto iterator = values.constBegin(); iterator != values.constEnd(); ++iterator)
            {
                if (!iterator.value().isDouble() || !isMeasurementExportValue(iterator.key()))
                {
                    continue;
                }
                stream << object.id.toString(QUuid::WithoutBraces) << ',' << '"' << object.name
                       << '"' << ',' << '"'
                       << values.value(QStringLiteral("sourceObject")).toString() << '"' << ','
                       << '"' << iterator.key() << '"' << ','
                       << QString::number(iterator.value().toDouble(), 'g', 16) << ','
                       << measurementUnit(values, iterator.key()) << ','
                       << (object.quality_warning ? "false" : "true") << ','
                       << object.created_at.toString(Qt::ISODateWithMs) << '\n';
            }
        }
    }

    if (!file.commit())
    {
        QMessageBox::critical(this, tr("导出测量失败"), file.errorString());
        return;
    }
    appendLog(QStringLiteral("INFO"), tr("测量结果已导出：%1").arg(path));
}

void SMainWindow::exportViewportImage()
{
    QString path = QFileDialog::getSaveFileName(this, tr("导出视口截图"), {},
                                                tr("PNG 图像 (*.png);;JPEG 图像 (*.jpg)"));
    if (path.isEmpty())
    {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty())
    {
        path += QStringLiteral(".png");
    }
    if (!m_viewport_container->grab().save(path))
    {
        QMessageBox::critical(this, tr("导出截图失败"), tr("无法编码或写入目标图像。"));
        return;
    }
    appendLog(QStringLiteral("INFO"), tr("视口截图已导出：%1").arg(path));
}

void SMainWindow::exportDiagnostics()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出诊断包"),
        QStringLiteral("smartGraphics3D-diagnostics-%1.sgdiag")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))),
        tr("smartGraphics3D 诊断包 (*.sgdiag)"));
    if (path.isEmpty())
    {
        return;
    }
    if (!path.endsWith(QStringLiteral(".sgdiag"), Qt::CaseInsensitive))
    {
        path += QStringLiteral(".sgdiag");
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("smartGraphics3DDiagnostics"));
    root.insert(QStringLiteral("formatVersion"), 1);
    root.insert(QStringLiteral("applicationVersion"), QString::fromLatin1(SMARTGRAPHICS3D_VERSION));
    root.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    root.insert(QStringLiteral("occtVersion"), QStringLiteral("7.7.0"));
    root.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
    root.insert(QStringLiteral("cpuArchitecture"), QSysInfo::currentCpuArchitecture());
    root.insert(QStringLiteral("projectId"), m_document.projectId().toString(QUuid::WithoutBraces));
    root.insert(QStringLiteral("projectName"), m_document.projectName());
    root.insert(QStringLiteral("objectCount"), static_cast<int>(m_document.objects().size()));
    root.insert(QStringLiteral("containsModelData"), false);
    root.insert(QStringLiteral("createdAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QJsonArray logs;
    for (const QString& line : m_structured_logs)
    {
        const QJsonDocument record = QJsonDocument::fromJson(line.toUtf8());
        if (record.isObject())
        {
            logs.push_back(record.object());
        }
    }
    root.insert(QStringLiteral("logs"), logs);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
    {
        QMessageBox::critical(this, tr("导出诊断包失败"), file.errorString());
        return;
    }
    appendLog(QStringLiteral("INFO"), tr("诊断包已导出：%1").arg(path));
}
} // namespace smartGraphics3D
