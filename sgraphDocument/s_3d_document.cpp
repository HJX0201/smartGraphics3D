#include "s_3d_document.h"

#include <algorithm>
#include <cmath>

namespace smartGraphics3D
{
S3dDocument::S3dDocument(QObject* parent) : QObject(parent)
{
    SCoordinateSystem world;
    world.name = tr("世界坐标系");
    world.source = tr("内置");
    m_coordinate_systems.push_back(std::move(world));
}

const QUuid& S3dDocument::projectId() const
{
    return m_project_id;
}

const QString& S3dDocument::projectName() const
{
    return m_project_name;
}

void S3dDocument::setProjectName(QString name)
{
    if (!name.trimmed().isEmpty() && m_project_name != name)
    {
        m_project_name = std::move(name);
        ++m_revision;
        setDirty(true);
        emit documentChanged();
    }
}

const QString& S3dDocument::filePath() const
{
    return m_file_path;
}

void S3dDocument::setFilePath(QString path)
{
    m_file_path = std::move(path);
}

bool S3dDocument::isDirty() const
{
    return m_dirty;
}

quint64 S3dDocument::revision() const
{
    return m_revision;
}

void S3dDocument::markSaved()
{
    setDirty(false);
}

void S3dDocument::markDirty()
{
    setDirty(true);
}

const std::vector<SSceneObject>& S3dDocument::objects() const
{
    return m_objects;
}

const SSceneObject* S3dDocument::findObject(const SObjectId& id) const
{
    const auto iterator = std::find_if(m_objects.cbegin(), m_objects.cend(),
                                       [&id](const SSceneObject& object)
                                       {
                                           return object.id == id;
                                       });
    return iterator == m_objects.cend() ? nullptr : &*iterator;
}

SSceneObject* S3dDocument::findObject(const SObjectId& id)
{
    const auto iterator = std::find_if(m_objects.begin(), m_objects.end(),
                                       [&id](const SSceneObject& object)
                                       {
                                           return object.id == id;
                                       });
    return iterator == m_objects.end() ? nullptr : &*iterator;
}

SResult<SObjectId> S3dDocument::addObject(SSceneObject object, QString operation_name,
                                          QString parameter_summary)
{
    object.name = object.name.trimmed();
    if (object.id.isNull())
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument, tr("对象 ID 不能为空"));
    }
    if (findObject(object.id))
    {
        return SResult<SObjectId>::failure(SErrorCode::Conflict, tr("对象 ID 已存在"));
    }
    if (object.name.isEmpty())
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument, tr("对象名称不能为空"));
    }
    if (object.shape.isNull() && object.type != SObjectType::Group &&
        object.type != SObjectType::Measurement && object.type != SObjectType::CoordinateSystem)
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument, tr("不能添加空几何对象"));
    }
    if (object.coordinate_system_id.isNull() && !m_coordinate_systems.empty())
    {
        object.coordinate_system_id = m_coordinate_systems.front().id;
    }
    const auto coordinate_system =
        std::find_if(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                     [&object](const SCoordinateSystem& candidate)
                     {
                         return candidate.id == object.coordinate_system_id;
                     });
    if (coordinate_system == m_coordinate_systems.cend())
    {
        return SResult<SObjectId>::failure(SErrorCode::NotFound, tr("对象引用的坐标系不存在"));
    }
    if (!object.parent_id.isNull())
    {
        const SSceneObject* parent = findObject(object.parent_id);
        if (!parent || parent->type != SObjectType::Group)
        {
            return SResult<SObjectId>::failure(SErrorCode::InvalidArgument,
                                               tr("对象父级必须是已存在的场景组"));
        }
    }
    if (object.stage == SDataStage::Original)
    {
        object.locked = true;
    }
    const SObjectId id = object.id;
    const auto result = commit(
        operation_name,
        [this, object = std::move(object)]() mutable
        {
            m_objects.push_back(std::move(object));
            return SResult<void>::success();
        },
        {id}, std::move(parameter_summary));
    if (!result)
    {
        return SResult<SObjectId>::failure(result.errorCode(), result.message(), result.details());
    }
    return SResult<SObjectId>::success(id);
}

