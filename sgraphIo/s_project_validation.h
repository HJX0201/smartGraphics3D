#pragma once

#include "s_result.h"
#include "s_scene_object.h"

#include <vector>

namespace smartGraphics3D::projectValidation
{
bool isValidLengthUnit(int value);
bool isValidAngleUnit(int value);
SResult<void> validateCoordinateSystems(const std::vector<SCoordinateSystem>& coordinate_systems);
SResult<void> validateObjects(const std::vector<SSceneObject>& objects,
                              const std::vector<SCoordinateSystem>& coordinate_systems);
} // namespace smartGraphics3D::projectValidation
