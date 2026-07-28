#include "s_3d_document.h"
#include "s_document_transaction.h"
#include "s_kernel_service.h"

#include <QtTest>

namespace smartGraphics3D
{
class SDocumentTest final : public QObject
{
    Q_OBJECT

  private slots:
    void addUndoRedoAndSnapshot()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("测试长方体");
        object.shape = box.value();
        const auto added = document.addObject(object, QStringLiteral("创建测试对象"));
        QVERIFY(added);
        QCOMPARE(document.objects().size(), std::size_t(1));
        QVERIFY(document.canUndo());

        QVERIFY(document.createSnapshot(QStringLiteral("初始")));
        document.undo();
        QCOMPARE(document.objects().size(), std::size_t(0));
        QVERIFY(document.canRedo());
        document.redo();
        QCOMPARE(document.objects().size(), std::size_t(1));

        QVERIFY(document.removeObjects({added.value()}));
        QCOMPARE(document.objects().size(), std::size_t(0));
        QVERIFY(document.restoreSnapshot(QStringLiteral("初始")));
        QCOMPARE(document.objects().size(), std::size_t(1));
    }

    void protectsLockedOriginal()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 10.0, 10.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("原始导入");
        object.shape = box.value();
        object.stage = SDataStage::Original;
        const auto added = document.addObject(object, QStringLiteral("导入"));
        QVERIFY(added);
        QVERIFY(document.findObject(added.value())->locked);
        const auto removed = document.removeObjects({added.value()});
        QVERIFY(!removed);
        QCOMPARE(removed.errorCode(), SErrorCode::Locked);
        QCOMPARE(document.objects().size(), std::size_t(1));
    }

    void derivedObjectsAreNonDestructiveAndRollbackOnFailure()
    {
        const auto kernel = createKernelService();
        const auto source_shape = kernel->makeBox({10.0, 10.0, 10.0});
        const auto derived_shape = kernel->makeSphere({4.0});
        QVERIFY(source_shape);
        QVERIFY(derived_shape);

        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("源对象");
        source.shape = source_shape.value();
        const auto source_id = document.addObject(source, QStringLiteral("创建源对象"));
        QVERIFY(source_id);

        SSceneObject derived;
        derived.name = QStringLiteral("派生对象");
        derived.shape = derived_shape.value();
        const auto added =
            document.addDerivedObject({source_id.value()}, derived, QStringLiteral("派生"), false);
        QVERIFY(added);
        QCOMPARE(document.objects().size(), std::size_t(2));
        QVERIFY(!document.findObject(source_id.value())->visible);
        QCOMPARE(document.findObject(added.value())->derived_from.front(), source_id.value());

        const std::size_t count_before = document.objects().size();
        SSceneObject invalid;
        invalid.name = QStringLiteral("无效");
        const auto rejected = document.addDerivedObject({source_id.value()}, invalid,
                                                        QStringLiteral("失败派生"), false);
        QVERIFY(!rejected);
        QCOMPARE(document.objects().size(), count_before);
    }

    void unitChangesUndoAndSnapshotsRestore()
    {
        S3dDocument document;
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Millimeter);
        QVERIFY(document.setLengthUnit(SLengthUnit::Inch));
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Inch);
        document.undo();
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Millimeter);
        document.redo();
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Inch);

        QVERIFY(document.createSnapshot(QStringLiteral("英制")));
        QVERIFY(document.setLengthUnit(SLengthUnit::Meter));
        QVERIFY(document.restoreSnapshot(QStringLiteral("英制")));
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Inch);
    }

    void invalidatesMeasurementWhenSourceIsDeleted()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 10.0, 10.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("测量源");
        source.shape = box.value();
        const auto source_id = document.addObject(source, QStringLiteral("创建源"));
        QVERIFY(source_id);

        SSceneObject measurement;
        measurement.name = QStringLiteral("体积");
        measurement.type = SObjectType::Measurement;
        measurement.derived_from = {source_id.value()};
        const auto measurement_id = document.addObject(measurement, QStringLiteral("创建测量"));
        QVERIFY(measurement_id);
        QVERIFY(document.removeObjects({source_id.value()}));
        const SSceneObject* stale = document.findObject(measurement_id.value());
        QVERIFY(stale);
        QVERIFY(stale->quality_warning);
        QVERIFY(!stale->quality_message.isEmpty());
    }

    void managesSceneHierarchyAndVisibilityAsTransactions()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({2.0, 2.0, 2.0});
        QVERIFY(box);
        S3dDocument document;

        SSceneObject group;
        group.name = QStringLiteral("组");
        group.type = SObjectType::Group;
        const auto group_id = document.addObject(group, QStringLiteral("创建组"));
        QVERIFY(group_id);
        SSceneObject child;
        child.name = QStringLiteral("子对象");
        child.shape = box.value();
        const auto child_id = document.addObject(child, QStringLiteral("创建对象"));
        QVERIFY(child_id);
        QVERIFY(document.setObjectParent(child_id.value(), group_id.value()));
        QCOMPARE(document.findObject(child_id.value())->parent_id, group_id.value());
        QVERIFY(!document.setObjectParent(group_id.value(), child_id.value()));

        SSceneObject other;
        other.name = QStringLiteral("其他对象");
        other.shape = box.value();
        const auto other_id = document.addObject(other, QStringLiteral("创建其他对象"));
        QVERIFY(other_id);
        QVERIFY(document.isolateObjects({child_id.value()}));
        QVERIFY(document.findObject(child_id.value())->visible);
        QVERIFY(!document.findObject(other_id.value())->visible);
        document.undo();
        QVERIFY(document.findObject(other_id.value())->visible);

        QVERIFY(document.removeObjects({group_id.value()}));
        QVERIFY(document.findObject(child_id.value())->parent_id.isNull());
    }

    void changesLengthAndAngleUnitsInOneTransaction()
    {
        S3dDocument document;
        const quint64 revision = document.revision();
        const auto changed = document.setUnits(SLengthUnit::Inch, SAngleUnit::Radian);
        QVERIFY(changed);
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Inch);
        QCOMPARE(document.unitSystem().angleUnit(), SAngleUnit::Radian);
        QCOMPARE(document.revision(), revision + 1);
        QVERIFY(document.canUndo());

        document.undo();
        QCOMPARE(document.unitSystem().lengthUnit(), SLengthUnit::Millimeter);
        QCOMPARE(document.unitSystem().angleUnit(), SAngleUnit::Degree);
    }

    void managesCoordinateSystems()
    {
        S3dDocument document;
        QCOMPARE(document.coordinateSystems().size(), std::size_t(1));
        SCoordinateSystem local;
        local.name = QStringLiteral("工件坐标系");
        local.parent_id = document.coordinateSystems().front().id;
        const auto added = document.addCoordinateSystem(local);
        QVERIFY(added);
        QCOMPARE(document.coordinateSystems().size(), std::size_t(2));

        SCoordinateSystem child;
        child.name = QStringLiteral("局部坐标系");
        child.parent_id = added.value();
        const auto child_id = document.addCoordinateSystem(child);
        QVERIFY(child_id);
        const auto parent_in_use = document.removeCoordinateSystem(added.value());
        QVERIFY(!parent_in_use);
        QCOMPARE(parent_in_use.errorCode(), SErrorCode::Conflict);
        QVERIFY(document.removeCoordinateSystem(child_id.value()));
        QVERIFY(document.removeCoordinateSystem(added.value()));
        QCOMPARE(document.coordinateSystems().size(), std::size_t(1));
        QVERIFY(!document.removeCoordinateSystem(document.coordinateSystems().front().id));
    }

    void rejectsDuplicateIdsAndInvalidReferences()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({4.0, 5.0, 6.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("对象");
        object.shape = box.value();
        const auto first = document.addObject(object, QStringLiteral("创建"));
        QVERIFY(first);
        const auto duplicate = document.addObject(object, QStringLiteral("重复创建"));
        QVERIFY(!duplicate);
        QCOMPARE(duplicate.errorCode(), SErrorCode::Conflict);

        SSceneObject bad_reference;
        bad_reference.name = QStringLiteral("错误坐标系");
        bad_reference.shape = box.value();
        bad_reference.coordinate_system_id = QUuid::createUuid();
        const auto invalid_reference = document.addObject(bad_reference, QStringLiteral("创建"));
        QVERIFY(!invalid_reference);
        QCOMPARE(invalid_reference.errorCode(), SErrorCode::NotFound);

        SCoordinateSystem duplicate_coordinate = document.coordinateSystems().front();
        duplicate_coordinate.name = QStringLiteral("重复坐标系");
        const auto duplicate_result = document.addCoordinateSystem(duplicate_coordinate);
        QVERIFY(!duplicate_result);
        QCOMPARE(duplicate_result.errorCode(), SErrorCode::Conflict);
    }

    void snapshotsAreNamedPersistentChanges()
    {
        S3dDocument document;
        document.markSaved();
        QVERIFY(!document.isDirty());
        QVERIFY(document.createSnapshot(QStringLiteral("基线")));
        QVERIFY(document.isDirty());
        QVERIFY(!document.history().isEmpty());
        QVERIFY(!document.history().back().undoable);

        const auto duplicate = document.createSnapshot(QStringLiteral("基线"));
        QVERIFY(!duplicate);
        QCOMPARE(duplicate.errorCode(), SErrorCode::Conflict);
        QCOMPARE(document.snapshotNames(), QStringList{QStringLiteral("基线")});
    }

    void validatesDisplayStyle()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({1.0, 1.0, 1.0});
        QVERIFY(box);
        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("显示对象");
        object.shape = box.value();
        const auto id = document.addObject(object, QStringLiteral("创建"));
        QVERIFY(id);

        SDisplayStyle invalid = object.display;
        invalid.transparency = 1.5;
        const auto result = document.setDisplayStyle(id.value(), invalid);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::InvalidArgument);
    }

    void overridesRestoresAndCopiesImportedAppearance()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({2.0, 3.0, 4.0});
        QVERIFY(box);
        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("彩色对象");
        object.shape = box.value();
        object.imported_appearance.valid = true;
        object.imported_appearance.base_style = {QColor(QStringLiteral("#3366cc")), 0.2};
        object.imported_appearance.fallback_style = object.imported_appearance.base_style;
        object.imported_appearance.face_overrides.push_back(
            {1, {QColor(QStringLiteral("#cc3333")), 0.35}});
        object.display.color = object.imported_appearance.fallback_style.color;
        object.display.transparency = object.imported_appearance.fallback_style.transparency;
        object.use_imported_appearance = true;
        const auto id = document.addObject(object, QStringLiteral("导入"));
        QVERIFY(id);

        QVERIFY(document.setObjectColors({id.value()}, QColor(QStringLiteral("#00aa88"))));
        const SSceneObject* overridden = document.findObject(id.value());
        QVERIFY(overridden);
        QVERIFY(!overridden->use_imported_appearance);
        QCOMPARE(overridden->display.color, QColor(QStringLiteral("#00aa88")));
        QCOMPARE(overridden->display.transparency, 0.2);

        document.undo();
        QVERIFY(document.findObject(id.value())->use_imported_appearance);
        document.redo();
        QVERIFY(!document.findObject(id.value())->use_imported_appearance);
        QVERIFY(document.restoreImportedAppearances({id.value()}));
        const SSceneObject* restored = document.findObject(id.value());
        QVERIFY(restored->use_imported_appearance);
        QCOMPARE(restored->display.color, QColor(QStringLiteral("#3366cc")));

        const auto copies = document.copyObjects({id.value()}, SCopyMode::SharedPresentation,
                                                 QStringLiteral("实例复制"));
        QVERIFY(copies);
        const SSceneObject* copy = document.findObject(copies.value().front());
        QVERIFY(copy);
        QVERIFY(copy->use_imported_appearance);
        QCOMPARE(copy->imported_appearance.face_overrides.size(), 1);

        const auto invalid = document.setObjectColors({id.value()}, QColor());
        QVERIFY(!invalid);
        QCOMPARE(invalid.errorCode(), SErrorCode::InvalidArgument);
    }

    void publicTransactionRollsBackAndCannotCommitTwice()
    {
        S3dDocument document;
        SDocumentTransaction transaction(document, QStringLiteral("失败事务"));
        const auto failed = transaction.commit(
            []()
            {
                return SResult<void>::failure(SErrorCode::InvalidArgument,
                                              QStringLiteral("测试失败"));
            });
        QVERIFY(!failed);
        QVERIFY(transaction.isFinished());
        QVERIFY(!document.isDirty());
        const auto repeated = transaction.commit(
            []()
            {
                return SResult<void>::success();
            });
        QVERIFY(!repeated);
        QCOMPARE(repeated.errorCode(), SErrorCode::Conflict);
    }

    void batchAddIsOneUndoableTransaction()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({2.0, 3.0, 4.0});
        QVERIFY(box);
        S3dDocument document;
        QList<SSceneObject> objects;
        for (int index = 0; index < 3; ++index)
        {
            SSceneObject object;
            object.name = QStringLiteral("阵列 %1").arg(index + 1);
            object.shape = box.value();
            objects.push_back(object);
        }
        const auto added = document.addObjects(objects, QStringLiteral("创建阵列"));
        QVERIFY(added);
        QCOMPARE(added.value().size(), 3);
        QCOMPARE(document.objects().size(), std::size_t(3));
        document.undo();
        QVERIFY(document.objects().empty());
    }

    void batchDerivedObjectsCommitAtomically()
    {
        const auto kernel = createKernelService();
        const auto source_shape = kernel->makeBox({4.0, 4.0, 4.0});
        const auto result_shape = kernel->makeSphere({2.0});
        QVERIFY(source_shape);
        QVERIFY(result_shape);
        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("源");
        source.shape = source_shape.value();
        const auto source_id = document.addObject(source, QStringLiteral("创建源"));
        QVERIFY(source_id);

        QList<SSceneObject> results;
        for (int index = 0; index < 2; ++index)
        {
            SSceneObject result;
            result.name = QStringLiteral("派生 %1").arg(index + 1);
            result.shape = result_shape.value();
            results.push_back(result);
        }
        const auto added =
            document.addDerivedObjects({source_id.value()}, results, QStringLiteral("批量派生"));
        QVERIFY(added);
        QCOMPARE(document.objects().size(), std::size_t(3));
        QVERIFY(!document.findObject(source_id.value())->visible);
        document.undo();
        QCOMPARE(document.objects().size(), std::size_t(1));
        QVERIFY(document.findObject(source_id.value())->visible);
    }

    void importedObjectAndCoordinateCommitAtomically()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({4.0, 5.0, 6.0});
        QVERIFY(box);
        S3dDocument document;
        const std::size_t coordinate_count = document.coordinateSystems().size();

        SCoordinateSystem imported_coordinates;
        imported_coordinates.name = QStringLiteral("导入 CAD 坐标系");
        imported_coordinates.parent_id = document.coordinateSystems().front().id;
        SSceneObject object;
        object.name = QStringLiteral("导入对象");
        object.shape = box.value();
        const auto added =
            document.addImportedObject(object, imported_coordinates, QStringLiteral("导入 CAD"),
                                       QStringLiteral("格式=STEP; 源单位=mm; 实际缩放=1"));
        QVERIFY(added);
        QCOMPARE(document.objects().size(), std::size_t(1));
        QCOMPARE(document.coordinateSystems().size(), coordinate_count + 1);
        QVERIFY(document.findObject(added.value())->locked);
        QCOMPARE(document.history().back().parameter_summary,
                 QStringLiteral("格式=STEP; 源单位=mm; 实际缩放=1"));

        document.undo();
        QVERIFY(document.objects().empty());
        QCOMPARE(document.coordinateSystems().size(), coordinate_count);

        SCoordinateSystem invalid_coordinates;
        invalid_coordinates.name = QStringLiteral("无效坐标系");
        invalid_coordinates.parent_id = QUuid::createUuid();
        SSceneObject invalid_object;
        invalid_object.name = QStringLiteral("失败导入");
        invalid_object.shape = box.value();
        const auto rejected = document.addImportedObject(invalid_object, invalid_coordinates,
                                                         QStringLiteral("失败导入"));
        QVERIFY(!rejected);
        QVERIFY(document.objects().empty());
        QCOMPARE(document.coordinateSystems().size(), coordinate_count);
    }

    void replacingInputInvalidatesDependentMeasurement()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({5.0, 5.0, 5.0});
        const auto sphere = kernel->makeSphere({2.0});
        QVERIFY(box);
        QVERIFY(sphere);
        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("工作对象");
        source.shape = box.value();
        const auto source_id = document.addObject(source, QStringLiteral("创建工作对象"));
        QVERIFY(source_id);
        SSceneObject measurement;
        measurement.name = QStringLiteral("旧测量");
        measurement.type = SObjectType::Measurement;
        measurement.derived_from = {source_id.value()};
        const auto measurement_id = document.addObject(measurement, QStringLiteral("创建测量"));
        QVERIFY(measurement_id);

        SSceneObject replacement;
        replacement.name = QStringLiteral("替换结果");
        replacement.shape = sphere.value();
        const auto replaced = document.addDerivedObject({source_id.value()}, replacement,
                                                        QStringLiteral("替换"), true);
        QVERIFY(replaced);
        const SSceneObject* stale = document.findObject(measurement_id.value());
        QVERIFY(stale);
        QVERIFY(stale->quality_warning);
        QVERIFY(stale->quality_message.contains(QStringLiteral("替换")));
    }

    void copiesIndependentAndSharedPresentationsTransactionally()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({8.0, 9.0, 10.0});
        QVERIFY(box);

        S3dDocument document;
        SSceneObject source;
        source.name = QStringLiteral("复制源");
        source.shape = box.value();
        const auto source_id = document.addObject(source, QStringLiteral("创建源"));
        QVERIFY(source_id);
        const QUuid source_group = document.findObject(source_id.value())->presentation_group_id;

        const auto independent = document.copyObjects(
            {source_id.value()}, SCopyMode::IndependentPresentation, QStringLiteral("普通复制"));
        QVERIFY(independent);
        QCOMPARE(independent.value().size(), 1);
        QVERIFY(document.findObject(independent.value().front())->presentation_group_id !=
                source_group);

        const auto shared =
            document.copyObjects({source_id.value(), independent.value().front()},
                                 SCopyMode::SharedPresentation, QStringLiteral("实例复制"));
        QVERIFY(shared);
        QCOMPARE(shared.value().size(), 2);
        QCOMPARE(document.findObject(shared.value().front())->presentation_group_id, source_group);
        QCOMPARE(document.presentationGroupMemberCount(source_group), 2);

        const QUuid independent_group =
            document.findObject(independent.value().front())->presentation_group_id;
        QCOMPARE(document.findObject(shared.value().back())->presentation_group_id,
                 independent_group);
        QCOMPARE(document.presentationGroupMemberCount(independent_group), 2);

        document.undo();
        QCOMPARE(document.objects().size(), std::size_t(2));
        document.redo();
        QCOMPARE(document.objects().size(), std::size_t(4));
    }

    void updatesInstanceTransformAndRejectsInvalidMatrices()
    {
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({4.0, 5.0, 6.0});
        QVERIFY(box);
        S3dDocument document;
        SSceneObject object;
        object.name = QStringLiteral("实例");
        object.shape = box.value();
        const auto id = document.addObject(object, QStringLiteral("创建"));
        QVERIFY(id);

        QMatrix4x4 transform;
        transform.translate(12.0F, 3.0F, -7.0F);
        transform.rotate(35.0F, 0.0F, 0.0F, 1.0F);
        QVERIFY(document.setObjectTransform(id.value(), transform, QStringLiteral("实例移动")));
        QVERIFY(document.findObject(id.value())->transform == transform);
        document.undo();
        QVERIFY(document.findObject(id.value())->transform.isIdentity());

        QMatrix4x4 shear;
        shear(0, 1) = 0.25F;
        const auto rejected =
            document.setObjectTransform(id.value(), shear, QStringLiteral("非法变换"));
        QVERIFY(!rejected);
        QCOMPARE(rejected.errorCode(), SErrorCode::InvalidArgument);
    }
};
} // namespace smartGraphics3D

QTEST_APPLESS_MAIN(smartGraphics3D::SDocumentTest)
#include "s_document_test.moc"
