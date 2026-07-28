#include "s_main_window.h"
#include "s_occ_viewport.h"

#include <QDateTime>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextEdit>
#include <algorithm>

namespace smartGraphics3D
{
void SMainWindow::createSnapshot()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("创建快照"), tr("快照名称"), QLineEdit::Normal,
        tr("快照 %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))),
        &accepted);
    if (!accepted)
    {
        return;
    }
    const auto result = m_document.createSnapshot(name);
    if (!result)
    {
        showFailure(tr("创建快照失败"), result);
    }
    else
    {
        appendLog(QStringLiteral("INFO"), tr("已创建快照：%1").arg(name));
    }
}

void SMainWindow::restoreSnapshot()
{
    if (m_document.snapshotNames().isEmpty())
    {
        QMessageBox::information(this, tr("恢复快照"), tr("当前项目没有命名快照。"));
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getItem(this, tr("恢复快照"), tr("选择快照"),
                                               m_document.snapshotNames(), 0, false, &accepted);
    if (!accepted)
    {
        return;
    }
    const auto result = m_document.restoreSnapshot(name);
    if (!result)
    {
        showFailure(tr("恢复快照失败"), result);
    }
}

void SMainWindow::saveSnapshotBranch()
{
    const QList<SSnapshotRecord> snapshots = m_document.snapshots();
    if (snapshots.isEmpty())
    {
        QMessageBox::information(this, tr("快照另存分支"), tr("当前项目没有可另存的命名快照。"));
        return;
    }
    QStringList names;
    for (const SSnapshotRecord& snapshot : snapshots)
    {
        names.push_back(snapshot.name);
    }
    bool accepted = false;
    const QString selected =
        QInputDialog::getItem(this, tr("快照另存分支"), tr("选择快照"), names, 0, false, &accepted);
    if (!accepted)
    {
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("保存项目分支"),
        QStringLiteral("%1-%2.sg3d").arg(m_document.projectName(), selected),
        tr("smartGraphics3D 项目 (*.sg3d)"));
    if (path.isEmpty())
    {
        return;
    }
    if (!path.endsWith(QStringLiteral(".sg3d"), Qt::CaseInsensitive))
    {
        path += QStringLiteral(".sg3d");
    }
    const auto iterator = std::find_if(snapshots.cbegin(), snapshots.cend(),
                                       [&selected](const SSnapshotRecord& snapshot)
                                       {
                                           return snapshot.name == selected;
                                       });
    if (iterator == snapshots.cend())
    {
        return;
    }
    const SSnapshotRecord snapshot = *iterator;
    const SProjectCodec codec = m_project_codec;
    const QString branch_name = QStringLiteral("%1-%2").arg(m_document.projectName(), selected);
    m_task_manager.run(
        tr("保存快照分支"),
        [codec, snapshot, branch_name, path](const STaskContext& context)
        {
            context.reportProgress(15, QObject::tr("构建分支快照"));
            S3dDocument branch;
            branch.replaceAll(QUuid::createUuid(), branch_name, snapshot.objects, {},
                              snapshot.units, snapshot.coordinate_systems, {});
            context.reportProgress(45, QObject::tr("原子写入分支"));
            return codec.save(branch, path);
        },
        [this, path](const SResult<void>& result)
        {
            if (!result)
            {
                showFailure(tr("保存分支失败"), result);
                return;
            }
            appendLog(QStringLiteral("INFO"), tr("快照分支已保存：%1").arg(path));
        });
}

void SMainWindow::executeCommand(const QString& command)
{
    const QString normalized = command.trimmed().toUpper();
    if (normalized == QStringLiteral("HELP"))
    {
        m_console->append(tr("命令：BOX、CYLINDER、SPHERE、FIT、UNDO、REDO、SAVE、OPEN、IMPORT"));
    }
    else if (SICommand* registered = m_command_registry.command(normalized))
    {
        SCommandContext context;
        context.document = &m_document;
        const SResult<void> result = registered->begin(context);
        if (result)
        {
            registered->confirm();
        }
        else
        {
            showFailure(tr("执行命令失败"), result);
        }
    }
    else
    {
        m_console->append(tr("未知命令：%1。输入 HELP 查看可用命令。").arg(command));
    }
}
} // namespace smartGraphics3D
