#include "s_3d_document.h"

namespace smartGraphics3D
{
namespace
{
constexpr int kMaximumUndoEntries = 100;
}

SResult<void> S3dDocument::createSnapshot(QString name)
{
    name = name.trimmed();
    if (name.isEmpty())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, tr("快照名称不能为空"));
    }
    if (m_snapshots.contains(name))
    {
        return SResult<void>::failure(SErrorCode::Conflict, tr("同名快照已存在"));
    }
    const SDocumentState state = captureState();
    SSnapshotRecord snapshot;
    snapshot.name = name;
    snapshot.project_name = state.name;
    snapshot.units = state.units;
    snapshot.objects = state.objects;
    snapshot.coordinate_systems = state.coordinate_systems;
    m_snapshots.insert(name, std::move(snapshot));

    SOperationRecord record;
    record.name = tr("创建快照“%1”").arg(name);
    record.undoable = false;
    m_history.push_back(std::move(record));
    ++m_revision;
    setDirty(true);
    emit documentChanged();
    emit historyChanged();
    return SResult<void>::success();
}

SResult<void> S3dDocument::restoreSnapshot(const QString& name)
{
    if (!m_snapshots.contains(name))
    {
        return SResult<void>::failure(SErrorCode::NotFound, tr("快照不存在"));
    }
    const SSnapshotRecord snapshot = m_snapshots.value(name);
    SDocumentState target;
    target.objects = snapshot.objects;
    target.name = snapshot.project_name;
    target.units = snapshot.units;
    target.coordinate_systems = snapshot.coordinate_systems;
    return commit(tr("恢复快照“%1”").arg(name),
                  [this, target]()
                  {
                      restoreState(target);
                      return SResult<void>::success();
                  });
}

QStringList S3dDocument::snapshotNames() const
{
    return m_snapshots.keys();
}

QList<SSnapshotRecord> S3dDocument::snapshots() const
{
    return m_snapshots.values();
}

const QList<SOperationRecord>& S3dDocument::history() const
{
    return m_history;
}

void S3dDocument::newDocument()
{
    m_project_id = QUuid::createUuid();
    m_project_name = tr("未命名项目");
    m_file_path.clear();
    m_objects.clear();
    m_undo_entries.clear();
    m_undo_index = 0;
    m_snapshots.clear();
    m_coordinate_systems.clear();
    SCoordinateSystem world;
    world.name = tr("世界坐标系");
    world.source = tr("内置");
    m_coordinate_systems.push_back(std::move(world));
    m_history.clear();
    ++m_revision;
    setDirty(false);
    emit documentChanged();
    emit historyChanged();
}

void S3dDocument::replaceAll(QUuid project_id, QString project_name,
                             std::vector<SSceneObject> objects, QList<SOperationRecord> history,
                             const SUnitSystem& units,
                             std::vector<SCoordinateSystem> coordinate_systems,
                             QList<SSnapshotRecord> snapshots)
{
    m_project_id = std::move(project_id);
    m_project_name = std::move(project_name);
    m_objects = std::move(objects);
    m_history = std::move(history);
    m_units = units;
    m_coordinate_systems = std::move(coordinate_systems);
    if (m_coordinate_systems.empty())
    {
        SCoordinateSystem world;
        world.name = tr("世界坐标系");
        world.source = tr("内置");
        m_coordinate_systems.push_back(std::move(world));
    }
    m_undo_entries.clear();
    m_undo_index = 0;
    m_snapshots.clear();
    for (SSnapshotRecord& snapshot : snapshots)
    {
        m_snapshots.insert(snapshot.name, std::move(snapshot));
    }
    ++m_revision;
    setDirty(false);
    emit documentChanged();
    emit historyChanged();
}

SResult<void> S3dDocument::commit(QString operation_name,
                                  const std::function<SResult<void>()>& mutation,
                                  const QList<SObjectId>& affected_ids, QString parameter_summary)
{
    const SDocumentState before = captureState();
    const SResult<void> result = mutation();
    if (!result)
    {
        restoreState(before);
        return result;
    }

    while (m_undo_entries.size() > m_undo_index)
    {
        m_undo_entries.removeLast();
    }
    m_undo_entries.push_back({operation_name, before, captureState()});
    if (m_undo_entries.size() > kMaximumUndoEntries)
    {
        m_undo_entries.removeFirst();
    }
    m_undo_index = m_undo_entries.size();

    SOperationRecord record;
    record.name = std::move(operation_name);
    record.object_ids = affected_ids;
    record.parameter_summary = parameter_summary.trimmed();
    if (record.parameter_summary.isEmpty())
    {
        record.parameter_summary =
            affected_ids.isEmpty() ? tr("无参数") : tr("影响 %1 个对象").arg(affected_ids.size());
    }
    m_history.push_back(std::move(record));
    ++m_revision;
    setDirty(true);
    emit documentChanged();
    emit historyChanged();
    return SResult<void>::success();
}

S3dDocument::SDocumentState S3dDocument::captureState() const
{
    return {m_objects, m_project_name, m_units, m_coordinate_systems};
}

void S3dDocument::restoreState(const SDocumentState& state)
{
    m_objects = state.objects;
    m_project_name = state.name;
    m_units = state.units;
    m_coordinate_systems = state.coordinate_systems;
}

void S3dDocument::setDirty(bool dirty)
{
    if (m_dirty != dirty)
    {
        m_dirty = dirty;
        emit dirtyChanged(dirty);
    }
}
} // namespace smartGraphics3D
