#include "s_kernel_measurement_utils.h"

#include "s_kernel_shape_access.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <QObject>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>

namespace smartGraphics3D::kernelMeasurement
{
namespace
{
template <typename T> QVector3D toVector3D(const T& value)
{
    return QVector3D(static_cast<float>(value.X()), static_cast<float>(value.Y()),
                     static_cast<float>(value.Z()));
}

TopAbs_ShapeEnum occShapeType(SSelectionMode type)
{
    switch (type)
    {
    case SSelectionMode::Solid:
        return TopAbs_SOLID;
    case SSelectionMode::Face:
        return TopAbs_FACE;
    case SSelectionMode::Edge:
        return TopAbs_EDGE;
    case SSelectionMode::Vertex:
        return TopAbs_VERTEX;
    case SSelectionMode::Object:
        return TopAbs_SHAPE;
    }
    return TopAbs_SHAPE;
}
} // namespace

SResult<TopoDS_Shape> selectedShape(const SKernelShape& input, SSelectionMode type,
                                    int one_based_index)
{
    if (input.isNull())
    {
        return SResult<TopoDS_Shape>::failure(SErrorCode::InvalidArgument,
                                              QObject::tr("测量对象为空"));
    }
    const TopoDS_Shape& shape = SKernelShapeAccess::native(input);
    if (type == SSelectionMode::Object)
    {
        return SResult<TopoDS_Shape>::success(shape);
    }
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(shape, occShapeType(type), map);
    if (one_based_index < 1 || one_based_index > map.Extent())
    {
        return SResult<TopoDS_Shape>::failure(
            SErrorCode::NotFound, QObject::tr("选择的子形状已失效"),
            QObject::tr("当前索引为 %1，可用范围为 1–%2。").arg(one_based_index).arg(map.Extent()));
    }
    return SResult<TopoDS_Shape>::success(map(one_based_index));
}

SResult<QVector3D> shapeDirection(const TopoDS_Shape& shape)
{
    if (shape.ShapeType() == TopAbs_EDGE)
    {
        const BRepAdaptor_Curve curve(TopoDS::Edge(shape));
        if (curve.GetType() != GeomAbs_Line)
        {
            return SResult<QVector3D>::failure(SErrorCode::Unsupported,
                                               QObject::tr("只有直线边可用于角度测量"));
        }
        const gp_Dir direction = curve.Line().Direction();
        return SResult<QVector3D>::success(toVector3D(direction));
    }
    if (shape.ShapeType() == TopAbs_FACE)
    {
        const BRepAdaptor_Surface surface(TopoDS::Face(shape));
        if (surface.GetType() != GeomAbs_Plane)
        {
            return SResult<QVector3D>::failure(SErrorCode::Unsupported,
                                               QObject::tr("只有平面可用于角度测量"));
        }
        const gp_Dir direction = surface.Plane().Axis().Direction();
        return SResult<QVector3D>::success(toVector3D(direction));
    }
    return SResult<QVector3D>::failure(SErrorCode::Unsupported,
                                       QObject::tr("角度测量需要两条直线边或两个平面"));
}
} // namespace smartGraphics3D::kernelMeasurement
