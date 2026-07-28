#include "s_coordinate_system.h"
#include "s_icon_factory.h"
#include "s_main_window.h"

#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <functional>

namespace smartGraphics3D
{
namespace
{
QString objectTypeName(SObjectType type)
{
    switch (type)
    {
    case SObjectType::Group:
        return QObject::tr("组");
    case SObjectType::CadShape:
        return QObject::tr("CAD 实体");
    case SObjectType::Mesh:
        return QObject::tr("网格");
    case SObjectType::Measurement:
        return QObject::tr("测量");
    case SObjectType::CoordinateSystem:
        return QObject::tr("坐标系");
    }
    return QObject::tr("对象");
}

SIconId objectTypeIcon(SObjectType type)
{
    switch (type)
    {
    case SObjectType::Group:
        return SIconId::SceneGroup;
    case SObjectType::CadShape:
        return SIconId::SceneCadShape;
    case SObjectType::Mesh:
        return SIconId::SceneMesh;
    case SObjectType::Measurement:
        return SIconId::SceneMeasurement;
    case SObjectType::CoordinateSystem:
        return SIconId::SceneCoordinateSystem;
    }
    return SIconId::SceneCadShape;
}

QString stageName(SDataStage stage)
{
    switch (stage)
    {
    case SDataStage::Original:
        return QObject::tr("原始导入");
    case SDataStage::Working:
        return QObject::tr("工作结果");
    case SDataStage::Published:
        return QObject::tr("发布结果");
    }
    return {};
}

QString displayModeName(SDisplayMode mode)
{
    switch (mode)
    {
    case SDisplayMode::Shaded:
        return QObject::tr("着色实体");
    case SDisplayMode::Wireframe:
        return QObject::tr("线框");
    case SDisplayMode::ShadedWithEdges:
        return QObject::tr("着色加边线");
    case SDisplayMode::HiddenLine:
        return QObject::tr("隐藏线");
    case SDisplayMode::Transparent:
        return QObject::tr("半透明");
    }
    return {};
}

QString coordinateSystemName(const S3dDocument& document, const QUuid& id)
{
    for (const SCoordinateSystem& coordinate_system : document.coordinateSystems())
    {
        if (coordinate_system.id == id)
        {
            return coordinate_system.name;
        }
    }
    return QObject::tr("未知坐标系");
}

QTreeWidgetItem* addCategory(QTreeWidget* tree, const QString& name)
{
    auto* item = new QTreeWidgetItem(tree);
    item->setText(0, name);
    item->setFirstColumnSpanned(true);
    item->setExpanded(true);
    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    return item;
}

void addProperty(QTreeWidgetItem* category, const QString& name, const QString& value)
{
    auto* item = new QTreeWidgetItem(category);
    item->setText(0, name);
    item->setText(1, value);
}

QString measurementUnit(const QJsonObject& properties, const QString& key)
{
    if (key == QStringLiteral("area") || key == QStringLiteral("surfaceArea"))
    {
        return properties.value(QStringLiteral("areaUnit")).toString();
    }
    if (key == QStringLiteral("volume"))
    {
        return properties.value(QStringLiteral("volumeUnit")).toString();
    }
    return properties.value(QStringLiteral("unit")).toString();
}

bool isMeasurementMetadata(const QString& key)
{
    return key == QStringLiteral("precision") || key == QStringLiteral("subShapeIndex") ||
           key.startsWith(QStringLiteral("annotation"));
}

void visitItems(QTreeWidgetItem* item, const std::function<void(QTreeWidgetItem*)>& visitor)
{
    visitor(item);
    for (int index = 0; index < item->childCount(); ++index)
    {
        visitItems(item->child(index), visitor);
    }
}
} // namespace

void SMainWindow::refreshSceneTree()
{
    const QList<SObjectId> selected = selectedObjectIds();
    const QString filter = m_scene_filter ? m_scene_filter->text().trimmed() : QString();
    m_refreshing_tree = true;
    m_scene_tree->clear();
    QMap<SObjectId, QTreeWidgetItem*> items;
    int face_count = 0;
    for (const SSceneObject& object : m_document.objects())
    {
        if (!filter.isEmpty() && !object.name.contains(filter, Qt::CaseInsensitive) &&
            !objectTypeName(object.type).contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }
        auto* item = new QTreeWidgetItem();
        items.insert(object.id, item);
        item->setText(0, object.name);
        item->setIcon(0, iconForAction(objectTypeIcon(object.type)));
        item->setToolTip(0, tr("类型：%1").arg(objectTypeName(object.type)));
        item->setData(0, Qt::UserRole, object.id.toString(QUuid::WithoutBraces));
        item->setText(1, stageName(object.stage));
        QStringList states;
        if (object.locked)
        {
            states.push_back(tr("锁定"));
            states.push_back(tr("只读"));
        }
        if (object.frozen)
        {
            states.push_back(tr("冻结"));
        }
        if (object.external_reference)
        {
            states.push_back(tr("外部引用"));
        }
        if (object.quality_warning)
        {
            states.push_back(tr("警告"));
        }
        const QString task_state =
            object.custom_properties.value(QStringLiteral("taskState")).toString();
        if (task_state == QStringLiteral("running"))
        {
            states.push_back(tr("计算中"));
        }
        else if (task_state == QStringLiteral("failed"))
        {
            states.push_back(tr("失败"));
        }
        if (m_document.isDirty())
        {
            states.push_back(tr("未保存"));
        }
        item->setText(2, states.join(QStringLiteral(" · ")));
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable |
                       Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        item->setCheckState(0, object.visible ? Qt::Checked : Qt::Unchecked);
        item->setSelected(selected.contains(object.id));
        const auto materialized = materializedShape(object);
        if (materialized)
        {
            const auto metrics = m_kernel->measure(materialized.value());
            if (metrics)
            {
                face_count += metrics.value().face_count;
            }
        }
    }

    for (const SSceneObject& object : m_document.objects())
    {
        QTreeWidgetItem* item = items.value(object.id, nullptr);
        if (!item)
        {
            continue;
        }
        QTreeWidgetItem* parent = items.value(object.parent_id, nullptr);
        if (parent)
        {
            parent->addChild(item);
        }
        else
        {
            m_scene_tree->addTopLevelItem(item);
        }
    }
    m_scene_tree->expandAll();
    m_statistics_label->setText(
        tr("对象 %1  面 %2").arg(m_document.objects().size()).arg(face_count));
    m_refreshing_tree = false;
}

void SMainWindow::refreshProperties()
{
    m_property_tree->clear();
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        m_selection_label->setText(tr("已选 %1 个对象").arg(m_scene_tree->selectedItems().size()));
        const QString world_name = m_document.coordinateSystems().empty()
                                       ? tr("无活动坐标系")
                                       : m_document.coordinateSystems().front().name;
        m_coordinate_system_label->setText(
            tr("%1 · %2").arg(world_name, m_document.unitSystem().lengthSuffix()));
        return;
    }
    m_selection_label->setText(tr("对象选择：%1").arg(object->name));
    m_coordinate_system_label->setText(
        tr("%1 · %2").arg(coordinateSystemName(m_document, object->coordinate_system_id),
                          m_document.unitSystem().lengthSuffix()));

