#include "s_main_window.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

namespace smartGraphics3D
{
namespace
{
QString recoveryFilePath(const S3dDocument& document)
{
    if (!document.filePath().isEmpty())
    {
        return document.filePath() + QStringLiteral(".autosave");
    }
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(directory).filePath(QStringLiteral("untitled.sg3d.autosave"));
}
} // namespace

void SMainWindow::saveProjectInBackground()
{
    if (m_document.filePath().isEmpty())
    {
        saveProjectAsInBackground();
        return;
    }
    queueProjectSave(m_document.filePath(), true, tr("保存项目"));
}

void SMainWindow::saveProjectAsInBackground()
{
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("保存 smartGraphics3D 项目"), m_document.projectName() + QStringLiteral(".sg3d"),
        tr("smartGraphics3D 项目 (*.sg3d)"));
    if (file_path.isEmpty())
    {
        return;
    }
    if (!file_path.endsWith(QStringLiteral(".sg3d"), Qt::CaseInsensitive))
    {
        file_path += QStringLiteral(".sg3d");
    }
    m_document.setFilePath(file_path);
    m_document.setProjectName(QFileInfo(file_path).completeBaseName());
    queueProjectSave(file_path, true, tr("项目另存为"));
}

void SMainWindow::queueProjectSave(const QString& file_path, bool mark_saved,
                                   const QString& task_name)
{
    const QUuid project_id = m_document.projectId();
    const QString project_name = m_document.projectName();
    const std::vector<SSceneObject> objects = m_document.objects();
    const QList<SOperationRecord> history = m_document.history();
    const SUnitSystem units = m_document.unitSystem();
    const std::vector<SCoordinateSystem> coordinates = m_document.coordinateSystems();
    const QList<SSnapshotRecord> snapshots = m_document.snapshots();
    const quint64 revision = m_document.revision();
    const SProjectCodec codec = m_project_codec;

    m_task_manager.run(
        task_name,
        [codec, project_id, project_name, objects, history, units, coordinates, snapshots,
         file_path](const STaskContext& context)
        {
            context.reportProgress(10, QObject::tr("创建一致性快照"));
            if (context.isCancellationRequested())
            {
                return SResult<void>::failure(SErrorCode::Cancelled, QObject::tr("保存已取消"));
            }
            S3dDocument snapshot;
            snapshot.replaceAll(project_id, project_name, objects, history, units, coordinates,
                                snapshots);
            context.reportProgress(35, QObject::tr("原子写入项目"));
            const SResult<void> result = codec.save(snapshot, file_path);
            if (!result)
            {
                return result;
            }
            context.reportProgress(95, QObject::tr("验证保存结果"));
            return SResult<void>::success();
        },
        [this, file_path, revision, mark_saved](const SResult<void>& result)
        {
            if (!result)
            {
                if (result.errorCode() != SErrorCode::Cancelled)
                {
                    showFailure(tr("保存项目失败"), result);
                }
                return;
            }
            if (mark_saved && m_document.filePath() == file_path &&
                m_document.revision() == revision)
            {
                m_document.markSaved();
                QFile::remove(recoveryFilePath(m_document));
            }
            appendLog(QStringLiteral("INFO"), mark_saved
                                                  ? tr("项目快照已保存：%1").arg(file_path)
                                                  : tr("自动恢复版本已保存：%1").arg(file_path));
        });
}
} // namespace smartGraphics3D