SResult<SObjectId> S3dDocument::addDerivedObject(const QList<SObjectId>& inputs,
                                                 SSceneObject result, QString operation_name,
                                                 bool replace_inputs, QString parameter_summary)
{
    result.name = result.name.trimmed();
    if (inputs.isEmpty() || result.shape.isNull())
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument,
                                           tr("派生操作缺少输入或结果"));
    }
    if (result.id.isNull() || findObject(result.id))
    {
        return SResult<SObjectId>::failure(
            result.id.isNull() ? SErrorCode::InvalidArgument : SErrorCode::Conflict,
            result.id.isNull() ? tr("派生对象 ID 不能为空") : tr("派生对象 ID 已存在"));
    }
    if (result.name.isEmpty())
    {
        return SResult<SObjectId>::failure(SErrorCode::InvalidArgument, tr("派生对象名称不能为空"));
    }
    for (const SObjectId& id : inputs)
    {
        const SSceneObject* input = findObject(id);
        if (!input)
        {
            return SResult<SObjectId>::failure(SErrorCode::NotFound, tr("输入对象不存在"));
        }
        if (input->locked && replace_inputs)
        {
            return SResult<SObjectId>::failure(SErrorCode::Locked, tr("原始或锁定对象不能被替换"));
        }
    }

    if (result.coordinate_system_id.isNull())
    {
        result.coordinate_system_id = findObject(inputs.front())->coordinate_system_id;
    }
    const auto coordinate_system =
        std::find_if(m_coordinate_systems.cbegin(), m_coordinate_systems.cend(),
                     [&result](const SCoordinateSystem& candidate)
                     {
                         return candidate.id == result.coordinate_system_id;
                     });
    if (coordinate_system == m_coordinate_systems.cend())
    {
        return SResult<SObjectId>::failure(SErrorCode::NotFound, tr("派生对象引用的坐标系不存在"));
    }
    if (!result.parent_id.isNull())
    {
        const SSceneObject* parent = findObject(result.parent_id);
        if (!parent || parent->type != SObjectType::Group)
        {
            return SResult<SObjectId>::failure(SErrorCode::InvalidArgument,
                                               tr("派生对象父级必须是已存在的场景组"));
        }
    }
    result.stage = SDataStage::Working;
    result.derived_from = inputs;
    const SObjectId result_id = result.id;
    const auto commit_result = commit(
        operation_name,
        [this, inputs, result = std::move(result), replace_inputs]() mutable
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
            m_objects.push_back(std::move(result));
            return SResult<void>::success();
        },
        inputs + QList<SObjectId>{result_id}, std::move(parameter_summary));
    if (!commit_result)
    {
        return SResult<SObjectId>::failure(commit_result.errorCode(), commit_result.message(),
                                           commit_result.details());
    }
    return SResult<SObjectId>::success(result_id);
}

SResult<void> S3dDocument::removeObjects(const QList<SObjectId>& ids)
{
    if (ids.isEmpty())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("请选择要删除的对象"));
    }
    for (const SObjectId& id : ids)
    {
        const SSceneObject* object = findObject(id);
        if (!object)
        {
            return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
        }
        if (object && object->locked)
        {
            return SResult<void>::failure(SErrorCode::Locked,
                                          tr("对象“%1”已锁定，不能删除").arg(object->name));
        }
    }
    return commit(
        tr("删除对象"),
        [this, ids]()
        {
            m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(),
                                           [&ids](const SSceneObject& object)
                                           {
                                               return ids.contains(object.id);
                                           }),
                            m_objects.end());
            for (SSceneObject& object : m_objects)
            {
                if (ids.contains(object.parent_id))
                {
                    object.parent_id = SObjectId();
                }
                if (object.type != SObjectType::Measurement)
                {
                    continue;
                }
                for (const SObjectId& source_id : object.derived_from)
                {
                    if (ids.contains(source_id))
                    {
                        object.quality_warning = true;
                        object.quality_message = tr("关联几何已删除，测量结果已失效");
                        break;
                    }
                }
            }
            return SResult<void>::success();
        },
        ids);
}

SResult<void> S3dDocument::renameObject(const SObjectId& id, QString name)
{
    if (name.trimmed().isEmpty())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("对象名称不能为空"));
    }
    return commit(tr("重命名对象"),
                  [this, id, name = std::move(name)]()
                  {
                      SSceneObject* object = findObject(id);
                      if (!object)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      object->name = name;
                      object->modified_at = QDateTime::currentDateTimeUtc();
                      return SResult<void>::success();
                  },
                  {id});
}

SResult<void> S3dDocument::setObjectVisible(const SObjectId& id, bool visible)
{
    const SSceneObject* current = findObject(id);
    if (current && current->visible == visible)
    {
        return SResult<void>::success();
    }
    return commit(visible ? tr("显示对象") : tr("隐藏对象"),
                  [this, id, visible]()
                  {
                      SSceneObject* object = findObject(id);
                      if (!object)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      object->visible = visible;
                      return SResult<void>::success();
                  },
                  {id});
}

