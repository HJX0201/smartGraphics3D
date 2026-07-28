#include "s_import_options.h"

#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <cmath>

namespace smartGraphics3D
{
namespace
{
SLengthUnit unitFromIndex(int index)
{
    switch (index)
    {
    case 1:
        return SLengthUnit::Centimeter;
    case 2:
        return SLengthUnit::Meter;
    case 3:
        return SLengthUnit::Inch;
    default:
        return SLengthUnit::Millimeter;
    }
}
} // namespace

SImportOptions requestImportOptions(QWidget* parent, const QString& file_path,
                                    const SUnitSystem& project_units)
{
    SImportOptions result;
    const QString extension = QFileInfo(file_path).suffix().toLower();
    if (extension == QStringLiteral("step") || extension == QStringLiteral("stp") ||
        extension == QStringLiteral("iges") || extension == QStringLiteral("igs"))
    {
        result.unit_was_embedded = true;
        result.source_unit = QObject::tr("文件内嵌单位");
        return result;
    }

    const QStringList names = {QObject::tr("毫米 (mm)"), QObject::tr("厘米 (cm)"),
                               QObject::tr("米 (m)"), QObject::tr("英寸 (in)")};
    bool accepted = false;
    const QString selected = QInputDialog::getItem(
        parent, QObject::tr("确认导入单位"),
        QObject::tr("%1 不可靠地保存长度单位。\n请选择文件数值实际使用的单位：")
            .arg(extension.toUpper()),
        names, static_cast<int>(project_units.lengthUnit()), false, &accepted);
    if (!accepted)
    {
        result.accepted = false;
        return result;
    }

    SUnitSystem source_units;
    source_units.setLengthUnit(unitFromIndex(names.indexOf(selected)));
    result.scale_to_millimeters = source_units.toMillimeters(1.0);
    result.source_unit = source_units.lengthSuffix();
    if (std::abs(result.scale_to_millimeters - 1.0) > 1.0e-12)
    {
        const auto answer = QMessageBox::question(
            parent, QObject::tr("确认实际缩放"),
            QObject::tr("导入时会把几何坐标实际乘以 %1，转换为内部毫米单位。\n"
                        "这会改变模型真实尺寸，是否继续？")
                .arg(result.scale_to_millimeters, 0, 'g', 12),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        result.accepted = answer == QMessageBox::Yes;
    }
    return result;
}
} // namespace smartGraphics3D
