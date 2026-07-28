#include "s_kernel_service.h"

#include "s_kernel_measurement_utils.h"
#include "s_kernel_shape_access.h"
#include "s_transform_utils.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QObject>
#include <QtMath>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace smartGraphics3D
{
namespace
{
SResult<SKernelShape> validateShape(const TopoDS_Shape& shape, const QString& operation)
{
    SKernelShape result = SKernelShapeAccess::fromNative(shape);
    if (result.isNull() || !result.isValid())
    {
        return SResult<SKernelShape>::failure(SErrorCode::GeometryFailure,
                                              QObject::tr("%1未生成有效实体").arg(operation),
                                              QObject::tr("请检查输入尺寸、相交关系或特征半径。"));
    }
    return SResult<SKernelShape>::success(std::move(result));
}

bool positive(double value)
{
    return std::isfinite(value) && value > 1.0e-9;
}

template <typename T> QVector3D toVector3D(const T& value)
{
    return QVector3D(static_cast<float>(value.X()), static_cast<float>(value.Y()),
                     static_cast<float>(value.Z()));
}

int countSubShapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum type)
{
    int count = 0;
    for (TopExp_Explorer explorer(shape, type); explorer.More(); explorer.Next())
    {
        ++count;
    }
    return count;
}

QString standardFailureMessage(const Standard_Failure& failure)
{
    return QString::fromLocal8Bit(failure.GetMessageString());
}

class SKernelService final : public SIKernelService
{
  public:
    SResult<SKernelShape> makeBox(const SBoxParameters& parameters) const override
    {
        if (!positive(parameters.length) || !positive(parameters.width) ||
            !positive(parameters.height))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("长方体尺寸必须大于零"));
        }
        try
        {
            return validateShape(
                BRepPrimAPI_MakeBox(parameters.length, parameters.width, parameters.height).Shape(),
                QObject::tr("创建长方体"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建长方体失败"), failure);
        }
    }

    SResult<SKernelShape> makeCompound(const QList<SKernelShape>& shapes) const override
    {
        if (shapes.isEmpty())
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("组合预览至少需要一个有效形状"));
        }
        try
        {
            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            for (const SKernelShape& shape : shapes)
            {
                if (shape.isNull())
                {
                    return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                          QObject::tr("组合预览包含空形状"));
                }
                builder.Add(compound, SKernelShapeAccess::native(shape));
            }
            return validateShape(compound, QObject::tr("创建组合预览"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建组合预览失败"), failure);
        }
    }

    SResult<SKernelShape> makeCylinder(const SCylinderParameters& parameters) const override
    {
        if (!positive(parameters.radius) || !positive(parameters.height))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("圆柱半径和高度必须大于零"));
        }
        try
        {
            return validateShape(
                BRepPrimAPI_MakeCylinder(parameters.radius, parameters.height).Shape(),
                QObject::tr("创建圆柱"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建圆柱失败"), failure);
        }
    }

    SResult<SKernelShape> makeCone(const SConeParameters& parameters) const override
    {
        if (!positive(parameters.bottom_radius) || parameters.top_radius < 0.0 ||
            !positive(parameters.height) ||
            std::abs(parameters.bottom_radius - parameters.top_radius) < 1.0e-9)
        {
            return SResult<SKernelShape>::failure(
                SErrorCode::InvalidArgument, QObject::tr("圆锥尺寸无效"),
                QObject::tr("底半径和高度必须大于零，两个半径不能相同。"));
        }
        try
        {
            return validateShape(BRepPrimAPI_MakeCone(parameters.bottom_radius,
                                                      parameters.top_radius, parameters.height)
                                     .Shape(),
                                 QObject::tr("创建圆锥"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建圆锥失败"), failure);
        }
    }

    SResult<SKernelShape> makeSphere(const SSphereParameters& parameters) const override
    {
        if (!positive(parameters.radius))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("球体半径必须大于零"));
        }
        try
        {
            return validateShape(BRepPrimAPI_MakeSphere(parameters.radius).Shape(),
                                 QObject::tr("创建球体"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建球体失败"), failure);
        }
    }

    SResult<SKernelShape> makeTorus(const STorusParameters& parameters) const override
    {
        if (!positive(parameters.major_radius) || !positive(parameters.minor_radius) ||
            parameters.minor_radius >= parameters.major_radius)
        {
            return SResult<SKernelShape>::failure(
                SErrorCode::InvalidArgument, QObject::tr("圆环体尺寸无效"),
                QObject::tr("主半径必须大于管半径，且二者都必须大于零。"));
        }
        try
        {
            return validateShape(
                BRepPrimAPI_MakeTorus(parameters.major_radius, parameters.minor_radius).Shape(),
                QObject::tr("创建圆环体"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建圆环体失败"), failure);
        }
    }

    SResult<SKernelShape> booleanOperation(const SKernelShape& first, const SKernelShape& second,
                                           SBooleanOperation operation) const override
    {
        if (first.isNull() || second.isNull())
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("布尔运算需要两个有效实体"));
        }
        try
        {
            const TopoDS_Shape& first_shape = SKernelShapeAccess::native(first);
            const TopoDS_Shape& second_shape = SKernelShapeAccess::native(second);
            switch (operation)
            {
            case SBooleanOperation::Union:
                return validateShape(BRepAlgoAPI_Fuse(first_shape, second_shape).Shape(),
                                     QObject::tr("布尔并集"));
            case SBooleanOperation::Difference:
                return validateShape(BRepAlgoAPI_Cut(first_shape, second_shape).Shape(),
                                     QObject::tr("布尔差集"));
            case SBooleanOperation::Intersection:
                return validateShape(BRepAlgoAPI_Common(first_shape, second_shape).Shape(),
                                     QObject::tr("布尔交集"));
            }
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("布尔运算失败"), failure);
        }
        return SResult<SKernelShape>::failure(SErrorCode::InternalFailure,
                                              QObject::tr("未知布尔运算"));
    }

    SResult<SKernelShape> filletAllEdges(const SKernelShape& input, double radius) const override
    {
        if (input.isNull() || !positive(radius))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("圆角输入或半径无效"));
        }
        try
        {
            BRepFilletAPI_MakeFillet builder(SKernelShapeAccess::native(input));
            for (TopExp_Explorer explorer(SKernelShapeAccess::native(input), TopAbs_EDGE);
                 explorer.More(); explorer.Next())
            {
                builder.Add(radius, TopoDS::Edge(explorer.Current()));
            }
            builder.Build();
            return validateShape(builder.Shape(), QObject::tr("全边圆角"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("圆角失败"), failure);
        }
    }

    SResult<SKernelShape> chamferAllEdges(const SKernelShape& input, double distance) const override
    {
        if (input.isNull() || !positive(distance))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("倒角输入或距离无效"));
        }
        try
        {
            const TopoDS_Shape& shape = SKernelShapeAccess::native(input);
            BRepFilletAPI_MakeChamfer builder(shape);
            for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next())
            {
                builder.Add(distance, TopoDS::Edge(explorer.Current()));
            }
            builder.Build();
            return validateShape(builder.Shape(), QObject::tr("全边倒角"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("倒角失败"), failure);
        }
    }

    SResult<SKernelShape> makeHole(const SKernelShape& input,
                                   const SHoleParameters& parameters) const override
    {
        if (input.isNull() || !positive(parameters.diameter) ||
            (!parameters.through_all && !positive(parameters.depth)))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("孔参数无效"));
        }
        const auto metrics = measure(input);
        if (!metrics)
        {
            return SResult<SKernelShape>::failure(metrics.errorCode(), metrics.message(),
                                                  metrics.details());
        }
        try
        {
            const SShapeMetrics& bounds = metrics.value();
            const double margin =
                std::max(1.0, static_cast<double>(bounds.maximum.z() - bounds.minimum.z()));
            TopoDS_Shape cutter;
            if (parameters.through_all)
            {
                cutter = BRepPrimAPI_MakeCylinder(
                             gp_Ax2(gp_Pnt(parameters.x, parameters.y, bounds.minimum.z() - margin),
                                    gp_Dir(0.0, 0.0, 1.0)),
                             parameters.diameter * 0.5,
                             bounds.maximum.z() - bounds.minimum.z() + 2.0 * margin)
                             .Shape();
            }
            else
            {
                cutter = BRepPrimAPI_MakeCylinder(
                             gp_Ax2(gp_Pnt(parameters.x, parameters.y, bounds.maximum.z()),
                                    gp_Dir(0.0, 0.0, -1.0)),
                             parameters.diameter * 0.5, parameters.depth)
                             .Shape();
            }
            return validateShape(BRepAlgoAPI_Cut(SKernelShapeAccess::native(input), cutter).Shape(),
                                 QObject::tr("创建孔"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("创建孔失败"), failure);
        }
    }

    SResult<SKernelShape> transform(const SKernelShape& input,
                                    const STransformParameters& parameters) const override
    {
        if (input.isNull() || !positive(parameters.uniform_scale) ||
            parameters.rotation_axis.lengthSquared() < 1.0e-12F)
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("变换参数无效"));
        }
        try
        {
            gp_Trsf translation;
            translation.SetTranslation(gp_Vec(parameters.translation.x(),
                                              parameters.translation.y(),
                                              parameters.translation.z()));
            gp_Trsf rotation;
            rotation.SetRotation(
                gp_Ax1(gp_Pnt(0.0, 0.0, 0.0),
                       gp_Dir(parameters.rotation_axis.x(), parameters.rotation_axis.y(),
                              parameters.rotation_axis.z())),
                qDegreesToRadians(parameters.rotation_degrees));
            gp_Trsf scale;
            scale.SetScale(gp_Pnt(0.0, 0.0, 0.0), parameters.uniform_scale);
            gp_Trsf combined = translation;
            combined.Multiply(rotation);
            combined.Multiply(scale);
            return validateShape(
                BRepBuilderAPI_Transform(SKernelShapeAccess::native(input), combined, true).Shape(),
                QObject::tr("实体变换"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("变换失败"), failure);
        }
    }

    SResult<SKernelShape> materialize(const SKernelShape& input,
                                      const QMatrix4x4& matrix) const override
    {
        if (input.isNull() || !isSimilarityTransform(matrix))
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("场景变换矩阵无效"));
        }
        if (matrix.isIdentity())
        {
            return SResult<SKernelShape>::success(input);
        }
        try
        {
            gp_Trsf transform;
            transform.SetValues(matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(0, 3),
                                matrix(1, 0), matrix(1, 1), matrix(1, 2), matrix(1, 3),
                                matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3));
            return validateShape(
                BRepBuilderAPI_Transform(SKernelShapeAccess::native(input), transform, true)
                    .Shape(),
                QObject::tr("物化场景几何"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("物化场景几何失败"), failure);
        }
    }

    SResult<SKernelShape> mirror(const SKernelShape& input,
                                 const QVector3D& plane_normal) const override
    {
        if (input.isNull() || plane_normal.lengthSquared() < 1.0e-12F)
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("镜像平面无效"));
        }
        try
        {
            gp_Trsf transform;
            transform.SetMirror(
                gp_Ax2(gp_Pnt(0.0, 0.0, 0.0),
                       gp_Dir(plane_normal.x(), plane_normal.y(), plane_normal.z())));
            return validateShape(
                BRepBuilderAPI_Transform(SKernelShapeAccess::native(input), transform, true)
                    .Shape(),
                QObject::tr("实体镜像"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("镜像失败"), failure);
        }
    }

    SResult<SKernelShape> section(const SKernelShape& input,
                                  const SSectionParameters& parameters) const override
    {
        if (input.isNull() || parameters.normal.lengthSquared() < 1.0e-12F)
        {
            return SResult<SKernelShape>::failure(SErrorCode::InvalidArgument,
                                                  QObject::tr("剖切平面无效"));
        }
        try
        {
            const gp_Pln plane(
                gp_Pnt(parameters.origin.x(), parameters.origin.y(), parameters.origin.z()),
                gp_Dir(parameters.normal.x(), parameters.normal.y(), parameters.normal.z()));
            BRepAlgoAPI_Section builder(SKernelShapeAccess::native(input), plane, false);
            builder.Approximation(true);
            builder.Build();
            if (!builder.IsDone() || builder.Shape().IsNull())
            {
                return SResult<SKernelShape>::failure(
                    SErrorCode::GeometryFailure, QObject::tr("剖切未产生交线"),
                    QObject::tr("请检查剖切平面是否与实体相交。"));
            }
            return validateShape(builder.Shape(), QObject::tr("创建剖切"));
        }
        catch (const Standard_Failure& failure)
        {
            return geometryFailure(QObject::tr("剖切失败"), failure);
        }
    }

    SResult<SShapeMetrics> measure(const SKernelShape& input) const override
    {
        if (input.isNull())
        {
            return SResult<SShapeMetrics>::failure(SErrorCode::InvalidArgument,
                                                   QObject::tr("无法测量空实体"));
        }
        try
        {
            const TopoDS_Shape& shape = SKernelShapeAccess::native(input);
            Bnd_Box bounds;
            BRepBndLib::Add(shape, bounds);
            double x_min = 0.0;
            double y_min = 0.0;
            double z_min = 0.0;
            double x_max = 0.0;
            double y_max = 0.0;
            double z_max = 0.0;
            bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);

            GProp_GProps surface_properties;
            GProp_GProps volume_properties;
            BRepGProp::SurfaceProperties(shape, surface_properties);
            BRepGProp::VolumeProperties(shape, volume_properties);

            SShapeMetrics metrics;
            metrics.minimum = QVector3D(static_cast<float>(x_min), static_cast<float>(y_min),
                                        static_cast<float>(z_min));
            metrics.maximum = QVector3D(static_cast<float>(x_max), static_cast<float>(y_max),
                                        static_cast<float>(z_max));
            const gp_Pnt center = volume_properties.Mass() > 1.0e-12
                                      ? volume_properties.CentreOfMass()
                                      : surface_properties.CentreOfMass();
            metrics.center_of_mass = toVector3D(center);
            metrics.surface_area = surface_properties.Mass();
            metrics.volume = volume_properties.Mass();
            metrics.solid_count = countSubShapes(shape, TopAbs_SOLID);
            metrics.face_count = countSubShapes(shape, TopAbs_FACE);
            metrics.edge_count = countSubShapes(shape, TopAbs_EDGE);
            metrics.vertex_count = countSubShapes(shape, TopAbs_VERTEX);
            return SResult<SShapeMetrics>::success(metrics);
        }
        catch (const Standard_Failure& failure)
        {
            return SResult<SShapeMetrics>::failure(SErrorCode::GeometryFailure,
                                                   QObject::tr("测量失败"),
                                                   standardFailureMessage(failure));
        }
    }

    SResult<SSubShapeMetrics> measureSubShape(const SKernelShape& input, SSelectionMode type,
                                              int one_based_index) const override
    {
        const auto selected = kernelMeasurement::selectedShape(input, type, one_based_index);
        if (!selected)
        {
            return SResult<SSubShapeMetrics>::failure(selected.errorCode(), selected.message(),
                                                      selected.details());
        }
        try
        {
            const TopoDS_Shape& shape = selected.value();
            SSubShapeMetrics metrics;
            metrics.type = type;
            if (shape.ShapeType() == TopAbs_VERTEX)
            {
                const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(shape));
                metrics.point = toVector3D(point);
                metrics.center = metrics.point;
                metrics.has_point = true;
            }
            else if (shape.ShapeType() == TopAbs_EDGE)
            {
                GProp_GProps properties;
                BRepGProp::LinearProperties(shape, properties);
                metrics.length = properties.Mass();
                const gp_Pnt center = properties.CentreOfMass();
                metrics.center = toVector3D(center);
                const BRepAdaptor_Curve curve(TopoDS::Edge(shape));
                if (curve.GetType() == GeomAbs_Circle)
                {
                    metrics.radius = curve.Circle().Radius();
                    metrics.has_radius = true;
                    const gp_Dir axis = curve.Circle().Axis().Direction();
                    metrics.direction = toVector3D(axis);
                    metrics.has_direction = true;
                }
                else if (curve.GetType() == GeomAbs_Line)
                {
                    const gp_Dir direction = curve.Line().Direction();
                    metrics.direction = toVector3D(direction);
                    metrics.has_direction = true;
                }
            }
            else if (shape.ShapeType() == TopAbs_FACE)
            {
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(shape, properties);
                metrics.area = properties.Mass();
                GProp_GProps boundary_properties;
                BRepGProp::LinearProperties(shape, boundary_properties);
                metrics.length = boundary_properties.Mass();
                const gp_Pnt center = properties.CentreOfMass();
                metrics.center = toVector3D(center);
                const BRepAdaptor_Surface surface(TopoDS::Face(shape));
                if (surface.GetType() == GeomAbs_Plane)
                {
                    const gp_Dir normal = surface.Plane().Axis().Direction();
                    metrics.direction = toVector3D(normal);
                    metrics.has_direction = true;
                }
                else if (surface.GetType() == GeomAbs_Cylinder)
                {
                    metrics.radius = surface.Cylinder().Radius();
                    metrics.has_radius = true;
                    const gp_Dir axis = surface.Cylinder().Axis().Direction();
                    metrics.direction = toVector3D(axis);
                    metrics.has_direction = true;
                }
                else if (surface.GetType() == GeomAbs_Sphere)
                {
                    metrics.radius = surface.Sphere().Radius();
                    metrics.has_radius = true;
                }
            }
            return SResult<SSubShapeMetrics>::success(metrics);
        }
        catch (const Standard_Failure& failure)
        {
            return SResult<SSubShapeMetrics>::failure(SErrorCode::GeometryFailure,
                                                      QObject::tr("子形状测量失败"),
                                                      standardFailureMessage(failure));
        }
    }

    SResult<double> distanceBetween(const SKernelShape& first, SSelectionMode first_type,
                                    int first_index, const SKernelShape& second,
                                    SSelectionMode second_type, int second_index) const override
    {
        const auto first_shape = kernelMeasurement::selectedShape(first, first_type, first_index);
        const auto second_shape =
            kernelMeasurement::selectedShape(second, second_type, second_index);
        if (!first_shape || !second_shape)
        {
            const auto& failure = !first_shape ? first_shape : second_shape;
            return SResult<double>::failure(failure.errorCode(), failure.message(),
                                            failure.details());
        }
        try
        {
            BRepExtrema_DistShapeShape distance(first_shape.value(), second_shape.value());
            distance.Perform();
            if (!distance.IsDone())
            {
                return SResult<double>::failure(SErrorCode::GeometryFailure,
                                                QObject::tr("距离计算未完成"));
            }
            return SResult<double>::success(distance.Value());
        }
        catch (const Standard_Failure& failure)
        {
            return SResult<double>::failure(SErrorCode::GeometryFailure,
                                            QObject::tr("距离测量失败"),
                                            standardFailureMessage(failure));
        }
    }

    SResult<double> angleBetween(const SKernelShape& first, SSelectionMode first_type,
                                 int first_index, const SKernelShape& second,
                                 SSelectionMode second_type, int second_index) const override
    {
        const auto first_shape = kernelMeasurement::selectedShape(first, first_type, first_index);
        const auto second_shape =
            kernelMeasurement::selectedShape(second, second_type, second_index);
        if (!first_shape || !second_shape)
        {
            const auto& failure = !first_shape ? first_shape : second_shape;
            return SResult<double>::failure(failure.errorCode(), failure.message(),
                                            failure.details());
        }
        const auto first_direction = kernelMeasurement::shapeDirection(first_shape.value());
        const auto second_direction = kernelMeasurement::shapeDirection(second_shape.value());
        if (!first_direction || !second_direction)
        {
            const auto& failure = !first_direction ? first_direction : second_direction;
            return SResult<double>::failure(failure.errorCode(), failure.message(),
                                            failure.details());
        }
        const double dot = qBound(
            -1.0,
            static_cast<double>(QVector3D::dotProduct(first_direction.value().normalized(),
                                                      second_direction.value().normalized())),
            1.0);
        return SResult<double>::success(qRadiansToDegrees(std::acos(dot)));
    }

  private:
    static SResult<SKernelShape> geometryFailure(const QString& message,
                                                 const Standard_Failure& failure)
    {
        return SResult<SKernelShape>::failure(SErrorCode::GeometryFailure, message,
                                              standardFailureMessage(failure));
    }
};
} // namespace

std::unique_ptr<SIKernelService> createKernelService()
{
    return std::make_unique<SKernelService>();
}
} // namespace smartGraphics3D