SResult<void> S3dDocument::setObjectLocked(const SObjectId& id, bool locked)
{
    const SSceneObject* current = findObject(id);
    if (current && current->locked == locked)
    {
        return SResult<void>::success();
    }
    return commit(locked ? tr("锁定对象") : tr("解锁对象"),
                  [this, id, locked]()
                  {
                      SSceneObject* object = findObject(id);
                      if (!object)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      object->locked = locked;
                      return SResult<void>::success();
                  },
                  {id});
}

SResult<void> S3dDocument::setObjectFrozen(const SObjectId& id, bool frozen)
{
    const SSceneObject* current = findObject(id);
    if (current && current->frozen == frozen)
    {
        return SResult<void>::success();
    }
    return commit(frozen ? tr("冻结对象") : tr("解冻对象"),
                  [this, id, frozen]()
                  {
                      SSceneObject* object = findObject(id);
                      if (!object)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      object->frozen = frozen;
                      return SResult<void>::success();
                  },
                  {id});
}

SResult<void> S3dDocument::setObjectParent(const SObjectId& id, const SObjectId& parent_id)
{
    if (id == parent_id)
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("对象不能成为自己的父对象"));
    }
    const SSceneObject* object = findObject(id);
    if (!object)
    {
        return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
    }
    if (!parent_id.isNull())
    {
        const SSceneObject* parent = findObject(parent_id);
        if (!parent || parent->type != SObjectType::Group)
        {
            return SResult<void>::failure(SErrorCode::InvalidArgument, tr("父对象必须是场景组"));
        }
        SObjectId ancestor_id = parent->parent_id;
        while (!ancestor_id.isNull())
        {
            if (ancestor_id == id)
            {
                return SResult<void>::failure(SErrorCode::Conflict, tr("场景层级不能形成循环"));
            }
            const SSceneObject* ancestor = findObject(ancestor_id);
            ancestor_id = ancestor ? ancestor->parent_id : SObjectId();
        }
    }
    if (object->parent_id == parent_id)
    {
        return SResult<void>::success();
    }
    return commit(tr("调整场景层级"),
                  [this, id, parent_id]()
                  {
                      SSceneObject* target = findObject(id);
                      if (!target)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      target->parent_id = parent_id;
                      return SResult<void>::success();
                  },
                  {id, parent_id});
}

SResult<void> S3dDocument::isolateObjects(const QList<SObjectId>& ids)
{
    if (ids.isEmpty())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("请选择需要独显的对象"));
    }
    for (const SObjectId& id : ids)
    {
        if (!findObject(id))
        {
            return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
        }
    }
    return commit(
        tr("独显对象"),
        [this, ids]()
        {
            for (SSceneObject& object : m_objects)
            {
                object.visible = ids.contains(object.id) || object.type == SObjectType::Measurement;
            }
            return SResult<void>::success();
        },
        ids);
}

SResult<void> S3dDocument::showAllObjects()
{
    return commit(tr("显示全部对象"),
                  [this]()
                  {
                      for (SSceneObject& object : m_objects)
                      {
                          object.visible = true;
                      }
                      return SResult<void>::success();
                  });
}

SResult<void> S3dDocument::setDisplayStyle(const SObjectId& id, const SDisplayStyle& style)
{
    if (!style.color.isValid() || !std::isfinite(style.transparency) || style.transparency < 0.0 ||
        style.transparency > 1.0)
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("显示颜色或透明度参数无效"));
    }
    return commit(tr("修改显示样式"),
                  [this, id, style]()
                  {
                      SSceneObject* object = findObject(id);
                      if (!object)
                      {
                          return SResult<void>::failure(SErrorCode::NotFound, tr("对象不存在"));
                      }
                      object->display = style;
                      object->use_imported_appearance = false;
                      object->modified_at = QDateTime::currentDateTimeUtc();
                      return SResult<void>::success();
                  },
                  {id});
}

bool S3dDocument::canUndo() const
{
    return m_undo_index > 0;
}

bool S3dDocument::canRedo() const
{
    return m_undo_index < m_undo_entries.size();
}

QString S3dDocument::undoText() const
{
    return canUndo() ? m_undo_entries.at(m_undo_index - 1).name : QString();
}

QString S3dDocument::redoText() const
{
    return canRedo() ? m_undo_entries.at(m_undo_index).name : QString();
}

void S3dDocument::undo()
{
    if (!canUndo())
    {
        return;
    }
    --m_undo_index;
    restoreState(m_undo_entries.at(m_undo_index).before);
    ++m_revision;
    setDirty(true);
    emit documentChanged();
    emit historyChanged();
}

void S3dDocument::redo()
{
    if (!canRedo())
    {
        return;
    }
    restoreState(m_undo_entries.at(m_undo_index).after);
    ++m_undo_index;
    ++m_revision;
    setDirty(true);
    emit documentChanged();
    emit historyChanged();
}
} // namespace smartGraphics3D
