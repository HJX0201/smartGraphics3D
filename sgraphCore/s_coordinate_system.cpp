#include "s_coordinate_system.h"

#include <QSet>
#include <algorithm>

namespace smartGraphics3D
{
namespace
{
const SCoordinateSystem*
findCoordinateSystem(const std::vector<SCoordinateSystem>& coordinate_systems, const QUuid& id)
{
    const auto iterator = std::find_if(coordinate_systems.cbegin(), coordinate_systems.cend(),
                                       [&id](const SCoordinateSystem& coordinate_system)
                                       {
                                           return coordinate_system.id == id;
                                       });
    return iterator == coordinate_systems.cend() ? nullptr : &*iterator;
}

SResult<QMatrix4x4> transformToRoot(const std::vector<SCoordinateSystem>& coordinate_systems,
                                    const QUuid& id)
{
    QMatrix4x4 result;
    QUuid current_id = id;
    QSet<QUuid> visited;
    while (!current_id.isNull())
    {
        if (visited.contains(current_id))
        {
            return SResult<QMatrix4x4>::failure(SErrorCode::CorruptData,
                                                QObject::tr("坐标系层级存在循环"));
        }
        visited.insert(current_id);
        const SCoordinateSystem* current = findCoordinateSystem(coordinate_systems, current_id);
        if (!current)
        {
            return SResult<QMatrix4x4>::failure(SErrorCode::NotFound, QObject::tr("坐标系不存在"));
        }
        if (!current->valid)
        {
            return SResult<QMatrix4x4>::failure(SErrorCode::InvalidArgument,
                                                QObject::tr("坐标系“%1”无效").arg(current->name));
        }
        result = current->transform_to_parent * result;
        current_id = current->parent_id;
    }
    return SResult<QMatrix4x4>::success(result);
}
} // namespace

SResult<QMatrix4x4>
SCoordinateSystemService::transform(const std::vector<SCoordinateSystem>& coordinate_systems,
                                    const QUuid& source_id, const QUuid& target_id)
{
    const auto source_to_root = transformToRoot(coordinate_systems, source_id);
    const auto target_to_root = transformToRoot(coordinate_systems, target_id);
    if (!source_to_root || !target_to_root)
    {
        const auto& failure = !source_to_root ? source_to_root : target_to_root;
        return SResult<QMatrix4x4>::failure(failure.errorCode(), failure.message(),
                                            failure.details());
    }
    bool invertible = false;
    const QMatrix4x4 root_to_target = target_to_root.value().inverted(&invertible);
    if (!invertible)
    {
        return SResult<QMatrix4x4>::failure(SErrorCode::InvalidArgument,
                                            QObject::tr("目标坐标系变换不可逆"));
    }
    return SResult<QMatrix4x4>::success(root_to_target * source_to_root.value());
}

QString
SCoordinateSystemService::directionLabel(const std::vector<SCoordinateSystem>& coordinate_systems,
                                         const QUuid& source_id, const QUuid& target_id)
{
    const SCoordinateSystem* source = findCoordinateSystem(coordinate_systems, source_id);
    const SCoordinateSystem* target = findCoordinateSystem(coordinate_systems, target_id);
    return QStringLiteral("%1 → %2").arg(source ? source->name : QObject::tr("未知坐标系"),
                                         target ? target->name : QObject::tr("未知坐标系"));
}
} // namespace smartGraphics3D
