#pragma once

#include "s_kernel_shape.h"
#include "s_types.h"
#include "s_unit_system.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <vector>

namespace smartGraphics3D
{
struct SSceneObject
{
    SObjectId id = QUuid::createUuid();
    SObjectId parent_id;
    SObjectId coordinate_system_id;
    QString name;
    SObjectType type = SObjectType::CadShape;
    SDataStage stage = SDataStage::Working;
    SKernelShape shape;
    QString source;
    QString external_path;
    QList<SObjectId> derived_from;
    QMatrix4x4 transform;
    QUuid presentation_group_id = QUuid::createUuid();
    SDisplayStyle display;
    SImportedAppearance imported_appearance;
    bool use_imported_appearance = false;
    bool visible = true;
    bool locked = false;
    bool frozen = false;
    bool external_reference = false;
    bool quality_warning = false;
    QString quality_message;
    int data_version = 1;
    QDateTime created_at = QDateTime::currentDateTimeUtc();
    QDateTime modified_at = created_at;
    QJsonObject custom_properties;
};

struct SOperationRecord
{
    QUuid id = QUuid::createUuid();
    QString name;
    QString parameter_summary;
    QList<SObjectId> object_ids;
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    bool success = true;
    bool undoable = true;
};

struct SSnapshotRecord
{
    QString name;
    QDateTime created_at = QDateTime::currentDateTimeUtc();
    QString project_name;
    SUnitSystem units;
    std::vector<SSceneObject> objects;
    std::vector<SCoordinateSystem> coordinate_systems;
};
} // namespace smartGraphics3D
