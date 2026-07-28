#include "s_main_window.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

namespace smartGraphics3D
{
namespace
{
QString cadExportFilter()
{
    return QObject::tr("三维文件 (*.step *.stp *.iges *.igs *.brep *.stl *.obj);;"
                       "STEP (*.step *.stp);;IGES (*.iges *.igs);;BREP (*.brep);;"
                       "STL (*.stl);;OBJ (*.obj)");
}

template <typename T>
void showExportFailure(QWidget* parent, const QString& action, const SResult<T>& result)
{
    QMessageBox box(QMessageBox::Critical, action,
                    QObject::tr("%1\n\n可能原因：%2")
                        .arg(result.message(), result.details().isEmpty()
                                                   ? QObject::tr("输入参数或几何条件不满足")
                                                   : result.details()),
                    QMessageBox::Ok, parent);
    box.setInformativeText(QObject::tr("项目未被修改。请调整参数后重试。"));
    box.setDetailedText(
        QObject::tr("错误码：%1\n诊断详情：%2")
            .arg(static_cast<int>(result.errorCode()))
            .arg(result.details().isEmpty() ? QObject::tr("无") : result.details()));
    box.exec();
}
} // namespace

void SMainWindow::exportSelected()
{
    const SSceneObject* object = singleSelectedObject();
    if (!object)
    {
        QMessageBox::information(this, tr("导出"), tr("请只选择一个要导出的几何对象。"));
        return;
    }
    const QString file_path = QFileDialog::getSaveFileName(
        this, tr("导出选中对象"), object->name + QStringLiteral(".step"), cadExportFilter());
    if (file_path.isEmpty())
    {
        return;
    }
    const auto preflight = m_cad_codec.compatibilityReport(file_path);
    if (!preflight)
    {
        appendLog(QStringLiteral("ERROR"), tr("导出预检查失败：%1").arg(file_path),
                  preflight.details(), preflight.errorCode());
        showExportFailure(this, tr("无法导出"), preflight);
        return;
    }
    QStringList compatibility_lines;
    if (!preflight.value().lost_properties.isEmpty())
    {
        compatibility_lines.push_back(
            tr("不会保留：%1").arg(preflight.value().lost_properties.join(QStringLiteral("、"))));
    }
    compatibility_lines.append(preflight.value().warnings);
    if (!compatibility_lines.isEmpty() &&
        QMessageBox::warning(
            this, tr("确认导出兼容性"),
            tr("目标格式：%1\n%2\n\n是否继续导出？")
                .arg(preflight.value().format, compatibility_lines.join(QStringLiteral("\n"))),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
    {
        return;
    }
    const auto materialized = materializedShape(*object);
    if (!materialized)
    {
        showFailure(tr("无法导出"),
                    SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                           materialized.details()));
        return;
    }
    const SKernelShape shape = materialized.value();
    auto result = std::make_shared<SResult<SFileCompatibilityReport>>();
    const SStandardCadCodec codec = m_cad_codec;
    m_task_manager.run(
        tr("导出 %1").arg(QFileInfo(file_path).fileName()),
        [codec, result, shape, file_path](const STaskContext& context)
        {
            context.reportProgress(15, QObject::tr("转换并写入几何"));
            *result = codec.write(shape, file_path);
            if (!*result)
            {
                return SResult<void>::failure(result->errorCode(), result->message(),
                                              result->details());
            }
            context.reportProgress(90, QObject::tr("完成文件写入"));
            return SResult<void>::success();
        },
        [this, result, file_path](const SResult<void>& completion)
        {
            if (!completion)
            {
                if (completion.errorCode() != SErrorCode::Cancelled)
                {
                    showExportFailure(this, tr("导出失败"), completion);
                }
                return;
            }
            QString message = tr("导出完成：%1").arg(file_path);
            if (!result->value().lost_properties.isEmpty())
            {
                message += tr("\n未保留：%1")
                               .arg(result->value().lost_properties.join(QStringLiteral("、")));
            }
            appendLog(QStringLiteral("INFO"), message,
                      result->value().warnings.join(QStringLiteral("; ")));
            QMessageBox::information(this, tr("导出完成"), message);
        });
}
} // namespace smartGraphics3D
