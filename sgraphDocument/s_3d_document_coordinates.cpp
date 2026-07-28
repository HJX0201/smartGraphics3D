#include "s_3d_document.h"

#include <algorithm>

namespace smartGraphics3D
{
const SUnitSystem& S3dDocument::unitSystem() const
{
    return m_units;
}

SResult<void> S3dDocument::setUnits(SLengthUnit length_unit, SAngleUnit angle_unit)
{
    if (m_units.lengthUnit() == length_unit && m_units.angleUnit() == angle_unit)
    {
        return SResult<void>::success();
    }
    return commit(
        tr("修改项目单位"),
        [this, length_unit, angle_unit]()
        {
            m_units.setLengthUnit(length_unit);
            m_units.setAngleUnit(angle_unit);
            return SResult<void>::success();
        },
        {},
        tr("长度=%1; 角度=%2")
            .arg(static_cast<int>(length_unit))
            .arg(static_cast<int>(angle_unit)));
}

SResult<void> S3dDocument::setLengthUnit(SLengthUnit unit)
{
    if (m_units.lengthUnit() == unit)
    {
        return SResult<void>::success();
    }
    return commit(tr("修改长度单位"),
                  [this, unit]()
                  {
                      m_units.setLengthUnit(unit);
                      return SResult<void>::success();
                  });
}

SResult<void> S3dDocument::setAngleUnit(SAngleUnit unit)
{
    if (m_units.angleUnit() == unit)
    {
        return SResult<void>::success();
    }
    return commit(tr("修改角度单位"),
                  [this, unit]()
                  {
                      m_units.setAngleUnit(unit);
                      return SResult<void>::success();
                  });
}

const std::vector<SCoordinateSystem>& S3dDocument::coordinateSystems() const
{
    return m_coordinate_systems;
}

SResult<QUuid> S3dDocument::addCoordinateSystem(SCoordinateSystem coordinate_system)
{
    coordinate_system.name = coordinate_system.name.trimmed();
    if (coordinate_system.name.isEmpty())
    {
        return SResult<QUuid>::failure(SErrorCode::InvalidArgument, tr("坐标系名称不能为空"));
    }
    if (coordinate_system.id.isNull())
    {
        return SResult<QUuid>::failure(SErrorCode::InvalidArgument, tr("坐标系 ID 不能为空"));
    }
    const auto duplicate = std::find_if(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                                        [&coordinate_system](const SCoordinateSystem& candidate)
                                        {
                                            return candidate.id == coordinate_system.id;
                                        });
    if (duplicate != m_coordinate_systems.cend())
    {
        return SResult<QUuid>::failure(SErrorCode::Conflict, tr("坐标系 ID 已存在"));
    }
    if (coordinate_system.parent_id == coordinate_system.id)
    {
        return SResult<QUuid>::failure(SErrorCode::Conflict, tr("坐标系不能以自身为父坐标系"));
    }
    if (coordinate_system.parent_id.isNull() && !m_coordinate_systems.empty())
    {
        return SResult<QUuid>::failure(SErrorCode::InvalidArgument,
                                       tr("新增坐标系必须指定父坐标系"));
    }
    if (!coordinate_system.parent_id.isNull())
    {
        const auto parent = std::find_if(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                                         [&coordinate_system](const SCoordinateSystem& candidate)
                                         {
                                             return candidate.id == coordinate_system.parent_id;
                                         });
        if (parent == m_coordinate_systems.cend())
        {
            return SResult<QUuid>::failure(SErrorCode::NotFound, tr("父坐标系不存在"));
        }
    }
    const QUuid id = coordinate_system.id;
    const auto result = commit(tr("创建坐标系"),
                               [this, coordinate_system = std::move(coordinate_system)]() mutable
                               {
                                   m_coordinate_systems.push_back(std::move(coordinate_system));
                                   return SResult<void>::success();
                               });
    if (!result)
    {
        return SResult<QUuid>::failure(result.errorCode(), result.message(), result.details());
    }
    return SResult<QUuid>::success(id);
}

SResult<void> S3dDocument::removeCoordinateSystem(const QUuid& id)
{
    const auto iterator = std::find_if(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                                       [&id](const SCoordinateSystem& coordinate_system)
                                       {
                                           return coordinate_system.id == id;
                                       });
    if (iterator == m_coordinate_systems.cend())
    {
        return SResult<void>::failure(SErrorCode::NotFound, tr("坐标系不存在"));
    }
    if (iterator->parent_id.isNull())
    {
        return SResult<void>::failure(SErrorCode::Locked, tr("世界坐标系不能删除"));
    }
    for (const SSceneObject& object : m_objects)
    {
        if (object.coordinate_system_id == id)
        {
            return SResult<void>::failure(SErrorCode::Conflict, tr("仍有对象使用该坐标系"));
        }
    }
    for (const SCoordinateSystem& coordinate_system : m_coordinate_systems)
    {
        if (coordinate_system.parent_id == id)
        {
            return SResult<void>::failure(SErrorCode::Conflict, tr("仍有子坐标系依赖该坐标系"));
        }
    }
    return commit(tr("删除坐标系"),
                  [this, id]()
                  {
                      m_coordinate_systems.erase(
                          std::remove_if(m_coordinate_systems.begin(), m_coordinate_systems.end(),
                                         [&id](const SCoordinateSystem& coordinate_system)
                                         {
                                             return coordinate_system.id == id;
                                         }),
                          m_coordinate_systems.end());
                      return SResult<void>::success();
                  });
}
} // namespace smartGraphics3D
