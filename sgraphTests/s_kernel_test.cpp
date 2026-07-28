#include "s_kernel_service.h"

#include <QtTest>
#include <cmath>

namespace smartGraphics3D
{
class SKernelTest final : public QObject
{
    Q_OBJECT

  private slots:
    void rejectsInvalidPrimitive()
    {
        const auto kernel = createKernelService();
        SBoxParameters parameters;
        parameters.length = 0.0;
        const auto result = kernel->makeBox(parameters);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::InvalidArgument);
    }

    void createsAndMeasuresBox()
    {
        const auto kernel = createKernelService();
        SBoxParameters parameters;
        parameters.length = 10.0;
        parameters.width = 20.0;
        parameters.height = 30.0;
        const auto shape = kernel->makeBox(parameters);
        QVERIFY(shape);
        QVERIFY(shape.value().isValid());

        const auto metrics = kernel->measure(shape.value());
        QVERIFY(metrics);
        QVERIFY(std::abs(metrics.value().volume - 6000.0) < 1.0e-6);
        QVERIFY(std::abs(metrics.value().surface_area - 2200.0) < 1.0e-6);
        QCOMPARE(metrics.value().solid_count, 1);
        QCOMPARE(metrics.value().face_count, 6);
    }

    void booleanUnionProducesValidShape()
    {
        const auto kernel = createKernelService();
        const auto first = kernel->makeBox({10.0, 10.0, 10.0});
        const auto second_source = kernel->makeBox({10.0, 10.0, 10.0});
        QVERIFY(first);
        QVERIFY(second_source);

        STransformParameters transform;
        transform.translation = QVector3D(5.0F, 0.0F, 0.0F);
        const auto second = kernel->transform(second_source.value(), transform);
        QVERIFY(second);
        const auto result =
            kernel->booleanOperation(first.value(), second.value(), SBooleanOperation::Union);
        QVERIFY(result);
        const auto metrics = kernel->measure(result.value());
        QVERIFY(metrics);
        QVERIFY(std::abs(metrics.value().volume - 1500.0) < 1.0e-5);
    }

    void transformsWithoutChangingSource()
    {
        const auto kernel = createKernelService();
        const auto source = kernel->makeSphere({10.0});
        QVERIFY(source);
        STransformParameters transform;
        transform.translation = QVector3D(25.0F, -5.0F, 2.0F);
        const auto moved = kernel->transform(source.value(), transform);
        QVERIFY(moved);

        const auto original_metrics = kernel->measure(source.value());
        const auto moved_metrics = kernel->measure(moved.value());
        QVERIFY(original_metrics);
        QVERIFY(moved_metrics);
        QVERIFY(std::abs(moved_metrics.value().center_of_mass.x() -
                         original_metrics.value().center_of_mass.x() - 25.0) < 1.0e-5);
    }

    void createsAllPrimitiveTypes()
    {
        const auto kernel = createKernelService();
        QVERIFY(kernel->makeCylinder({8.0, 20.0}));
        QVERIFY(kernel->makeCone({10.0, 4.0, 25.0}));
        QVERIFY(kernel->makeSphere({12.0}));
        QVERIFY(kernel->makeTorus({20.0, 5.0}));

        QVERIFY(!kernel->makeCylinder({-1.0, 20.0}));
        QVERIFY(!kernel->makeCone({2.0, 4.0, 0.0}));
        QVERIFY(!kernel->makeSphere({0.0}));
        QVERIFY(!kernel->makeTorus({5.0, 8.0}));
    }

    void appliesBooleanEdgeAndHoleOperations()
    {
        const auto kernel = createKernelService();
        const auto source = kernel->makeBox({40.0, 30.0, 20.0});
        const auto tool = kernel->makeCylinder({6.0, 30.0});
        QVERIFY(source);
        QVERIFY(tool);

        STransformParameters move;
        move.translation = QVector3D(20.0F, 15.0F, -5.0F);
        const auto moved_tool = kernel->transform(tool.value(), move);
        QVERIFY(moved_tool);
        QVERIFY(kernel->booleanOperation(source.value(), moved_tool.value(),
                                         SBooleanOperation::Difference));
        QVERIFY(kernel->booleanOperation(source.value(), moved_tool.value(),
                                         SBooleanOperation::Intersection));
        QVERIFY(kernel->filletAllEdges(source.value(), 1.0));
        QVERIFY(kernel->chamferAllEdges(source.value(), 1.0));

        SHoleParameters hole;
        hole.x = 20.0;
        hole.y = 15.0;
        hole.diameter = 6.0;
        hole.through_all = true;
        const auto holed = kernel->makeHole(source.value(), hole);
        QVERIFY(holed);
        const auto source_metrics = kernel->measure(source.value());
        const auto hole_metrics = kernel->measure(holed.value());
        QVERIFY(source_metrics);
        QVERIFY(hole_metrics);
        QVERIFY(hole_metrics.value().volume < source_metrics.value().volume);
    }

