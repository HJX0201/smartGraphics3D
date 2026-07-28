#pragma once

#include "s_result.h"
#include "s_types.h"

#include <QMatrix4x4>
#include <vector>

namespace smartGraphics3D
{
class SCoordinateSystemService
{
  public:
    static SResult<QMatrix4x4> transform(const std::vector<SCoordinateSystem>& coordinate_systems,
                                         const QUuid& source_id, const QUuid& target_id);
    static QString directionLabel(const std::vector<SCoordinateSystem>& coordinate_systems,
                                  const QUuid& source_id, const QUuid& target_id);
};
} // namespace smartGraphics3D
