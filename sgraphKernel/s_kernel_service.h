#pragma once

#include "s_kernel_shape.h"
#include "s_result.h"
#include "s_types.h"

#include <QVector3D>
#include <memory>

namespace smartGraphics3D
{
enum class SBooleanOperation
{
    Union,
    Difference,
    Intersection
};

struct SBoxParameters
{
    double length = 100.0;
    double width = 80.0;
    double height = 60.0;
};

struct SCylinderParameters
{
    double radius = 30.0;
    double height = 80.0;
};

struct SConeParameters
{
    double bottom_radius = 35.0;
    double top_radius = 10.0;
    double height = 80.0;
};

struct SSphereParameters
{
    double radius = 40.0;
};

struct STorusParameters
{
    double major_radius = 45.0;
    double minor_radius = 12.0;
};

struct STransformParameters
{
    QVector3D translation;
    QVector3D rotation_axis = QVector3D(0.0F, 0.0F, 1.0F);
    double rotation_degrees = 0.0;
    double uniform_scale = 1.0;
};

struct SHoleParameters
{
    double x = 0.0;
    double y = 0.0;
    double diameter = 10.0;
    double depth = 0.0;
    bool through_all = true;
};

struct SSectionParameters
{
    QVector3D origin;
    QVector3D normal = QVector3D(0.0F, 0.0F, 1.0F);
};

struct SShapeMetrics
{
    QVector3D minimum;
    QVector3D maximum;
    QVector3D center_of_mass;
    double surface_area = 0.0;
    double volume = 0.0;
    int solid_count = 0;
    int face_count = 0;
    int edge_count = 0;
    int vertex_count = 0;
};

struct SSubShapeMetrics
{
    SSelectionMode type = SSelectionMode::Object;
    QVector3D point;
    QVector3D center;
    QVector3D direction;
    double length = 0.0;
    double area = 0.0;
    double radius = 0.0;
    bool has_point = false;
    bool has_direction = false;
    bool has_radius = false;
};

class SIKernelService
{
  public:
    virtual ~SIKernelService() = default;

    virtual SResult<SKernelShape> makeBox(const SBoxParameters& parameters) const = 0;
    virtual SResult<SKernelShape> makeCylinder(const SCylinderParameters& parameters) const = 0;
    virtual SResult<SKernelShape> makeCone(const SConeParameters& parameters) const = 0;
    virtual SResult<SKernelShape> makeSphere(const SSphereParameters& parameters) const = 0;
    virtual SResult<SKernelShape> makeTorus(const STorusParameters& parameters) const = 0;
    virtual SResult<SKernelShape> makeCompound(const QList<SKernelShape>& shapes) const = 0;
    virtual SResult<SKernelShape> booleanOperation(const SKernelShape& first,
                                                   const SKernelShape& second,
                                                   SBooleanOperation operation) const = 0;
    virtual SResult<SKernelShape> filletAllEdges(const SKernelShape& input,
                                                 double radius) const = 0;
    virtual SResult<SKernelShape> chamferAllEdges(const SKernelShape& input,
                                                  double distance) const = 0;
    virtual SResult<SKernelShape> makeHole(const SKernelShape& input,
                                           const SHoleParameters& parameters) const = 0;
    virtual SResult<SKernelShape> transform(const SKernelShape& input,
                                            const STransformParameters& parameters) const = 0;
    virtual SResult<SKernelShape> materialize(const SKernelShape& input,
                                              const QMatrix4x4& transform) const = 0;
    virtual SResult<SKernelShape> mirror(const SKernelShape& input,
                                         const QVector3D& plane_normal) const = 0;
    virtual SResult<SKernelShape> section(const SKernelShape& input,
                                          const SSectionParameters& parameters) const = 0;
    virtual SResult<SShapeMetrics> measure(const SKernelShape& input) const = 0;
    virtual SResult<SSubShapeMetrics>
    measureSubShape(const SKernelShape& input, SSelectionMode type, int one_based_index) const = 0;
    virtual SResult<double> distanceBetween(const SKernelShape& first, SSelectionMode first_type,
                                            int first_index, const SKernelShape& second,
                                            SSelectionMode second_type, int second_index) const = 0;
    virtual SResult<double> angleBetween(const SKernelShape& first, SSelectionMode first_type,
                                         int first_index, const SKernelShape& second,
                                         SSelectionMode second_type, int second_index) const = 0;
};

std::unique_ptr<SIKernelService> createKernelService();
} // namespace smartGraphics3D
