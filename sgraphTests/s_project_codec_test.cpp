#include "s_kernel_service.h"
#include "s_project_codec.h"

#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

namespace smartGraphics3D
{
class SProjectCodecTest final : public QObject
{
    Q_OBJECT

  private slots:
    void roundTripsProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("中文项目.sg3d"));

        const auto kernel = createKernelService();
        const auto sphere = kernel->makeSphere({25.0});
        QVERIFY(sphere);

        S3dDocument source;
        source.setProjectName(QStringLiteral("中文测试"));
        SSceneObject object;
        object.name = QStringLiteral("测试球体");
        object.shape = sphere.value();
        object.display.color = QColor(QStringLiteral("#31a9e1"));
        object.imported_appearance.valid = true;
        object.imported_appearance.base_style = {QColor(QStringLiteral("#204080")), 0.15};
        object.imported_appearance.fallback_style = {QColor(QStringLiteral("#204080")), 0.15};
        object.imported_appearance.face_overrides.push_back(
            {1, {QColor(QStringLiteral("#e04030")), 0.4}});
        object.use_imported_appearance = true;
        QVERIFY(source.addObject(object, QStringLiteral("创建球体")));

        SProjectCodec codec;
        QVERIFY(codec.save(source, path));
        QVERIFY(QFileInfo::exists(path));

