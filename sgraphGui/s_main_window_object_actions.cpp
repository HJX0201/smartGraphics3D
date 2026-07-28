#include "s_main_window.h"

#include <QColorDialog>
#include <QMessageBox>

namespace smartGraphics3D
{
void SMainWindow::duplicateSelectionWithMode(SCopyMode mode)
{
    const QList<SObjectId> ids = selectedObjectIds();
    QList<SKernelShape> shapes;
    for (const SObjectId& id : ids)
    {
        const SSceneObject* object = m_document.findObject(id);
        if (!object || object->shape.isNull())
        {
            continue;
        }
        const auto materialized = materializedShape(*object);
        if (!materialized)
        {
            showFailure(tr("复制预览失败"),
                        SResult<void>::failure(materialized.errorCode(), materialized.message(),
                                               materialized.details()));
            return;
        }
        shapes.push_back(materialized.value());
    }
    if (shapes.isEmpty())
    {
        return;
    }
    const QString operation =
        mode == SCopyMode::SharedPresentation ? tr("实例复制") : tr("普通复制");
    const auto preview = m_kernel->makeCompound(shapes);
    if (!preview)
    {
        showFailure(
            tr("复制预览失败"),
            SResult<void>::failure(preview.errorCode(), preview.message(), preview.details()));
        return;
    }
    bool replace_inputs = false;
    if (!confirmShapePreview(preview.value(), operation, {}, replace_inputs))
    {
        return;
    }
    const auto committed = m_document.copyObjects(ids, mode, operation);
    if (!committed)
    {
        showFailure(tr("复制失败"),
                    SResult<void>::failure(committed.errorCode(), committed.message(),
                                           committed.details()));
    }
}

void SMainWindow::duplicateSelection()
{
    duplicateSelectionWithMode(SCopyMode::IndependentPresentation);
}

void SMainWindow::duplicateSelectionShared()
{
    duplicateSelectionWithMode(SCopyMode::SharedPresentation);
}

void SMainWindow::setSelectionColor()
{
    QList<SObjectId> ids;
    QColor initial_color(205, 228, 238);
    for (const SObjectId& id : selectedObjectIds())
    {
        const SSceneObject* object = m_document.findObject(id);
        if (object && !object->shape.isNull())
        {
            if (ids.isEmpty())
            {
                initial_color = object->display.color;
            }
            ids.push_back(id);
        }
    }
    if (ids.isEmpty())
    {
        return;
    }

    const QColor color = QColorDialog::getColor(initial_color, this, tr("设置对象颜色"));
    if (!color.isValid())
    {
        return;
    }
    const auto result = m_document.setObjectColors(ids, color);
    if (!result)
    {
        showFailure(tr("设置颜色失败"), result);
    }
}

void SMainWindow::restoreSelectionImportedColors()
{
    QList<SObjectId> ids;
    for (const SObjectId& id : selectedObjectIds())
    {
        const SSceneObject* object = m_document.findObject(id);
        if (object && object->imported_appearance.valid)
        {
            ids.push_back(id);
        }
    }
    if (ids.isEmpty())
    {
        return;
    }
    const auto result = m_document.restoreImportedAppearances(ids);
    if (!result)
    {
        showFailure(tr("恢复导入颜色失败"), result);
    }
}

void SMainWindow::deleteSelection()
{
    const QList<SObjectId> ids = selectedObjectIds();
    if (ids.isEmpty())
    {
        return;
    }
    if (QMessageBox::question(this, tr("删除对象"),
                              tr("确定删除选中的 %1 个对象？锁定对象不会被删除。").arg(ids.size()),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    const auto result = m_document.removeObjects(ids);
    if (!result)
    {
        showFailure(tr("删除失败"), result);
    }
}
} // namespace smartGraphics3D
