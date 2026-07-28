#pragma once

#include "s_kernel_service.h"

#include <TopoDS_Shape.hxx>

namespace smartGraphics3D::kernelMeasurement
{
SResult<TopoDS_Shape> selectedShape(const SKernelShape& input, SSelectionMode type,
                                    int one_based_index);
SResult<QVector3D> shapeDirection(const TopoDS_Shape& shape);
} // namespace smartGraphics3D::kernelMeasurement