    QTreeWidgetItem* basic = addCategory(m_property_tree, tr("基本信息"));
    addProperty(basic, tr("名称"), object->name);
    addProperty(basic, tr("对象 ID"), object->id.toString(QUuid::WithoutBraces));
    addProperty(basic, tr("类型"), objectTypeName(object->type));
    addProperty(basic, tr("阶段"), stageName(object->stage));
    addProperty(basic, tr("数据版本"), QString::number(object->data_version));
    addProperty(basic, tr("锁定"), object->locked ? tr("是") : tr("否"));
    addProperty(basic, tr("冻结"), object->frozen ? tr("是") : tr("否"));

    QTreeWidgetItem* transform = addCategory(m_property_tree, tr("变换"));
    const QUuid world_id = m_document.coordinateSystems().empty()
                               ? QUuid()
                               : m_document.coordinateSystems().front().id;
    addProperty(transform, tr("坐标方向"),
                SCoordinateSystemService::directionLabel(m_document.coordinateSystems(),
                                                         object->coordinate_system_id, world_id));
    addProperty(transform, tr("平移 X"), QString::number(object->transform(0, 3), 'f', 3));
    addProperty(transform, tr("平移 Y"), QString::number(object->transform(1, 3), 'f', 3));
    addProperty(transform, tr("平移 Z"), QString::number(object->transform(2, 3), 'f', 3));

