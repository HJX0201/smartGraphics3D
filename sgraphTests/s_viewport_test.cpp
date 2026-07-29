#include "s_3d_document.h"
#include "s_kernel_service.h"
#include "s_occ_viewport.h"
#include "s_render_quality.h"

#include <QtTest>

namespace smartGraphics3D
{
class SViewportTest final : public QObject
{
    Q_OBJECT

  private slots:
    void initializesGridViewsDisplayAndClipPlanes()
    {
        SOccViewport viewport;
        viewport.resize(800, 600);
        viewport.show();
        QTRY_VERIFY_WITH_TIMEOUT(viewport.isInitialized(), 3000);
        QVERIFY(viewport.isGridActive());
        viewport.setGridVisible(false);
        QVERIFY(!viewport.isGridActive());
        viewport.setGridVisible(true);
        QVERIFY(viewport.isGridActive());

        viewport.setStandardView(SStandardView::Front);
        QCOMPARE(viewport.standardView(), SStandardView::Front);
        viewport.setPerspective(true);
        QVERIFY(viewport.isPerspective());
        viewport.setFreeRotation(true);
        QVERIFY(viewport.isFreeRotation());
        viewport.setDisplayMode(SDisplayMode::HiddenLine);
        QCOMPARE(viewport.displayMode(), SDisplayMode::HiddenLine);
        viewport.setDisplayMode(SDisplayMode::Transparent);
        QCOMPARE(viewport.displayMode(), SDisplayMode::Transparent);
        viewport.setSelectionMode(SSelectionMode::Face);
        QCOMPARE(viewport.selectionMode(), SSelectionMode::Face);

        viewport.setClipPlanes(
            {{QVector3D(1.0F, 0.0F, 0.0F), 5.0, false}, {QVector3D(0.0F, 0.0F, 1.0F), 10.0, true}});
        QCOMPARE(viewport.clipPlaneCount(), 2);
    }

    void keepsDeferredViewportStateUntilInitialization()
    {
        SOccViewport viewport;
        viewport.setStandardView(SStandardView::Right);
        viewport.setPerspective(true);
        viewport.setFreeRotation(true);
        viewport.setGridVisible(false);
        viewport.setDisplayMode(SDisplayMode::Wireframe);
        viewport.setSelectionMode(SSelectionMode::Edge);
        viewport.setClipPlanes({{QVector3D(0.0F, 1.0F, 0.0F), 12.0, false}});

        viewport.resize(800, 600);
        viewport.show();
        QTRY_VERIFY_WITH_TIMEOUT(viewport.isInitialized(), 3000);
        QCOMPARE(viewport.standardView(), SStandardView::Right);
        QVERIFY(viewport.isPerspective());
        QVERIFY(viewport.isFreeRotation());
        QVERIFY(!viewport.isGridActive());
        viewport.setGridVisible(true);
        QVERIFY(viewport.isGridActive());
        QCOMPARE(viewport.displayMode(), SDisplayMode::Wireframe);
        QCOMPARE(viewport.selectionMode(), SSelectionMode::Edge);
        QCOMPARE(viewport.clipPlaneCount(), 1);
    }

    void synchronizesCameraAndSelectionHighlight()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(box);
        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("选择测试");
        object.shape = box.value();
        const auto object_id = document.addObject(object, QStringLiteral("创建"));
        QVERIFY(object_id);

        SOccViewport first;
        SOccViewport second;
        first.resize(640, 480);
        second.resize(640, 480);
        first.setDocument(&document);
        second.setDocument(&document);
        first.show();
        second.show();
        QTRY_VERIFY_WITH_TIMEOUT(first.isInitialized() && second.isInitialized(), 3000);

        first.setPerspective(true);
        second.copyCameraFrom(first);
        QVERIFY(second.isPerspective());
        first.setSelectionMode(SSelectionMode::Object);
        first.selectObject(object_id.value());
        QCOMPARE(first.selectedObjectIds(), QList<SObjectId>{object_id.value()});
        second.setSelectedObjects(first.selectedObjectIds());
        QCOMPARE(second.selectedObjectIds(), QList<SObjectId>{object_id.value()});

        QVERIFY(document.setObjectFrozen(object_id.value(), true));
        first.selectObject(object_id.value());
        QVERIFY(first.selectedObjectIds().isEmpty());
    }

