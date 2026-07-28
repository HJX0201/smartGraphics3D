// clang-format off
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
// clang-format on

#include "s_3d_document.h"
#include "s_kernel_service.h"
#include "s_occ_viewport.h"
#include "s_standard_cad_codec.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QSysInfo>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace smartGraphics3D
{
namespace
{
constexpr int kHeavyEntityCount = 5000;
constexpr int kHeavyClusterCount = kHeavyEntityCount / 4;

struct SProcessMemory
{
    qint64 private_bytes = 0;
    qint64 working_set = 0;
};

SResult<SKernelShape> makeComplexSet(bool heavy)
{
    const auto kernel = createKernelService();
    const auto box = kernel->makeBox({80.0, 55.0, 35.0});
    const auto cylinder = kernel->makeCylinder({22.0, 70.0});
    const auto sphere = kernel->makeSphere({31.0});
    const auto torus = kernel->makeTorus({42.0, 12.0});
    if (!box || !cylinder || !sphere || !torus)
    {
        return SResult<SKernelShape>::failure(SErrorCode::GeometryFailure,
                                              QObject::tr("无法生成基准几何"));
    }

    if (!heavy)
    {
        QList<SKernelShape> standard_shapes{box.value()};
        const QList<SKernelShape> standard_sources{cylinder.value(), sphere.value(), torus.value()};
        const QList<QVector3D> offsets{QVector3D(105.0F, 0.0F, 0.0F), QVector3D(0.0F, 100.0F, 0.0F),
                                       QVector3D(105.0F, 100.0F, 0.0F)};
        for (int index = 0; index < standard_sources.size(); ++index)
        {
            STransformParameters parameters;
            parameters.translation = offsets.at(index);
            parameters.rotation_degrees = 13.0 * static_cast<double>(index + 1);
            const auto transformed = kernel->transform(standard_sources.at(index), parameters);
            if (!transformed)
            {
                return transformed;
            }
            standard_shapes.push_back(transformed.value());
        }
        return kernel->makeCompound(standard_shapes);
    }

    const QList<SKernelShape> sources{box.value(), cylinder.value(), sphere.value(), torus.value()};
    QList<SKernelShape> shapes;
    for (int cluster = 0; cluster < kHeavyClusterCount; ++cluster)
    {
        const int column = cluster % 50;
        const int row = cluster / 50;
        const double cluster_angle = 7.0 * static_cast<double>(cluster % 51);
        const double radians = qDegreesToRadians(cluster_angle);
        const QVector3D cluster_origin(
            static_cast<float>(column * 260.0 + 18.0 * std::cos(radians)),
            static_cast<float>(row * 250.0 + 18.0 * std::sin(radians)),
            static_cast<float>((cluster % 5) * 12.0));
        for (int source_index = 0; source_index < sources.size(); ++source_index)
        {
            STransformParameters parameters;
            parameters.translation =
                cluster_origin + QVector3D(static_cast<float>((source_index % 2) * 105.0),
                                           static_cast<float>((source_index / 2) * 100.0),
                                           static_cast<float>(source_index * 14.0));
            parameters.rotation_degrees =
                cluster_angle + 13.0 * static_cast<double>(source_index + 1);
            const auto transformed = kernel->transform(sources.at(source_index), parameters);
            if (!transformed)
            {
                return transformed;
            }
            shapes.push_back(transformed.value());
        }
    }
    return kernel->makeCompound(shapes);
}

SResult<SKernelShape> loadDataset(const QString& dataset_path)
{
    SStandardCadCodec codec;
    const auto imported = codec.read(dataset_path);
    if (!imported)
    {
        return SResult<SKernelShape>::failure(imported.errorCode(), imported.message());
    }
    return SResult<SKernelShape>::success(imported.value().shape);
}

bool generateDataset(const QString& dataset_path, bool heavy)
{
    const auto generated = makeComplexSet(heavy);
    if (!generated)
    {
        return false;
    }
    SStandardCadCodec codec;
    return static_cast<bool>(codec.write(generated.value(), dataset_path));
}

bool writeJson(const QString& path, const QJsonObject& json)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QByteArray fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex();
}

SProcessMemory processMemory()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    SProcessMemory memory;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)) != FALSE)
    {
        memory.private_bytes = static_cast<qint64>(counters.PrivateUsage);
        memory.working_set = static_cast<qint64>(counters.WorkingSetSize);
    }
    return memory;
}

