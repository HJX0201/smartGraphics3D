#include "s_kernel_shape.h"

#include "s_kernel_shape_access.h"

#include <BRepCheck_Analyzer.hxx>
#include <TopoDS_Shape.hxx>

namespace smartGraphics3D
{
struct SKernelShape::SImpl
{
    TopoDS_Shape shape;
};

SKernelShape::SKernelShape() : m_impl(std::make_shared<SImpl>())
{
}

SKernelShape::SKernelShape(std::shared_ptr<SImpl> impl) : m_impl(std::move(impl))
{
}

SKernelShape::SKernelShape(const SKernelShape&) = default;
SKernelShape::SKernelShape(SKernelShape&&) noexcept = default;
SKernelShape& SKernelShape::operator=(const SKernelShape&) = default;
SKernelShape& SKernelShape::operator=(SKernelShape&&) noexcept = default;
SKernelShape::~SKernelShape() = default;

bool SKernelShape::isNull() const
{
    return !m_impl || m_impl->shape.IsNull();
}

bool SKernelShape::isValid() const
{
    return !isNull() && BRepCheck_Analyzer(m_impl->shape).IsValid();
}

SKernelShape SKernelShapeAccess::fromNative(const TopoDS_Shape& shape)
{
    auto impl = std::make_shared<SKernelShape::SImpl>();
    impl->shape = shape;
    return SKernelShape(std::move(impl));
}

const TopoDS_Shape& SKernelShapeAccess::native(const SKernelShape& shape)
{
    static const TopoDS_Shape kNullShape;
    return shape.m_impl ? shape.m_impl->shape : kNullShape;
}
} // namespace smartGraphics3D