    void selectsProgressiveQualityForLargeMeshes()
    {
        QVERIFY(!SRenderQualityPolicy::shouldUseProgressiveRendering(199999));
        QVERIFY(SRenderQualityPolicy::shouldUseProgressiveRendering(200000));
        QVERIFY(SRenderQualityPolicy::deviationCoefficient(true) >
                SRenderQualityPolicy::deviationCoefficient(false));
    }

    void usesConnectedPresentationsForSharedGroups()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 12.0, 14.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("共享原型");
        source.shape = box.value();
        const auto source_id = document.addObject(source, QStringLiteral("创建"));
        QVERIFY(source_id);
        const auto copies = document.copyObjects({source_id.value()}, SCopyMode::SharedPresentation,
                                                 QStringLiteral("实例复制"));
        QVERIFY(copies);
        QMatrix4x4 transform;
        transform.translate(30.0F, 0.0F, 0.0F);
        QVERIFY(document.setObjectTransform(copies.value().front(), transform));

        SOccViewport viewport;
        viewport.resize(640, 480);
        viewport.setDocument(&document);
        viewport.show();
        QTRY_VERIFY_WITH_TIMEOUT(viewport.isInitialized(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(viewport.renderResourceStatistics().connected_instances, 2, 3000);
        const SRenderResourceStatistics shared = viewport.renderResourceStatistics();
        QCOMPARE(shared.shared_prototypes, 1);
        QCOMPARE(shared.independent_presentations, 0);
        QVERIFY(shared.estimated_gpu_geometry_bytes > 0);
        QVERIFY(shared.rendered_triangles > 0);
        QVERIFY(!shared.occt_statistics.isEmpty());

        viewport.setSelectionMode(SSelectionMode::Object);
        viewport.selectObject(copies.value().front());
        QCOMPARE(viewport.selectedObjectIds(), QList<SObjectId>{copies.value().front()});

        S3dDocument independent_document;
        SSceneObject independent_source;
        independent_source.name = QStringLiteral("独立源");
        independent_source.shape = box.value();
        const auto independent_id =
            independent_document.addObject(independent_source, QStringLiteral("创建"));
        QVERIFY(independent_id);
        QVERIFY(independent_document.copyObjects({independent_id.value()},
                                                 SCopyMode::IndependentPresentation,
                                                 QStringLiteral("普通复制")));
        viewport.setDocument(&independent_document);
        QTRY_COMPARE_WITH_TIMEOUT(viewport.renderResourceStatistics().independent_presentations, 2,
                                  3000);
        const SRenderResourceStatistics independent = viewport.renderResourceStatistics();
        QCOMPARE(independent.shared_prototypes, 0);
        QCOMPARE(independent.connected_instances, 0);
        QCOMPARE(independent.rendered_triangles, shared.rendered_triangles);
        QCOMPARE(independent.estimated_gpu_geometry_bytes, shared.estimated_gpu_geometry_bytes * 2);
    }

    void usesColoredPrototypeForImportedAppearance()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 12.0, 14.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("彩色共享原型");
        source.shape = box.value();
        source.imported_appearance.valid = true;
        source.imported_appearance.base_style = {QColor(QStringLiteral("#3366cc")), 0.1};
        source.imported_appearance.fallback_style = source.imported_appearance.base_style;
        source.imported_appearance.face_overrides.push_back(
            {1, {QColor(QStringLiteral("#cc3333")), 0.3}});
        source.use_imported_appearance = true;
        const auto source_id = document.addObject(source, QStringLiteral("导入"));
        QVERIFY(source_id);
        const auto copies = document.copyObjects({source_id.value()}, SCopyMode::SharedPresentation,
                                                 QStringLiteral("实例复制"));
        QVERIFY(copies);

        SOccViewport viewport;
        viewport.resize(640, 480);
        viewport.setDocument(&document);
        viewport.show();
        QTRY_VERIFY_WITH_TIMEOUT(viewport.isInitialized(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(viewport.renderResourceStatistics().connected_instances, 2, 3000);
        const SRenderResourceStatistics statistics = viewport.renderResourceStatistics();
        QCOMPARE(statistics.shared_prototypes, 1);
        QCOMPARE(statistics.colored_prototypes, 1);

        viewport.setSelectionMode(SSelectionMode::Object);
        viewport.selectObject(copies.value().front());
        QCOMPARE(viewport.selectedObjectIds(), QList<SObjectId>{copies.value().front()});
    }
};
} // namespace smartGraphics3D

QTEST_MAIN(smartGraphics3D::SViewportTest)
#include "s_viewport_test.moc"
