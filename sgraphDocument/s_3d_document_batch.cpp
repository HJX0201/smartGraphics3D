#include "s_3d_document.h"
#include "s_document_transaction.h"

#include <QSet>
#include <algorithm>

namespace smartGraphics3D
{
SResult<QList<SObjectId>> S3dDocument::addObjects(QList<SSceneObject> objects,
                                                  QString operation_name, QString parameter_summary)
{
    if (objects.isEmpty())
    {
        return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                  tr("批量创建对象不能为空"));
    }

    QSet<SObjectId> ids;
    QList<SObjectId> added_ids;
    for (SSceneObject& object : objects)
    {
        object.name = object.name.trimmed();
        if (object.id.isNull() || findObject(object.id) || ids.contains(object.id))
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::Conflict,
                                                      tr("批量创建包含空 ID 或重复对象 ID"));
        }
        if (object.name.isEmpty())
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                      tr("对象名称不能为空"));
        }
        if (object.shape.isNull() && object.type != SObjectType::Group &&
            object.type != SObjectType::Measurement && object.type != SObjectType::CoordinateSystem)
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                      tr("批量创建包含空几何对象"));
        }
        if (object.coordinate_system_id.isNull() && !m_coordinate_systems.empty())
        {
            object.coordinate_system_id = m_coordinate_systems.front().id;
        }
        const bool coordinate_exists =
            std::any_of(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                        [&object](const SCoordinateSystem& candidate)
                        {
                            return candidate.id == object.coordinate_system_id;
                        });
        if (!coordinate_exists)
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::NotFound,
                                                      tr("对象引用的坐标系不存在"));
        }
        if (object.stage == SDataStage::Original)
        {
            object.locked = true;
        }
        ids.insert(object.id);
        added_ids.push_back(object.id);
    }

    SDocumentTransaction transaction(*this, std::move(operation_name), added_ids,
                                     std::move(parameter_summary));
    const SResult<void> committed = transaction.commit(
        [this, objects = std::move(objects)]() mutable
        {
            for (SSceneObject& object : objects)
            {
                m_objects.push_back(std::move(object));
            }
            return SResult<void>::success();
        });
    if (!committed)
    {
        return SResult<QList<SObjectId>>::failure(committed.errorCode(), committed.message(),
                                                  committed.details());
    }
    return SResult<QList<SObjectId>>::success(added_ids);
}

SResult<SObjectId> S3dDocument::addImportedObject(SSceneObject object,
                                                  SCoordinateSystem coordinate_system,
                                                  QString operation_name, QString parameter_summary)
{
    object.name = object.name.trimmed();
    coordinate_system.name = coordinate_system.name.trimmed();
    if (object.id.isNull() || findObject(object.id) || object.name.isEmpty() ||
        object.shape.isNull())
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument,
                                           tr("导入对象包含无效名称、ID 或空几何"));
    }
    if (coordinate_system.id.isNull() || coordinate_system.name.isEmpty())
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument,
                                           tr("导入坐标系包含无效名称或 ID"));
    }
    const bool coordinate_exists =
        std::any_of(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                    [&coordinate_system](const SCoordinateSystem& candidate)
                    {
                        return candidate.id == coordinate_system.id;
                    });
    if (coordinate_exists)
    {
        return SResult<SObjectId>::failure(SErrorCode::Conflict, tr("导入坐标系 ID 已存在"));
    }
    const bool parent_exists =
        std::any_of(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                    [&coordinate_system](const SCoordinateSystem& candidate)
                    {
                        return candidate.id == coordinate_system.parent_id;
                    });
    if (!parent_exists)
    {
        return SResult<SObjectId>::failure(SErrorCode::NotFound, tr("导入坐标系的父坐标系不存在"));
    }
    object.coordinate_system_id = coordinate_system.id;
    object.stage = SDataStage::Original;
    object.locked = true;
    const SObjectId object_id = object.id;
    SDocumentTransaction transaction(*this, std::move(operation_name), {object_id},
                                     std::move(parameter_summary));
    const SResult<void> committed = transaction.commit(
        [this, object = std::move(object),
         coordinate_system = std::move(coordinate_system)]() mutable
        {
            m_coordinate_systems.push_back(std::move(coordinate_system));
            m_objects.push_back(std::move(object));
            return SResult<void>::success();
        });
    if (!committed)
    {
        return SResult<SObjectId>::failure(committed.errorCode(), committed.message(),
                                           committed.details());
    }
    return SResult<SObjectId>::success(object_id);
}

