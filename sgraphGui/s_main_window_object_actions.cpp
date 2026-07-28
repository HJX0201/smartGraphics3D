#include "s_main_window.h"

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
