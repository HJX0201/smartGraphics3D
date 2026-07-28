#include "s_import_options.h"
#include "s_main_window.h"
#include "s_occ_viewport.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTreeWidget>
#include <cmath>

namespace smartGraphics3D
{
namespace
{
QString cadFilter()
{
    return QObject::tr("三维文件 (*.step *.stp *.iges *.igs *.brep *.stl *.obj);;"
                       "STEP (*.step *.stp);;IGES (*.iges *.igs);;BREP (*.brep);;"
                       "STL (*.stl);;OBJ (*.obj)");
}

QString projectFilter()
{
    return QObject::tr("smartGraphics3D 项目 (*.sg3d)");
}

QString recoveryPath(const S3dDocument& document)
{
    if (!document.filePath().isEmpty())
    {
        return document.filePath() + QStringLiteral(".autosave");
    }
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(directory).filePath(QStringLiteral("untitled.sg3d.autosave"));
}

template <typename T>
void showResultFailure(QWidget* parent, const QString& action, const SResult<T>& result)
{
    QMessageBox box(QMessageBox::Critical, action,
                    QObject::tr("%1\n\n可能原因：%2")
                        .arg(result.message(), result.details().isEmpty()
                                                   ? QObject::tr("输入参数或几何条件不满足")
                                                   : result.details()),
                    QMessageBox::Ok, parent);
    box.setInformativeText(QObject::tr("项目未被修改。请调整参数后重试。"));
    box.setDetailedText(
        QObject::tr("错误码：%1\n诊断详情：%2\n"
                    "相关记录已写入任务面板或结构化日志。")
            .arg(static_cast<int>(result.errorCode()))
            .arg(result.details().isEmpty() ? QObject::tr("无") : result.details()));
    box.exec();
}

bool requestDouble(QWidget* parent, const QString& title, const QString& label, double initial,
                   double minimum, double maximum, double& value, int decimals = 3)
{
    bool accepted = false;
    const double candidate = QInputDialog::getDouble(parent, title, label, initial, minimum,
                                                     maximum, decimals, &accepted);
    if (accepted)
    {
        value = candidate;
    }
    return accepted;
}
} // namespace

void SMainWindow::showFailure(const QString& action, const SResult<void>& result)
{
    appendLog(QStringLiteral("ERROR"), action + QStringLiteral("：") + result.message(),
              result.details(), result.errorCode());
    showResultFailure(this, action, result);
}

bool SMainWindow::confirmSaveChanges()
{
    if (!m_document.isDirty())
    {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("存在未保存修改"), tr("项目“%1”包含未保存修改。").arg(m_document.projectName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel)
    {
        return false;
    }
    return answer == QMessageBox::Discard || saveProject();
}

void SMainWindow::newProject()
{
    if (!confirmSaveChanges())
    {
        return;
    }
    m_document.newDocument();
    appendLog(QStringLiteral("INFO"), tr("已创建新项目"));
}

void SMainWindow::openProject()
{
    if (!confirmSaveChanges())
    {
        return;
    }
    const QString file_path =
        QFileDialog::getOpenFileName(this, tr("打开 smartGraphics3D 项目"), {}, projectFilter());
    if (file_path.isEmpty())
    {
        return;
    }

    QString load_path = file_path;
    const QString autosave_path = file_path + QStringLiteral(".autosave");
    const QFileInfo project_info(file_path);
    const QFileInfo autosave_info(autosave_path);
    if (autosave_info.exists() && autosave_info.lastModified() > project_info.lastModified())
    {
        const auto answer = QMessageBox::question(
            this, tr("发现自动恢复版本"),
            tr("自动恢复版本比正式项目更新。\n\n正式项目：%1\n恢复版本：%2\n\n是否打开恢复版本？")
                .arg(project_info.lastModified().toString(Qt::ISODate),
                     autosave_info.lastModified().toString(Qt::ISODate)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes)
        {
            load_path = autosave_path;
        }
    }

    const auto result = m_project_codec.load(m_document, load_path);
    if (!result)
    {
        showFailure(tr("打开项目失败"), result);
        return;
    }
    m_document.setFilePath(file_path);
    if (load_path == autosave_path)
    {
        m_document.markDirty();
        appendLog(QStringLiteral("WARNING"), tr("已打开自动恢复版本，请检查后保存。"));
    }
    else
    {
        m_document.markSaved();
    }
    m_viewport->fitAll();
    appendLog(QStringLiteral("INFO"), tr("已打开项目：%1").arg(file_path));
}

bool SMainWindow::saveProject()
{
    if (m_document.filePath().isEmpty())
    {
        return saveProjectAs();
    }
    const auto result = m_project_codec.save(m_document, m_document.filePath());
    if (!result)
    {
        showFailure(tr("保存项目失败"), result);
        return false;
    }
    m_document.markSaved();
    QFile::remove(recoveryPath(m_document));
    appendLog(QStringLiteral("INFO"), tr("项目已保存：%1").arg(m_document.filePath()));
    return true;
}

bool SMainWindow::saveProjectAs()
{
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("保存 smartGraphics3D 项目"), m_document.projectName() + QStringLiteral(".sg3d"),
        projectFilter());
    if (file_path.isEmpty())
    {
        return false;
    }
    if (!file_path.endsWith(QStringLiteral(".sg3d"), Qt::CaseInsensitive))
    {
        file_path += QStringLiteral(".sg3d");
    }
    m_document.setFilePath(file_path);
    m_document.setProjectName(QFileInfo(file_path).completeBaseName());
    return saveProject();
}

void SMainWindow::importCad()
{
    const QString file_path =
        QFileDialog::getOpenFileName(this, tr("导入三维文件"), {}, cadFilter());
    if (file_path.isEmpty())
    {
        return;
    }
    const SImportOptions options = requestImportOptions(this, file_path, m_document.unitSystem());
    if (!options.accepted)
    {
        return;
    }

    auto result = std::make_shared<SResult<SImportedShape>>();
    const SStandardCadCodec codec = m_cad_codec;
    m_task_manager.run(
        tr("导入 %1").arg(QFileInfo(file_path).fileName()),
        [codec, result, file_path, options](const STaskContext& context)
        {
            context.reportProgress(15, QObject::tr("读取并转换几何"));
            *result = codec.read(file_path);
            if (!*result)
            {
                return SResult<void>::failure(result->errorCode(), result->message(),
                                              result->details());
            }
            if (std::abs(options.scale_to_millimeters - 1.0) > 1.0e-12)
            {
                context.reportProgress(65, QObject::tr("按确认单位缩放几何"));
                const auto kernel = createKernelService();
                STransformParameters transform;
                transform.uniform_scale = options.scale_to_millimeters;
                const auto scaled = kernel->transform(result->value().shape, transform);
                if (!scaled)
                {
                    return SResult<void>::failure(scaled.errorCode(), scaled.message(),
                                                  scaled.details());
                }
                result->value().shape = scaled.value();
                result->value().report.warnings.push_back(
                    QObject::tr("已按源单位 %1 实际缩放 %2 倍")
                        .arg(options.source_unit)
                        .arg(options.scale_to_millimeters, 0, 'g', 12));
            }
            const auto kernel = createKernelService();
            const auto metrics = kernel->measure(result->value().shape);
            if (metrics)
            {
                const QVector3D size = metrics.value().maximum - metrics.value().minimum;
                const double maximum_size = qMax(size.x(), qMax(size.y(), size.z()));
                if (maximum_size < 0.01 || maximum_size > 1.0e6)
                {
                    result->value().report.warnings.push_back(
                        QObject::tr("模型最大尺寸为 %1 mm，可能仍存在单位差异")
                            .arg(maximum_size, 0, 'g', 8));
                }
            }
            context.reportProgress(90, QObject::tr("等待加入项目"));
            return SResult<void>::success();
        },
        [this, result, file_path, options](const SResult<void>& completion)
        {
            if (!completion)
            {
                if (completion.errorCode() != SErrorCode::Cancelled)
                {
                    showResultFailure(this, tr("导入失败"), completion);
                }
                appendLog(QStringLiteral("ERROR"), tr("导入未提交：%1").arg(file_path),
                          completion.details());
                return;
            }
            SSceneObject object;
            object.name = result->value().suggested_name;
            object.shape = result->value().shape;
            const QString extension = QFileInfo(file_path).suffix();
            object.type = extension.compare(QStringLiteral("stl"), Qt::CaseInsensitive) == 0 ||
                                  extension.compare(QStringLiteral("obj"), Qt::CaseInsensitive) == 0
                              ? SObjectType::Mesh
                              : SObjectType::CadShape;
            object.stage = SDataStage::Original;
            object.source = tr("文件导入");
            object.external_path = QFileInfo(file_path).absoluteFilePath();
            object.external_reference = true;
            object.locked = true;
            object.custom_properties.insert(QStringLiteral("sourceUnit"), options.source_unit);
            object.custom_properties.insert(QStringLiteral("importScale"),
                                            options.scale_to_millimeters);
            object.custom_properties.insert(QStringLiteral("unitEmbedded"),
                                            options.unit_was_embedded);
            object.custom_properties.insert(
                QStringLiteral("compatibilityWarnings"),
                result->value().report.warnings.join(QStringLiteral("; ")));
            SCoordinateSystem imported_coordinates;
            imported_coordinates.name = tr("%1 CAD 坐标系").arg(object.name);
            imported_coordinates.source = tr("文件导入");
            if (!m_document.coordinateSystems().empty())
            {
                imported_coordinates.parent_id = m_document.coordinateSystems().front().id;
            }
            const QString parameter_summary = tr("格式=%1; 源单位=%2; 实际缩放=%3; 外部只读引用")
                                                  .arg(extension.toUpper(), options.source_unit)
                                                  .arg(options.scale_to_millimeters, 0, 'g', 12);
            const auto added =
                m_document.addImportedObject(std::move(object), std::move(imported_coordinates),
                                             tr("导入 CAD"), parameter_summary);
            if (!added)
            {
                showResultFailure(this, tr("导入失败"), added);
                return;
            }
            m_viewport->fitAll();
            appendLog(QStringLiteral("INFO"), tr("已导入：%1").arg(file_path),
                      result->value().report.warnings.join(QStringLiteral("; ")));
        });
}

void SMainWindow::archiveProject()
{
    const QString parent_path = QFileDialog::getExistingDirectory(this, tr("选择项目归档保存位置"));
    if (parent_path.isEmpty())
    {
        return;
    }
    const QString archive_path =
        QDir(parent_path)
            .filePath(QStringLiteral("%1-archive-%2")
                          .arg(m_document.projectName(), QDateTime::currentDateTime().toString(
                                                             QStringLiteral("yyyyMMdd-HHmmss"))));
    auto snapshot = std::make_shared<S3dDocument>();
    snapshot->replaceAll(m_document.projectId(), m_document.projectName(), m_document.objects(),
                         m_document.history(), m_document.unitSystem(),
                         m_document.coordinateSystems(), m_document.snapshots());
    const SProjectCodec codec = m_project_codec;
    m_task_manager.run(
        tr("归档项目"),
        [codec, snapshot, archive_path](const STaskContext& context)
        {
            context.reportProgress(10, QObject::tr("保存项目与依赖"));
            const auto result = codec.createArchive(*snapshot, archive_path);
            if (!result)
            {
                return result;
            }
            context.reportProgress(95, QObject::tr("提交归档"));
            return SResult<void>::success();
        },
        [this, archive_path](const SResult<void>& result)
        {
            if (!result)
            {
                if (result.errorCode() != SErrorCode::Cancelled)
                {
                    showFailure(tr("项目归档失败"), result);
                }
                return;
            }
            appendLog(QStringLiteral("INFO"), tr("项目归档已创建：%1").arg(archive_path));
        });
}

void SMainWindow::autoSave()
{
    if (!m_document.isDirty())
    {
        return;
    }
    const QString file_path = recoveryPath(m_document);
    QDir().mkpath(QFileInfo(file_path).absolutePath());
    queueProjectSave(file_path, false, tr("自动保存恢复版本"));
}

void SMainWindow::checkRecovery()
{
    const QString file_path = recoveryPath(m_document);
    if (!QFileInfo::exists(file_path))
    {
        return;
    }
    if (QMessageBox::question(
            this, tr("恢复未保存项目"), tr("发现上次会话的未命名自动恢复项目，是否打开？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
    {
        const auto result = m_project_codec.load(m_document, file_path);
        if (!result)
        {
            showFailure(tr("恢复失败"), result);
        }
    }
}

SResult<SObjectId> SMainWindow::addShape(const SKernelShape& shape, const QString& name,
                                         SDataStage stage, const QString& source,
                                         const QString& parameter_summary)
{
    SSceneObject object;
    object.name = name;
    object.shape = shape;
    object.stage = stage;
    object.source = source.isEmpty() ? tr("smartGraphics3D 创建") : source;
    return m_document.addObject(std::move(object), tr("创建%1").arg(name), parameter_summary);
}

void SMainWindow::addDerivedShape(const SKernelShape& shape, const QString& name,
                                  const QList<SObjectId>& inputs, const QString& operation,
                                  bool replace_inputs)
{
    SSceneObject object;
    object.name = name;
    object.shape = shape;
    object.source = operation;
    const auto result =
        m_document.addDerivedObject(inputs, std::move(object), operation, replace_inputs);
    if (!result)
    {
        showResultFailure(this, operation, result);
        return;
    }
    m_viewport->selectObject(result.value());
}

void SMainWindow::runBoolean(SBooleanOperation operation)
{
    const QList<SObjectId> ids = selectedObjectIds();
    if (ids.size() != 2)
    {
        QMessageBox::information(this, tr("布尔运算"), tr("请按顺序选择两个实体。"));
        return;
    }
    const SSceneObject* first = m_document.findObject(ids.at(0));
    const SSceneObject* second = m_document.findObject(ids.at(1));
    if (!first || !second)
    {
        return;
    }
    const QString operation_name = operation == SBooleanOperation::Union        ? tr("布尔并集")
                                   : operation == SBooleanOperation::Difference ? tr("布尔差集")
                                                                                : tr("布尔交集");
    const auto first_materialized = materializedShape(*first);
    const auto second_materialized = materializedShape(*second);
    if (!first_materialized || !second_materialized)
    {
        const auto& failure = !first_materialized ? first_materialized : second_materialized;
        showFailure(operation_name, SResult<void>::failure(failure.errorCode(), failure.message(),
                                                           failure.details()));
        return;
    }
    const SKernelShape first_shape = first_materialized.value();
    const SKernelShape second_shape = second_materialized.value();
    runShapeTask(operation_name, ids, operation_name, tr("输入对象=2; 运算=%1").arg(operation_name),
                 [first_shape, second_shape, operation](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->booleanOperation(first_shape, second_shape, operation);
                 });
}

void SMainWindow::runFillet()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("圆角"), tr("请选择一个实体。"));
        return;
    }
    double radius = 2.0;
    if (!requestDouble(this, tr("全边圆角"), tr("半径 (mm)"), radius, 0.001, 1.0e6, radius))
    {
        return;
    }
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("圆角失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape shape = materialized.value();
    const SObjectId id = object->id;
    const QString result_name = object->name + tr(" 圆角");
    runShapeTask(tr("全边圆角"), {id}, result_name, tr("半径=%1 mm").arg(radius, 0, 'g', 12),
                 [shape, radius](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->filletAllEdges(shape, radius);
                 });
}

void SMainWindow::runChamfer()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("倒角"), tr("请选择一个实体。"));
        return;
    }
    double distance = 2.0;
    if (!requestDouble(this, tr("全边倒角"), tr("距离 (mm)"), distance, 0.001, 1.0e6, distance))
    {
        return;
    }
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("倒角失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape shape = materialized.value();
    const SObjectId id = object->id;
    const QString result_name = object->name + tr(" 倒角");
    runShapeTask(tr("全边倒角"), {id}, result_name, tr("距离=%1 mm").arg(distance, 0, 'g', 12),
                 [shape, distance](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->chamferAllEdges(shape, distance);
                 });
}

void SMainWindow::runHole()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("孔"), tr("请选择一个实体。"));
        return;
    }
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("孔创建失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    SHoleParameters parameters;
    const auto metrics = m_kernel->measure(materialized.value());
    if (metrics)
    {
        parameters.x = (metrics.value().minimum.x() + metrics.value().maximum.x()) * 0.5;
        parameters.y = (metrics.value().minimum.y() + metrics.value().maximum.y()) * 0.5;
    }
    if (!requestDouble(this, tr("孔"), tr("中心 X (mm)"), parameters.x, -1.0e9, 1.0e9,
                       parameters.x) ||
        !requestDouble(this, tr("孔"), tr("中心 Y (mm)"), parameters.y, -1.0e9, 1.0e9,
                       parameters.y) ||
        !requestDouble(this, tr("孔"), tr("直径 (mm)"), parameters.diameter, 0.001, 1.0e9,
                       parameters.diameter))
    {
        return;
    }
    parameters.through_all = QMessageBox::question(this, tr("孔类型"), tr("创建贯穿孔？"),
                                                   QMessageBox::Yes | QMessageBox::No,
                                                   QMessageBox::Yes) == QMessageBox::Yes;
    if (!parameters.through_all && !requestDouble(this, tr("盲孔"), tr("深度 (mm)"),
                                                  parameters.depth, 0.001, 1.0e9, parameters.depth))
    {
        return;
    }
    const SKernelShape shape = materialized.value();
    const SObjectId id = object->id;
    const QString result_name = object->name + tr(" 孔");
    runShapeTask(tr("创建孔"), {id}, result_name,
                 tr("中心=(%1, %2) mm; 直径=%3 mm; %4")
                     .arg(parameters.x, 0, 'g', 12)
                     .arg(parameters.y, 0, 'g', 12)
                     .arg(parameters.diameter, 0, 'g', 12)
                     .arg(parameters.through_all
                              ? tr("贯穿")
                              : tr("深度=%1 mm").arg(parameters.depth, 0, 'g', 12)),
                 [shape, parameters](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->makeHole(shape, parameters);
                 });
}

void SMainWindow::runMirror()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("镜像"), tr("请选择一个实体。"));
        return;
    }
    bool accepted = false;
    const QString axis =
        QInputDialog::getItem(this, tr("镜像"), tr("镜像平面"),
                              {tr("YZ 平面"), tr("XZ 平面"), tr("XY 平面")}, 0, false, &accepted);
    if (!accepted)
    {
        return;
    }
    const QVector3D normal = axis == tr("YZ 平面")   ? QVector3D(1.0F, 0.0F, 0.0F)
                             : axis == tr("XZ 平面") ? QVector3D(0.0F, 1.0F, 0.0F)
                                                     : QVector3D(0.0F, 0.0F, 1.0F);
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("镜像失败"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape shape = materialized.value();
    const SObjectId id = object->id;
    const QString result_name = object->name + tr(" 镜像");
    runShapeTask(tr("镜像"), {id}, result_name, tr("镜像平面=%1").arg(axis),
                 [shape, normal](const STaskContext&)
                 {
                     const auto kernel = createKernelService();
                     return kernel->mirror(shape, normal);
                 });
}

} // namespace smartGraphics3D
