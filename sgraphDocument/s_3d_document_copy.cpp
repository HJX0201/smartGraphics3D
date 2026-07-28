#include "s_3d_document.h"
#include "s_transform_utils.h"

namespace smartGraphics3D
{
SResult<QList<SObjectId>> S3dDocument::copyObjects(const QList<SObjectId>& ids, SCopyMode mode,
                                                   QString operation_name,
                                                   QString parameter_summary)
{
    if (ids.isEmpty())
    {
        return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                  tr("请选择需要复制的对象"));
    }

    QList<SSceneObject> copies;
    for (const SObjectId& id : ids)
    {
        const SSceneObject* source = findObject(id);
        if (!source || source->shape.isNull())
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::NotFound,
                                                      tr("复制源对象不存在或没有几何"));
        }

        SSceneObject copy = *source;
        copy.id = SObjectId::createUuid();
        copy.name += mode == SCopyMode::SharedPresentation ? tr(" 实例") : tr(" 副本");
        copy.stage = SDataStage::Working;
        copy.locked = false;
        copy.external_reference = false;
        copy.external_path.clear();
        copy.source = mode == SCopyMode::SharedPresentation ? tr("实例复制") : tr("普通复制");
        copy.derived_from = {source->id};
        copy.created_at = QDateTime::currentDateTimeUtc();
        copy.modified_at = copy.created_at;
        if (mode == SCopyMode::IndependentPresentation)
        {
            copy.presentation_group_id = QUuid::createUuid();
        }
        copies.push_back(std::move(copy));
    }

    if (operation_name.trimmed().isEmpty())
    {
        operation_name = mode == SCopyMode::SharedPresentation ? tr("实例复制") : tr("普通复制");
    }
    return addObjects(std::move(copies), std::move(operation_name), std::move(parameter_summary));
}

SResult<void> S3dDocument::setObjectTransform(const SObjectId& id, const QMatrix4x4& transform,
                                              QString operation_name)
{
    if (!isSimilarityTransform(transform))
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("对象变换矩阵无效"));
    }
    const SSceneObject* current = findObject(id);
    if (!current)
    {
        return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
    }
    if (current->locked)
    {
        return SResult<void>::failure(SErrorCode::Locked, tr("锁定对象不能变换"));
    }
    if (current->transform == transform)
    {
        return SResult<void>::success();
    }
    if (operation_name.trimmed().isEmpty())
    {
        operation_name = tr("更新实例变换");
    }
    return commit(std::move(operation_name),
                  [this, id, transform]()
                  {
                      SSceneObject* object = findObject(id);
                      if (!object)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      object->transform = transform;
                      object->modified_at = QDateTime::currentDateTimeUtc();
                      return SResult<void>::success();
                  },
                  {id});
}

int S3dDocument::presentationGroupMemberCount(const QUuid& group_id) const
{
    if (group_id.isNull())
    {
        return 0;
    }
    int count = 0;
    for (const SSceneObject& object : m_objects)
    {
        if (object.presentation_group_id == group_id && !object.shape.isNull())
        {
            ++count;
        }
    }
    return count;
}
} // namespace smartGraphics3D
