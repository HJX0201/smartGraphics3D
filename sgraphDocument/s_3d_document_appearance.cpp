#include "s_3d_document.h"

#include <QSet>

namespace smartGraphics3D
{
SResult<void> S3dDocument::setObjectColors(const QList<SObjectId>& ids, const QColor& color)
{
    if (ids.isEmpty() || !color.isValid())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("请选择对象和有效颜色"));
    }

    QSet<SObjectId> unique_ids;
    for (const SObjectId& id : ids)
    {
        const SSceneObject* object = findObject(id);
        if (!object)
        {
            return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
        }
        if (object->shape.isNull())
        {
            return SResult<void>::failure(SErrorCode::InvalidArgument,
                                          tr("对象没有可设置颜色的几何"));
        }
        unique_ids.insert(id);
    }

    const QList<SObjectId> affected_ids = unique_ids.values();
    return commit(
        tr("设置对象颜色"),
        [this, affected_ids, color]()
        {
            for (const SObjectId& id : affected_ids)
            {
                SSceneObject* object = findObject(id);
                if (!object)
                {
                    return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                }
                object->display.color = color;
                object->use_imported_appearance = false;
                object->modified_at = QDateTime::currentDateTimeUtc();
            }
            return SResult<void>::success();
        },
        affected_ids);
}

SResult<void> S3dDocument::restoreImportedAppearances(const QList<SObjectId>& ids)
{
    if (ids.isEmpty())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("请选择要恢复颜色的对象"));
    }

    QSet<SObjectId> unique_ids;
    for (const SObjectId& id : ids)
    {
        const SSceneObject* object = findObject(id);
        if (!object)
        {
            return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
        }
        if (!object->imported_appearance.valid)
        {
            return SResult<void>::failure(SErrorCode::InvalidArgument,
                                          tr("所选对象没有可恢复的导入颜色"));
        }
        unique_ids.insert(id);
    }

    const QList<SObjectId> affected_ids = unique_ids.values();
    return commit(
        tr("恢复导入颜色"),
        [this, affected_ids]()
        {
            for (const SObjectId& id : affected_ids)
            {
                SSceneObject* object = findObject(id);
                if (!object)
                {
                    return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                }
                object->display.color = object->imported_appearance.fallback_style.color;
                object->display.transparency =
                    object->imported_appearance.fallback_style.transparency;
                object->use_imported_appearance = true;
                object->modified_at = QDateTime::currentDateTimeUtc();
            }
            return SResult<void>::success();
        },
        affected_ids);
}
} // namespace smartGraphics3D