double median(QList<double> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

qint64 medianInteger(QList<qint64> values)
{
    std::sort(values.begin(), values.end());
    return values.at(values.size() / 2);
}

int runCase(const QString& mode_name, int count, const QString& output_path,
            const QString& dataset_path, bool heavy)
{
    const auto complex_set = loadDataset(dataset_path);
    if (!complex_set)
    {
        return 2;
    }

    const bool shared = mode_name == QStringLiteral("shared");
    S3dDocument document;
    QList<SSceneObject> objects;
    const QUuid shared_group = QUuid::createUuid();
    QElapsedTimer creation_timer;
    creation_timer.start();
    for (int index = 0; index < count; ++index)
    {
        SSceneObject object;
        object.name = QStringLiteral("Benchmark %1").arg(index + 1);
        object.shape = complex_set.value();
        object.presentation_group_id = shared ? shared_group : QUuid::createUuid();
        object.transform.translate(static_cast<float>((index % 10) * 240),
                                   static_cast<float>((index / 10) * 230), 0.0F);
        objects.push_back(std::move(object));
    }
    const auto added = document.addObjects(std::move(objects), QStringLiteral("基准场景"));
    if (!added)
    {
        return 4;
    }
    const qint64 creation_nanoseconds = creation_timer.nsecsElapsed();

    SOccViewport viewport;
    viewport.resize(1024, 768);
    viewport.setProgressiveRenderingEnabled(false);
    viewport.setDocument(&document);
    QElapsedTimer display_timer;
    display_timer.start();
    viewport.show();
    for (int attempt = 0; attempt < 1000; ++attempt)
    {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        const SRenderResourceStatistics statistics = viewport.renderResourceStatistics();
        if (viewport.isInitialized() &&
            statistics.independent_presentations + statistics.connected_instances == count)
        {
            break;
        }
    }
    viewport.repaint();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    const qint64 display_nanoseconds = display_timer.nsecsElapsed();

    QElapsedTimer redraw_timer;
    redraw_timer.start();
    for (int frame = 0; frame < 30; ++frame)
    {
        viewport.repaint();
        QApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const qint64 redraw_nanoseconds = redraw_timer.nsecsElapsed();
    const SRenderResourceStatistics statistics = viewport.renderResourceStatistics();
    const SProcessMemory memory = processMemory();

    QFileInfo dataset_info(dataset_path);
    QJsonObject dataset;
    dataset.insert(QStringLiteral("path"), dataset_info.fileName());
    dataset.insert(QStringLiteral("sizeBytes"), static_cast<double>(dataset_info.size()));
    dataset.insert(QStringLiteral("sha256"), QString::fromLatin1(fileHash(dataset_path)));
    dataset.insert(QStringLiteral("entityCount"), heavy ? kHeavyEntityCount : 4);
    dataset.insert(QStringLiteral("profile"),
                   heavy ? QStringLiteral("heavy-5000-entity") : QStringLiteral("standard"));
    dataset.insert(QStringLiteral("trianglesPerSet"),
                   count > 0 ? static_cast<double>(statistics.rendered_triangles / count) : 0.0);

    QJsonObject result;
    result.insert(QStringLiteral("mode"), mode_name);
    result.insert(QStringLiteral("count"), count);
    result.insert(QStringLiteral("creationMs"),
                  static_cast<double>(creation_nanoseconds) / 1000000.0);
    result.insert(QStringLiteral("firstDisplayMs"),
                  static_cast<double>(display_nanoseconds) / 1000000.0);
    result.insert(QStringLiteral("redraw30Ms"),
                  static_cast<double>(redraw_nanoseconds) / 1000000.0);
    result.insert(QStringLiteral("privateBytes"), static_cast<double>(memory.private_bytes));
    result.insert(QStringLiteral("workingSetBytes"), static_cast<double>(memory.working_set));
    result.insert(QStringLiteral("estimatedGpuGeometryBytes"),
                  static_cast<double>(statistics.estimated_gpu_geometry_bytes));
    result.insert(QStringLiteral("graphicStructures"), statistics.graphic_structures);
    result.insert(QStringLiteral("independentPresentations"), statistics.independent_presentations);
    result.insert(QStringLiteral("sharedPrototypes"), statistics.shared_prototypes);
    result.insert(QStringLiteral("connectedInstances"), statistics.connected_instances);
    result.insert(QStringLiteral("renderedTriangles"),
                  static_cast<double>(statistics.rendered_triangles));
    result.insert(QStringLiteral("occtStatistics"), statistics.occt_statistics);
    result.insert(QStringLiteral("dataset"), dataset);
    return writeJson(output_path, result) ? 0 : 5;
}

QJsonObject summarize(const QString& mode, int count, const QJsonArray& runs)
{
    QList<double> creation;
    QList<double> display;
    QList<double> redraw;
    QList<qint64> private_bytes;
    QList<qint64> working_set;
    QList<qint64> gpu_bytes;
    for (const QJsonValue& value : runs)
    {
        const QJsonObject run = value.toObject();
        creation.push_back(run.value(QStringLiteral("creationMs")).toDouble());
        display.push_back(run.value(QStringLiteral("firstDisplayMs")).toDouble());
        redraw.push_back(run.value(QStringLiteral("redraw30Ms")).toDouble());
        private_bytes.push_back(
            static_cast<qint64>(run.value(QStringLiteral("privateBytes")).toDouble()));
        working_set.push_back(
            static_cast<qint64>(run.value(QStringLiteral("workingSetBytes")).toDouble()));
        gpu_bytes.push_back(
            static_cast<qint64>(run.value(QStringLiteral("estimatedGpuGeometryBytes")).toDouble()));
    }
    const QJsonObject representative = runs.at(runs.size() / 2).toObject();
    QJsonObject summary;
    summary.insert(QStringLiteral("mode"), mode);
    summary.insert(QStringLiteral("count"), count);
    summary.insert(QStringLiteral("creationMs"), median(creation));
    summary.insert(QStringLiteral("firstDisplayMs"), median(display));
    summary.insert(QStringLiteral("redraw30Ms"), median(redraw));
    summary.insert(QStringLiteral("privateBytes"),
                   static_cast<double>(medianInteger(private_bytes)));
    summary.insert(QStringLiteral("workingSetBytes"),
                   static_cast<double>(medianInteger(working_set)));
    summary.insert(QStringLiteral("estimatedGpuGeometryBytes"),
                   static_cast<double>(medianInteger(gpu_bytes)));
    for (const QString& key :
         {QStringLiteral("graphicStructures"), QStringLiteral("independentPresentations"),
          QStringLiteral("sharedPrototypes"), QStringLiteral("connectedInstances"),
          QStringLiteral("renderedTriangles")})
    {
        summary.insert(key, representative.value(key));
    }
    return summary;
}

int runSuite(const QString& output_directory, const QList<int>& counts, bool heavy)
{
    QDir directory(output_directory);
    if (!directory.mkpath(QStringLiteral(".")) || !directory.mkpath(QStringLiteral("raw")))
    {
        return 10;
    }
    const QString dataset_path = directory.filePath(QStringLiteral("dataset.brep"));
    if (!generateDataset(dataset_path, heavy))
    {
        return 14;
    }
    QJsonArray all_runs;
    QJsonArray summaries;
    QJsonObject dataset;
    for (const int count : counts)
    {
        for (const QString& mode : {QStringLiteral("independent"), QStringLiteral("shared")})
        {
            QJsonArray case_runs;
            for (int run = 1; run <= 3; ++run)
            {
                const QString output_path = directory.filePath(
                    QStringLiteral("raw/%1-%2-%3.json").arg(mode).arg(count).arg(run));
                QProcess process;
                process.setProgram(QCoreApplication::applicationFilePath());
                process.setArguments(
                    {QStringLiteral("--case"), mode, QString::number(count), output_path,
                     dataset_path, heavy ? QStringLiteral("heavy") : QStringLiteral("standard")});
                process.start();
                if (!process.waitForStarted(30000) || !process.waitForFinished(180000) ||
                    process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
                {
                    return 11;
                }
                const QJsonObject result = readJson(output_path);
                if (result.isEmpty())
                {
                    return 12;
                }
                dataset = result.value(QStringLiteral("dataset")).toObject();
                case_runs.push_back(result);
                all_runs.push_back(result);
            }
            summaries.push_back(summarize(mode, count, case_runs));
        }
    }

    QJsonObject environment;
    environment.insert(QStringLiteral("timestamp"),
                       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    environment.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
    environment.insert(QStringLiteral("cpuArchitecture"), QSysInfo::currentCpuArchitecture());
    environment.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    environment.insert(QStringLiteral("occtVersion"), QStringLiteral("7.7.0"));
#ifdef NDEBUG
    environment.insert(QStringLiteral("configuration"), QStringLiteral("Release"));
#else
    environment.insert(QStringLiteral("configuration"), QStringLiteral("Debug"));
#endif

    QJsonObject report;
    report.insert(QStringLiteral("environment"), environment);
    report.insert(QStringLiteral("dataset"), dataset);
    report.insert(QStringLiteral("runs"), all_runs);
    report.insert(QStringLiteral("summary"), summaries);
    return writeJson(directory.filePath(QStringLiteral("benchmark.json")), report) ? 0 : 13;
}
} // namespace
} // namespace smartGraphics3D

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if ((arguments.size() == 6 || arguments.size() == 7) &&
        arguments.at(1) == QStringLiteral("--case"))
    {
        return smartGraphics3D::runCase(
            arguments.at(2), arguments.at(3).toInt(), arguments.at(4), arguments.at(5),
            arguments.size() == 7 && arguments.at(6) == QStringLiteral("heavy"));
    }
    if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--suite"))
    {
        return smartGraphics3D::runSuite(arguments.at(2), {1, 10, 50, 100}, false);
    }
    if (arguments.size() == 4 && arguments.at(1) == QStringLiteral("--suite-count"))
    {
        const int count = arguments.at(3).toInt();
        if (count < 1 || count > 10)
        {
            return 1;
        }
        return smartGraphics3D::runSuite(arguments.at(2), {count}, true);
    }
    if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--suite-heavy"))
    {
        return smartGraphics3D::runSuite(arguments.at(2), {1, 2, 5, 10}, true);
    }
    return 1;
}