    QTreeWidgetItem* display = addCategory(m_property_tree, tr("显示"));
    addProperty(display, tr("可见"), object->visible ? tr("是") : tr("否"));
    addProperty(display, tr("显示模式"), displayModeName(object->display.mode));
    addProperty(display, tr("颜色"), object->display.color.name());
    addProperty(display, tr("透明度"), QString::number(object->display.transparency, 'f', 2));
    const int presentation_members =
        m_document.presentationGroupMemberCount(object->presentation_group_id);
    addProperty(display, tr("显示资源"),
                presentation_members > 1 ? tr("共享实例（%1 个对象）").arg(presentation_members)
                                         : tr("独立显示"));

    QTreeWidgetItem* source = addCategory(m_property_tree, tr("数据来源"));
    addProperty(source, tr("来源"), object->source);
    addProperty(source, tr("外部引用"), object->external_reference ? tr("是") : tr("否"));
    if (!object->external_path.isEmpty())
    {
        addProperty(source, tr("外部路径"), object->external_path);
    }
    addProperty(source, tr("创建时间"), object->created_at.toLocalTime().toString(Qt::ISODate));
    addProperty(source, tr("修改时间"), object->modified_at.toLocalTime().toString(Qt::ISODate));

    QTreeWidgetItem* quality = addCategory(m_property_tree, tr("质量信息"));
    addProperty(quality, tr("状态"), object->quality_warning ? tr("警告") : tr("正常"));
    if (object->quality_warning)
    {
        addProperty(quality, tr("质量警告"), object->quality_message);
    }
    if (object->type == SObjectType::Measurement)
    {
        const QJsonObject properties = object->custom_properties;
        QTreeWidgetItem* measurement = addCategory(m_property_tree, tr("测量结果"));
        addProperty(measurement, tr("关联对象"),
                    properties.value(QStringLiteral("sourceObject")).toString());
        addProperty(measurement, tr("测量类型"),
                    properties.value(QStringLiteral("kind")).toString());
        addProperty(measurement, tr("精度"),
                    QString::number(properties.value(QStringLiteral("precision")).toInt(3)));
        for (auto iterator = properties.constBegin(); iterator != properties.constEnd(); ++iterator)
        {
            if (iterator.value().isDouble() && !isMeasurementMetadata(iterator.key()))
            {
                addProperty(measurement, iterator.key(),
                            QStringLiteral("%1 %2")
                                .arg(iterator.value().toDouble(), 0, 'f', 3)
                                .arg(measurementUnit(properties, iterator.key())));
            }
        }
    }
    else
    {
        const auto materialized = materializedShape(*object);
        if (materialized)
        {
            const auto metrics = m_kernel->measure(materialized.value());
            if (metrics)
            {
                QTreeWidgetItem* geometry = addCategory(m_property_tree, tr("几何统计"));
                const SShapeMetrics& value = metrics.value();
                const SUnitSystem& units = m_document.unitSystem();
                addProperty(geometry, tr("包围盒"),
                            tr("%1 × %2 × %3 %4")
                                .arg(units.fromMillimeters(value.maximum.x() - value.minimum.x()),
                                     0, 'f', 3)
                                .arg(units.fromMillimeters(value.maximum.y() - value.minimum.y()),
                                     0, 'f', 3)
                                .arg(units.fromMillimeters(value.maximum.z() - value.minimum.z()),
                                     0, 'f', 3)
                                .arg(units.lengthSuffix()));
                addProperty(geometry, tr("表面积"),
                            tr("%1 mm²").arg(value.surface_area, 0, 'f', 3));
                addProperty(geometry, tr("体积"), tr("%1 mm³").arg(value.volume, 0, 'f', 3));
                addProperty(geometry, tr("拓扑"),
                            tr("%1 面 / %2 边 / %3 点")
                                .arg(value.face_count)
                                .arg(value.edge_count)
                                .arg(value.vertex_count));
            }
        }
    }

