#include "s_kernel_service.h"
#include "s_standard_cad_codec.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace smartGraphics3D
{
class SCadCodecTest final : public QObject
{
    Q_OBJECT

  private slots:
    void supportsDeclaredFormats()
    {
        SStandardCadCodec codec;
        const QStringList formats = codec.extensions();
        for (const QString& extension :
             {QStringLiteral("step"), QStringLiteral("stp"), QStringLiteral("iges"),
              QStringLiteral("igs"), QStringLiteral("brep"), QStringLiteral("stl"),
              QStringLiteral("obj")})
        {
            QVERIFY2(formats.contains(extension), qPrintable(extension));
        }
    }

    void roundTripsCommonFormatsAndChinesePath_data()
    {
        QTest::addColumn<QString>("extension");
        QTest::newRow("brep") << QStringLiteral("brep");
        QTest::newRow("step") << QStringLiteral("step");
        QTest::newRow("iges") << QStringLiteral("iges");
        QTest::newRow("stl") << QStringLiteral("stl");
        QTest::newRow("obj") << QStringLiteral("obj");
    }

    void roundTripsCommonFormatsAndChinesePath()
    {
        QFETCH(QString, extension);
        QTemporaryDir directory(QStringLiteral("smartGraphics3D-中文-XXXXXX"));
        QVERIFY(directory.isValid());
        const auto kernel = createKernelService();
        const auto shape = kernel->makeBox({10.0, 20.0, 30.0});
        QVERIFY(shape);
        SStandardCadCodec codec;

        const QString path = directory.filePath(QStringLiteral("中文模型.%1").arg(extension));
        const auto written = codec.write(shape.value(), path);
        QVERIFY2(written, qPrintable(written.message() + written.details()));
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(!written.value().preserved_properties.isEmpty());
        const auto loaded = codec.read(path);
        QVERIFY2(loaded, qPrintable(loaded.message() + loaded.details()));
        QVERIFY(!loaded.value().shape.isNull());
        QCOMPARE(loaded.value().suggested_name, QStringLiteral("中文模型"));
    }

    void rejectsUnsupportedAndMissingFiles()
    {
        SStandardCadCodec codec;
        const auto kernel = createKernelService();
        const auto shape = kernel->makeBox({1.0, 1.0, 1.0});
        QVERIFY(shape);
        const auto unsupported = codec.write(shape.value(), QStringLiteral("model.unknown"));
        QVERIFY(!unsupported);
        QCOMPARE(unsupported.errorCode(), SErrorCode::Unsupported);
        const auto missing = codec.read(QStringLiteral("Z:/definitely/missing.step"));
        QVERIFY(!missing);
        QCOMPARE(missing.errorCode(), SErrorCode::NotFound);
    }

    void reportsExportCompatibilityBeforeWriting()
    {
        SStandardCadCodec codec;
        const auto step = codec.compatibilityReport(QStringLiteral("model.step"));
        QVERIFY(step);
        QVERIFY(step.value().lost_properties.contains(QStringLiteral("smartGraphics3D 操作历史")));

        const auto stl = codec.compatibilityReport(QStringLiteral("model.stl"));
        QVERIFY(stl);
        QVERIFY(stl.value().lost_properties.contains(QStringLiteral("精确曲面")));
        QVERIFY(!stl.value().warnings.isEmpty());

        const auto unsupported = codec.compatibilityReport(QStringLiteral("model.unsupported"));
        QVERIFY(!unsupported);
        QCOMPARE(unsupported.errorCode(), SErrorCode::Unsupported);
    }
};
} // namespace smartGraphics3D

QTEST_APPLESS_MAIN(smartGraphics3D::SCadCodecTest)
#include "s_cad_codec_test.moc"