    void mirrorsAndRejectsInvalidOperations()
    {
        const auto kernel = createKernelService();
        const auto source = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(source);
        QVERIFY(kernel->mirror(source.value(), QVector3D(1.0F, 0.0F, 0.0F)));
        QVERIFY(!kernel->mirror(source.value(), QVector3D()));
        QVERIFY(!kernel->filletAllEdges(source.value(), -1.0));
        QVERIFY(!kernel->chamferAllEdges(source.value(), 0.0));
    }

    void sectionsAndMeasuresSubShapes()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(box);

        SSectionParameters section;
        section.origin = QVector3D(0.0F, 0.0F, 15.0F);
        const auto section_shape = kernel->section(box.value(), section);
        QVERIFY(section_shape);
        QVERIFY(!section_shape.value().isNull());

        const auto vertex = kernel->measureSubShape(box.value(), SSelectionMode::Vertex, 1);
        QVERIFY(vertex);
        QVERIFY(vertex.value().has_point);
        const auto edge = kernel->measureSubShape(box.value(), SSelectionMode::Edge, 1);
        QVERIFY(edge);
        QVERIFY(edge.value().length > 0.0);
        const auto face = kernel->measureSubShape(box.value(), SSelectionMode::Face, 1);
        QVERIFY(face);
        QVERIFY(face.value().area > 0.0);
        QVERIFY(face.value().length > 0.0);
        QVERIFY(face.value().has_direction);
        QVERIFY(!kernel->measureSubShape(box.value(), SSelectionMode::Face, 1000));
    }

    void measuresDistanceAndAngle()
    {
        const auto kernel = createKernelService();
        const auto first = kernel->makeBox({10.0, 10.0, 10.0});
        QVERIFY(first);
        STransformParameters transform;
        transform.translation = QVector3D(25.0F, 0.0F, 0.0F);
        const auto second = kernel->transform(first.value(), transform);
        QVERIFY(second);
        const auto distance = kernel->distanceBetween(first.value(), SSelectionMode::Vertex, 1,
                                                      second.value(), SSelectionMode::Vertex, 1);
        QVERIFY(distance);
        QVERIFY(std::abs(distance.value() - 25.0) < 1.0e-5);
        const auto angle = kernel->angleBetween(first.value(), SSelectionMode::Edge, 1,
                                                first.value(), SSelectionMode::Edge, 1);
        QVERIFY(angle);
        QVERIFY(std::abs(angle.value()) < 1.0e-8);
    }

    void materializesSceneTransformsAndRejectsShear()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(box);

        QMatrix4x4 transform;
        transform.translate(25.0F, -8.0F, 4.0F);
        transform.scale(2.0F);
        const auto materialized = kernel->materialize(box.value(), transform);
        QVERIFY(materialized);
        const auto metrics = kernel->measure(materialized.value());
        QVERIFY(metrics);
        QVERIFY(std::abs(metrics.value().minimum.x() - 25.0) < 1.0e-5);
        QVERIFY(std::abs(metrics.value().minimum.y() + 8.0) < 1.0e-5);
        QVERIFY(std::abs(metrics.value().minimum.z() - 4.0) < 1.0e-5);
        QVERIFY(std::abs(metrics.value().volume - 48000.0) < 1.0e-3);

        QMatrix4x4 shear;
        shear(0, 1) = 0.5F;
        QVERIFY(!kernel->materialize(box.value(), shear));
    }
};
} // namespace smartGraphics3D

QTEST_APPLESS_MAIN(smartGraphics3D::SKernelTest)
#include "s_kernel_test.moc"
