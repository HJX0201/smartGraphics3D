#pragma once

#include "s_kernel_shape.h"

#include <TopoDS_Shape.hxx>

namespace smartGraphics3D
{
class SKernelShapeAccess
{
  public:
    static SKernelShape fromNative(const TopoDS_Shape& shape);
    static const TopoDS_Shape& native(const SKernelShape& shape);
};
} // namespace smartGraphics3D