    QTreeWidgetItem* processing = addCategory(m_property_tree, tr("处理历史"));
    for (const SOperationRecord& record : m_document.history())
    {
        if (record.object_ids.contains(object->id))
        {
            addProperty(processing,
                        record.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss")),
                        record.name);
        }
    }
    if (processing->childCount() == 0)
    {
        addProperty(processing, tr("记录"), tr("暂无关联操作"));
    }

    QTreeWidgetItem* custom = addCategory(m_property_tree, tr("自定义属性"));
    for (auto iterator = object->custom_properties.constBegin();
         iterator != object->custom_properties.constEnd(); ++iterator)
    {
        addProperty(custom, iterator.key(), iterator.value().toVariant().toString());
    }
    if (custom->childCount() == 0)
    {
        addProperty(custom, tr("属性"), tr("无"));
    }
}

void SMainWindow::refreshHistory()
{
    m_history_table->setRowCount(m_document.history().size());
    int row = 0;
    for (const SOperationRecord& record : m_document.history())
    {
        QStringList object_names;
        for (const SObjectId& id : record.object_ids)
        {
            const SSceneObject* object = m_document.findObject(id);
            object_names.push_back(object ? object->name
                                          : id.toString(QUuid::WithoutBraces).left(8));
        }
        m_history_table->setItem(row, 0,
                                 new QTableWidgetItem(record.timestamp.toLocalTime().toString(
                                     QStringLiteral("HH:mm:ss"))));
        m_history_table->setItem(row, 1, new QTableWidgetItem(record.name));
        m_history_table->setItem(row, 2,
                                 new QTableWidgetItem(object_names.join(QStringLiteral("、"))));
        m_history_table->setItem(row, 3, new QTableWidgetItem(record.parameter_summary));
        m_history_table->setItem(row, 4,
                                 new QTableWidgetItem(record.success ? tr("成功") : tr("失败")));
        m_history_table->setItem(row, 5,
                                 new QTableWidgetItem(record.undoable ? tr("是") : tr("否")));
        ++row;
    }
}

QList<SObjectId> SMainWindow::selectedObjectIds() const
{
    QList<SObjectId> ids;
    for (QTreeWidgetItem* item : m_scene_tree->selectedItems())
    {
        ids.push_back(QUuid(item->data(0, Qt::UserRole).toString()));
    }
    return ids;
}

const SSceneObject* SMainWindow::singleSelectedObject() const
{
    const QList<SObjectId> ids = selectedObjectIds();
    return ids.size() == 1 ? m_document.findObject(ids.front()) : nullptr;
}

void SMainWindow::selectTreeItems(const QList<SObjectId>& ids)
{
    m_refreshing_tree = true;
    for (int index = 0; index < m_scene_tree->topLevelItemCount(); ++index)
    {
        visitItems(m_scene_tree->topLevelItem(index),
                   [&ids](QTreeWidgetItem* item)
                   {
                       item->setSelected(
                           ids.contains(QUuid(item->data(0, Qt::UserRole).toString())));
                   });
    }
    m_refreshing_tree = false;
}
} // namespace smartGraphics3D
