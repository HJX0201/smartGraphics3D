#include "s_project_validation.h"

#include "s_kernel_shape_access.h"
#include "s_transform_utils.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <cmath>

namespace smartGraphics3D::projectValidation
{
namespace
{
bool isValidObjectType(SObjectType type)
{
    const int value = static_cast<int>(type);
    return value >= static_cast<int>(SObjectType::Group) &&
           value <= static_cast<int>(SObjectType::CoordinateSystem);
}

bool isValidDataStage(SDataStage stage)
{
    const int value = static_cast<int>(stage);
    return value >= static_cast<int>(SDataStage::Original) &&
           value <= static_cast<int>(SDataStage::Published);
}

bool isValidDisplayMode(SDisplayMode mode)
{
    const int value = static_cast<int>(mode);
    return value >= static_cast<int>(SDisplayMode::Shaded) &&
           value <= static_cast<int>(SDisplayMode::Transparent);
}

bool isValidAppearanceStyle(const SAppearanceStyle& style)
{
    return style.color.isValid() && std::isfinite(style.transparency) &&
           style.transparency >= 0.0 && style.transparency <= 1.0;
}

bool isValidImportedAppearance(const SSceneObject& object)
{
    const SImportedAppearance& appearance = object.imported_appearance;
    if (!appearance.valid)
    {
        return !object.use_imported_appearance;
    }
    if (object.shape.isNull() || !isValidAppearanceStyle(appearance.base_style) ||
        !isValidAppearanceStyle(appearance.fallback_style))
    {
        return false;
    }

    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(SKernelShapeAccess::native(object.shape), TopAbs_FACE, faces);
    QSet<int> indices;
    for (const SFaceAppearance& face : appearance.face_overrides)
    {
        if (face.face_index <= 0 || face.face_index > faces.Extent() ||
            indices.contains(face.face_index) || !isValidAppearanceStyle(face.style))
        {
            return false;
        }
        indices.insert(face.face_index);
    }
    return true;
}
} // namespace

bool isValidLengthUnit(int value)
{
    return value >= static_cast<int>(SLengthUnit::Millimeter) &&
           value <= static_cast<int>(SLengthUnit::Inch);
}

bool isValidAngleUnit(int value)
{
    return value >= static_cast<int>(SAngleUnit::Degree) &&
           value <= static_cast<int>(SAngleUnit::Radian);
}

SResult<void> validateCoordinateSystems(const std::vector<SCoordinateSystem>& coordinate_systems)
{
    if (coordinate_systems.empty())
    {
        return SResult<void>::failure(SErrorCode::CorruptData, QObject::tr("项目缺少世界坐标系"));
    }

    QHash<QUuid, QUuid> parents;
    int root_count = 0;
    for (const SCoordinateSystem& coordinate_system : coordinate_systems)
    {
        if (coordinate_system.id.isNull() || coordinate_system.name.trimmed().isEmpty())
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目包含无效坐标系"));
        }
        if (parents.contains(coordinate_system.id))
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目包含重复坐标系 ID"));
        }
        parents.insert(coordinate_system.id, coordinate_system.parent_id);
        if (coordinate_system.parent_id.isNull())
        {
            ++root_count;
        }
    }
    if (root_count != 1)
    {
        return SResult<void>::failure(SErrorCode::CorruptData,
                                      QObject::tr("项目必须且只能包含一个世界坐标系"));
    }

    for (auto iterator = parents.cbegin(); iterator != parents.cend(); ++iterator)
    {
        QSet<QUuid> visited;
        QUuid current = iterator.key();
        while (!current.isNull())
        {
            if (visited.contains(current))
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("坐标系层级存在循环"));
            }
            visited.insert(current);
            if (!parents.contains(current))
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("坐标系引用了不存在的父坐标系"));
            }
            current = parents.value(current);
        }
    }
    return SResult<void>::success();
}

SResult<void> validateObjects(const std::vector<SSceneObject>& objects,
                              const std::vector<SCoordinateSystem>& coordinate_systems)
{
    QSet<QUuid> coordinate_ids;
    for (const SCoordinateSystem& coordinate_system : coordinate_systems)
    {
        coordinate_ids.insert(coordinate_system.id);
    }

    QHash<QUuid, QUuid> parents;
    QHash<QUuid, SObjectType> types;
    for (const SSceneObject& object : objects)
    {
        if (object.id.isNull() || object.name.trimmed().isEmpty() ||
            !isValidObjectType(object.type) || !isValidDataStage(object.stage) ||
            !isValidDisplayMode(object.display.mode) || !object.display.color.isValid() ||
            !std::isfinite(object.display.transparency) || object.display.transparency < 0.0 ||
            object.display.transparency > 1.0 ||
            !coordinate_ids.contains(object.coordinate_system_id) ||
            (!object.shape.isNull() && object.presentation_group_id.isNull()) ||
            !isSimilarityTransform(object.transform) || !isValidImportedAppearance(object))
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目包含无效对象元数据"));
        }
        if (parents.contains(object.id))
        {
            return SResult<void>::failure(SErrorCode::CorruptData,
                                          QObject::tr("项目包含重复对象 ID"));
        }
        parents.insert(object.id, object.parent_id);
        types.insert(object.id, object.type);
    }

    for (auto iterator = parents.cbegin(); iterator != parents.cend(); ++iterator)
    {
        QSet<QUuid> visited;
        QUuid current = iterator.key();
        while (!current.isNull())
        {
            if (visited.contains(current))
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("场景对象层级存在循环"));
            }
            visited.insert(current);
            if (!parents.contains(current))
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("场景对象引用了不存在的父对象"));
            }
            const QUuid parent = parents.value(current);
            if (!parent.isNull() && types.value(parent) != SObjectType::Group)
            {
                return SResult<void>::failure(SErrorCode::CorruptData,
                                              QObject::tr("场景对象父级不是场景组"));
            }
            current = parent;
        }
    }
    return SResult<void>::success();
}
} // namespace smartGraphics3D::projectValidation
