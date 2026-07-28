#include "s_project_codec.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

namespace smartGraphics3D
{
SResult<void> SProjectCodec::createArchive(const S3dDocument& document,
                                           const QString& directory_path) const
{
    const QFileInfo target_info(directory_path);
    if (target_info.exists())
    {
        return SResult<void>::failure(
            SErrorCode::FileFailure, QObject::tr("归档目录已存在"),
            QObject::tr("请选择一个新的归档目录，避免覆盖已有交付数据。"));
    }
    QDir parent = target_info.dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral(".")))
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("无法创建归档父目录"));
    }
    QTemporaryDir temporary(parent.filePath(QStringLiteral(".sg3d-archive-XXXXXX")));
    if (!temporary.isValid())
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("无法创建临时归档目录"));
    }

    QDir root(temporary.path());
    root.mkpath(QStringLiteral("dependencies"));
    const QString project_name =
        document.projectName().isEmpty() ? QStringLiteral("project") : document.projectName();
    const QString project_path = root.filePath(project_name + QStringLiteral(".sg3d"));
    const auto saved = save(document, project_path);
    if (!saved)
    {
        return saved;
    }

    QJsonArray dependencies;
    QSet<QString> used_names;
    for (const SSceneObject& object : document.objects())
    {
        if (!object.external_reference || object.external_path.isEmpty())
        {
            continue;
        }
        const QFileInfo source(object.external_path);
        QJsonObject entry;
        entry.insert(QStringLiteral("objectId"), object.id.toString(QUuid::WithoutBraces));
        entry.insert(QStringLiteral("originalPath"), source.absoluteFilePath());
        entry.insert(QStringLiteral("available"), source.exists());
        if (source.exists())
        {
            QString name = source.fileName();
            int suffix = 2;
            while (used_names.contains(name.toLower()))
            {
                name = QStringLiteral("%1-%2.%3")
                           .arg(source.completeBaseName())
                           .arg(suffix++)
                           .arg(source.suffix());
            }
            used_names.insert(name.toLower());
            const QString relative = QStringLiteral("dependencies/%1").arg(name);
            if (!QFile::copy(source.absoluteFilePath(), root.filePath(relative)))
            {
                return SResult<void>::failure(SErrorCode::FileFailure,
                                              QObject::tr("复制归档依赖失败"),
                                              source.absoluteFilePath());
            }
            entry.insert(QStringLiteral("archivedPath"), relative);
        }
        dependencies.push_back(entry);
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("format"), QStringLiteral("smartGraphics3DArchive"));
    manifest.insert(QStringLiteral("version"), 1);
    manifest.insert(QStringLiteral("projectFile"), QFileInfo(project_path).fileName());
    manifest.insert(QStringLiteral("createdAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    manifest.insert(QStringLiteral("dependencies"), dependencies);
    QSaveFile manifest_file(root.filePath(QStringLiteral("manifest.json")));
    if (!manifest_file.open(QIODevice::WriteOnly) ||
        manifest_file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0 ||
        !manifest_file.commit())
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("写入归档清单失败"),
                                      manifest_file.errorString());
    }

    temporary.setAutoRemove(false);
    if (!parent.rename(QFileInfo(temporary.path()).fileName(), target_info.fileName()))
    {
        temporary.setAutoRemove(true);
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("提交归档目录失败"));
    }
    return SResult<void>::success();
}
} // namespace smartGraphics3D