        S3dDocument loaded;
        QVERIFY(codec.load(loaded, path));
        QCOMPARE(loaded.projectName(), QStringLiteral("中文测试"));
        QCOMPARE(loaded.objects().size(), std::size_t(1));
        QCOMPARE(loaded.objects().front().name, QStringLiteral("测试球体"));
        QVERIFY(loaded.objects().front().shape.isValid());
        QVERIFY(loaded.objects().front().use_imported_appearance);
        QCOMPARE(loaded.objects().front().imported_appearance.face_overrides.size(), 1);
        QCOMPARE(loaded.objects().front().imported_appearance.face_overrides.front().face_index, 1);
        QCOMPARE(loaded.objects().front().imported_appearance.face_overrides.front().style.color,
                 QColor(QStringLiteral("#e04030")));
    }

    void rejectsTruncatedProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("broken.sg3d"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("SGRAPH3D");
        file.close();

        S3dDocument document;
        SProjectCodec codec;
        const auto result = codec.load(document, path);
        QVERIFY(!result);
    }

    void rejectsOutOfRangeImportedFaceAppearance()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("invalid-color-face.sg3d"));
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({2.0, 3.0, 4.0});
        QVERIFY(box);

        S3dDocument source;
        SSceneObject object;
        object.name = QStringLiteral("彩色对象");
        object.shape = box.value();
        object.imported_appearance.valid = true;
        object.imported_appearance.base_style = {QColor(QStringLiteral("#336699")), 0.0};
        object.imported_appearance.fallback_style = object.imported_appearance.base_style;
        object.imported_appearance.face_overrides.push_back(
            {1, {QColor(QStringLiteral("#cc3300")), 0.0}});
        object.use_imported_appearance = true;
        QVERIFY(source.addObject(object, QStringLiteral("导入")));

        SProjectCodec codec;
        QVERIFY(codec.save(source, path));
        QFile input(path);
        QVERIFY(input.open(QIODevice::ReadOnly));
        QDataStream input_stream(&input);
        input_stream.setByteOrder(QDataStream::LittleEndian);
        QByteArray magic;
        QByteArray metadata;
        quint32 version = 0;
        input_stream >> magic >> version >> metadata;
        const QByteArray shape_tail = input.readAll();
        input.close();

        QJsonObject root = QJsonDocument::fromJson(metadata).object();
        QJsonArray objects = root.value(QStringLiteral("objects")).toArray();
        QJsonObject object_json = objects.at(0).toObject();
        QJsonObject appearance = object_json.value(QStringLiteral("importedAppearance")).toObject();
        QJsonArray faces = appearance.value(QStringLiteral("faces")).toArray();
        QJsonObject face = faces.at(0).toObject();
        face.insert(QStringLiteral("faceIndex"), 999);
        faces.replace(0, face);
        appearance.insert(QStringLiteral("faces"), faces);
        object_json.insert(QStringLiteral("importedAppearance"), appearance);
        objects.replace(0, object_json);
        root.insert(QStringLiteral("objects"), objects);

        QSaveFile output(path);
        QVERIFY(output.open(QIODevice::WriteOnly));
        QDataStream output_stream(&output);
        output_stream.setByteOrder(QDataStream::LittleEndian);
        output_stream << magic << version << QJsonDocument(root).toJson(QJsonDocument::Compact);
        QCOMPARE(output.write(shape_tail), static_cast<qint64>(shape_tail.size()));
        QVERIFY(output.commit());

        S3dDocument loaded;
        const auto result = codec.load(loaded, path);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::CorruptData);
    }

    void persistsUnitsMeasurementsTransformsAndSnapshots()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("完整项目.sg3d"));
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(box);

        S3dDocument source;
        QVERIFY(source.setLengthUnit(SLengthUnit::Inch));
        SCoordinateSystem local;
        local.name = QStringLiteral("工件坐标系");
        local.parent_id = source.coordinateSystems().front().id;
        local.transform_to_parent.translate(10.0F, 20.0F, 30.0F);
        const auto coordinate_id = source.addCoordinateSystem(local);
        QVERIFY(coordinate_id);
        SSceneObject shape;
        shape.name = QStringLiteral("变换实体");
        shape.shape = box.value();
        shape.coordinate_system_id = coordinate_id.value();
        shape.transform.translate(1.0F, 2.0F, 3.0F);
        const auto shape_id = source.addObject(shape, QStringLiteral("创建"));
        QVERIFY(shape_id);

        SSceneObject measurement;
        measurement.name = QStringLiteral("体积测量");
        measurement.type = SObjectType::Measurement;
        measurement.derived_from = {shape_id.value()};
        measurement.custom_properties.insert(QStringLiteral("volume"), 6000.0);
        QVERIFY(source.addObject(measurement, QStringLiteral("测量")));
        QVERIFY(source.createSnapshot(QStringLiteral("发布前")));

        SProjectCodec codec;
        QVERIFY(codec.save(source, path));
        S3dDocument loaded;
        QVERIFY(codec.load(loaded, path));
        QCOMPARE(loaded.unitSystem().lengthUnit(), SLengthUnit::Inch);
        QCOMPARE(loaded.objects().size(), std::size_t(2));
        QCOMPARE(loaded.objects().front().transform(0, 3), 1.0F);
        QCOMPARE(loaded.coordinateSystems().size(), std::size_t(2));
        QCOMPARE(loaded.objects().front().coordinate_system_id, coordinate_id.value());
        QCOMPARE(loaded.snapshotNames(), QStringList{QStringLiteral("发布前")});
        QVERIFY(loaded.restoreSnapshot(QStringLiteral("发布前")));
        QCOMPARE(loaded.objects().size(), std::size_t(2));
        QCOMPARE(loaded.coordinateSystems().size(), std::size_t(2));
    }

    void reportsMissingExternalReferenceUsingCachedGeometry()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("external.sg3d"));
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({10.0, 10.0, 10.0});
        QVERIFY(box);

        S3dDocument source;
        SSceneObject object;
        object.name = QStringLiteral("外部模型");
        object.shape = box.value();
        object.external_reference = true;
        object.external_path = directory.filePath(QStringLiteral("不存在.step"));
        QVERIFY(source.addObject(object, QStringLiteral("导入")));
        SProjectCodec codec;
        QVERIFY(codec.save(source, path));

        S3dDocument loaded;
        QVERIFY(codec.load(loaded, path));
        QVERIFY(loaded.objects().front().quality_warning);
        QVERIFY(loaded.objects().front().shape.isValid());
    }

    void keepsFormalProjectSeparateFromRecoveryCopy()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString project_path = directory.filePath(QStringLiteral("正式项目.sg3d"));
        const QString recovery_path = project_path + QStringLiteral(".autosave");
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({3.0, 4.0, 5.0});
        const auto sphere = kernel->makeSphere({2.0});
        QVERIFY(box);
        QVERIFY(sphere);

        S3dDocument working;
        SSceneObject first;
        first.name = QStringLiteral("正式对象");
        first.shape = box.value();
        QVERIFY(working.addObject(first, QStringLiteral("创建")));
        SProjectCodec codec;
        QVERIFY(codec.save(working, project_path));

        SSceneObject recovered;
        recovered.name = QStringLiteral("恢复对象");
        recovered.shape = sphere.value();
        QVERIFY(working.addObject(recovered, QStringLiteral("创建")));
        QVERIFY(codec.save(working, recovery_path));

        S3dDocument formal;
        S3dDocument recovery;
        QVERIFY(codec.load(formal, project_path));
        QVERIFY(codec.load(recovery, recovery_path));
        QCOMPARE(formal.objects().size(), std::size_t(1));
        QCOMPARE(recovery.objects().size(), std::size_t(2));
        QCOMPARE(recovery.objects().back().name, QStringLiteral("恢复对象"));
    }

    void rejectsUnsupportedVersion()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("future.sg3d"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << QByteArray("SGRAPH3D") << quint32(999) << QByteArray("{}");
        file.close();

        S3dDocument document;
        SProjectCodec codec;
        const auto result = codec.load(document, path);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::VersionMismatch);
    }

    void rejectsStructurallyCorruptProject()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("duplicate-coordinate.sg3d"));

        const QUuid duplicate_id = QUuid::createUuid();
        QJsonObject coordinate;
        coordinate.insert(QStringLiteral("id"), duplicate_id.toString(QUuid::WithoutBraces));
        coordinate.insert(QStringLiteral("name"), QStringLiteral("世界坐标系"));
        coordinate.insert(QStringLiteral("parentId"), QString());
        QJsonArray matrix;
        for (int index = 0; index < 16; ++index)
        {
            matrix.push_back(index % 5 == 0 ? 1.0 : 0.0);
        }
        coordinate.insert(QStringLiteral("transform"), matrix);

        QJsonObject root;
        root.insert(QStringLiteral("projectId"),
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
        root.insert(QStringLiteral("projectName"), QStringLiteral("损坏项目"));
        root.insert(QStringLiteral("lengthUnit"), 0);
        root.insert(QStringLiteral("angleUnit"), 0);
        root.insert(QStringLiteral("objects"), QJsonArray());
        root.insert(QStringLiteral("history"), QJsonArray());
        root.insert(QStringLiteral("snapshots"), QJsonArray());
        root.insert(QStringLiteral("coordinateSystems"), QJsonArray{coordinate, coordinate});

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << QByteArray("SGRAPH3D") << quint32(3)
               << QJsonDocument(root).toJson(QJsonDocument::Compact);
        file.close();

        S3dDocument document;
        SProjectCodec codec;
        const auto result = codec.load(document, path);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::CorruptData);
    }

    void archivesProjectAndExternalDependencies()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString dependency = directory.filePath(QStringLiteral("外部参考.brep"));
        QFile dependency_file(dependency);
        QVERIFY(dependency_file.open(QIODevice::WriteOnly));
        dependency_file.write("reference");
        dependency_file.close();

        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({2.0, 3.0, 4.0});
        QVERIFY(box);
        S3dDocument document;
        document.setProjectName(QStringLiteral("归档测试"));
        SSceneObject object;
        object.name = QStringLiteral("外部对象");
        object.shape = box.value();
        object.external_reference = true;
        object.external_path = dependency;
        QVERIFY(document.addObject(object, QStringLiteral("导入")));

        const QString archive_path = directory.filePath(QStringLiteral("交付归档"));
        SProjectCodec codec;
        QVERIFY(codec.createArchive(document, archive_path));
        QVERIFY(QFileInfo::exists(QDir(archive_path).filePath(QStringLiteral("manifest.json"))));
        QVERIFY(QFileInfo::exists(QDir(archive_path).filePath(QStringLiteral("归档测试.sg3d"))));
        QVERIFY(QFileInfo::exists(
            QDir(archive_path).filePath(QStringLiteral("dependencies/外部参考.brep"))));
    }

    void persistsSharedPresentationGroupsAndTransforms()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("instances.sg3d"));
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({7.0, 8.0, 9.0});
        QVERIFY(box);

        S3dDocument source;
        SSceneObject object;
        object.name = QStringLiteral("原型");
        object.shape = box.value();
        const auto source_id = source.addObject(object, QStringLiteral("创建"));
        QVERIFY(source_id);
        const auto copies = source.copyObjects({source_id.value()}, SCopyMode::SharedPresentation,
                                               QStringLiteral("实例复制"));
        QVERIFY(copies);
        QMatrix4x4 transform;
        transform.translate(25.0F, 0.0F, 0.0F);
        QVERIFY(source.setObjectTransform(copies.value().front(), transform));
        QVERIFY(source.createSnapshot(QStringLiteral("实例状态")));

        SProjectCodec codec;
        QVERIFY(codec.save(source, path));
        S3dDocument loaded;
        QVERIFY(codec.load(loaded, path));
        QCOMPARE(loaded.objects().size(), std::size_t(2));
        QCOMPARE(loaded.objects().front().presentation_group_id,
                 loaded.objects().back().presentation_group_id);
        QVERIFY(loaded.objects().back().transform == transform);
        QCOMPARE(
            loaded.presentationGroupMemberCount(loaded.objects().front().presentation_group_id), 2);
        QVERIFY(loaded.restoreSnapshot(QStringLiteral("实例状态")));
        QVERIFY(loaded.objects().back().transform == transform);
    }

    void rejectsDifferentGeometryInsideOnePresentationGroup()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("corrupt-instance-group.sg3d"));
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({5.0, 6.0, 7.0});
        const auto sphere = kernel->makeSphere({4.0});
        QVERIFY(box);
        QVERIFY(sphere);

        const QUuid group_id = QUuid::createUuid();
        S3dDocument source;
        SSceneObject first;
        first.name = QStringLiteral("错误组一");
        first.shape = box.value();
        first.presentation_group_id = group_id;
        QVERIFY(source.addObject(first, QStringLiteral("创建")));
        SSceneObject second;
        second.name = QStringLiteral("错误组二");
        second.shape = sphere.value();
        second.presentation_group_id = group_id;
        QVERIFY(source.addObject(second, QStringLiteral("创建")));

        SProjectCodec codec;
        QVERIFY(codec.save(source, path));
        S3dDocument loaded;
        const auto result = codec.load(loaded, path);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::CorruptData);
    }

    void rejectsLegacyProjectExtension()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("legacy-v2.s3"
                                                               "dcam"));
        S3dDocument loaded;
        SProjectCodec codec;
        const auto result = codec.load(loaded, path);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::Unsupported);
    }

    void rejectsLegacyProjectMagic()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("legacy-magic.sg3d"));
        const auto kernel = createKernelService();
        const auto box = kernel->makeBox({3.0, 4.0, 5.0});
        QVERIFY(box);

        S3dDocument source;
        SSceneObject object;
        object.name = QStringLiteral("旧项目对象");
        object.shape = box.value();
        QVERIFY(source.addObject(object, QStringLiteral("创建")));
        SProjectCodec codec;
        QVERIFY(codec.save(source, path));

        QFile input(path);
        QVERIFY(input.open(QIODevice::ReadOnly));
        QDataStream input_stream(&input);
        input_stream.setByteOrder(QDataStream::LittleEndian);
        QByteArray magic;
        QByteArray metadata;
        quint32 version = 0;
        input_stream >> magic >> version >> metadata;
        QCOMPARE(magic, QByteArray("SGRAPH3D"));
        QCOMPARE(version, quint32(3));
        const QByteArray shape_tail = input.readAll();
        input.close();

        QSaveFile output(path);
        QVERIFY(output.open(QIODevice::WriteOnly));
        QDataStream output_stream(&output);
        output_stream.setByteOrder(QDataStream::LittleEndian);
        output_stream << QByteArray("S3"
                                    "DCAM")
                      << quint32(2) << metadata;
        QCOMPARE(output.write(shape_tail), static_cast<qint64>(shape_tail.size()));
        QVERIFY(output.commit());

        S3dDocument loaded;
        const auto result = codec.load(loaded, path);
        QVERIFY(!result);
        QCOMPARE(result.errorCode(), SErrorCode::CorruptData);
    }
};
} // namespace smartGraphics3D

QTEST_APPLESS_MAIN(smartGraphics3D::SProjectCodecTest)
#include "s_project_codec_test.moc"