SResult<QList<SObjectId>> S3dDocument::addDerivedObjects(const QList<SObjectId>& inputs,
                                                         QList<SSceneObject> results,
                                                         QString operation_name,
                                                         bool replace_inputs,
                                                         QString parameter_summary)
{
    if (inputs.isEmpty() || results.isEmpty())
    {
        return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                  tr("批量派生操作缺少输入或结果"));
    }
    for (const SObjectId& input_id : inputs)
    {
        const SSceneObject* input = findObject(input_id);
        if (!input)
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::NotFound,
                                                      tr("派生输入对象不存在"));
        }
        if (replace_inputs && input->locked)
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::Locked,
                                                      tr("原始或锁定对象不能被替换"));
        }
    }

    QSet<SObjectId> result_ids;
    QList<SObjectId> added_ids;
    for (SSceneObject& result : results)
    {
        result.name = result.name.trimmed();
        if (result.id.isNull() || findObject(result.id) || result_ids.contains(result.id) ||
            result.name.isEmpty() || result.shape.isNull())
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                      tr("批量派生结果包含无效名称、ID 或空几何"));
        }
        if (result.coordinate_system_id.isNull())
        {
            result.coordinate_system_id = findObject(inputs.front())->coordinate_system_id;
        }
        const bool coordinate_exists =
            std::any_of(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                        [&result](const SCoordinateSystem& candidate)
                        {
                            return candidate.id == result.coordinate_system_id;
                        });
        if (!coordinate_exists)
        {
            return SResult<QList<SObjectId>>::failure(SErrorCode::NotFound,
                                                      tr("派生结果引用的坐标系不存在"));
        }
        if (!result.parent_id.isNull())
        {
            const SSceneObject* parent = findObject(result.parent_id);
            if (!parent || parent->type != SObjectType::Group)
            {
                return SResult<QList<SObjectId>>::failure(SErrorCode::InvalidArgument,
                                                          tr("派生结果父级必须是已存在的场景组"));
            }
        }
        result.stage = SDataStage::Working;
        result.derived_from = inputs;
        result_ids.insert(result.id);
        added_ids.push_back(result.id);
    }

    SDocumentTransaction transaction(*this, std::move(operation_name), inputs + added_ids,
                                     std::move(parameter_summary));
    const SResult<void> committed = transaction.commit(
        [this, inputs, results = std::move(results), replace_inputs]() mutable
        {
            if (replace_inputs)
            {
                m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(),
                                               [&inputs](const SSceneObject& object)
                                               {
                                                   return inputs.contains(object.id);
                                               }),
                                m_objects.end());
                for (SSceneObject& object : m_objects)
                {
                    if (object.type != SObjectType::Measurement)
                    {
                        continue;
                    }
                    for (const SObjectId& source_id : object.derived_from)
                    {
                        if (inputs.contains(source_id))
                        {
                            object.quality_warning = true;
                            object.quality_message = tr("关联几何已被替换，测量结果已失效");
                            break;
                        }
                    }
                }
            }
            else
            {
                for (const SObjectId& id : inputs)
                {
                    if (SSceneObject* input = findObject(id))
                    {
                        input->visible = false;
                    }
                }
            }
            for (SSceneObject& result : results)
            {
                m_objects.push_back(std::move(result));
            }
            return SResult<void>::success();
        });
    if (!committed)
    {
        return SResult<QList<SObjectId>>::failure(committed.errorCode(), committed.message(),
                                                  committed.details());
    }
    return SResult<QList<SObjectId>>::success(added_ids);
}
} // namespace smartGraphics3D
