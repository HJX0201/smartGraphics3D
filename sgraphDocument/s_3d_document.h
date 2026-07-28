#pragma once

#include "s_result.h"
#include "s_scene_object.h"
#include "s_unit_system.h"

#include <QObject>
#include <QString>
#include <functional>
#include <vector>

namespace smartGraphics3D
{
class SDocumentTransaction;

class S3dDocument final : public QObject
{
    Q_OBJECT

  public:
    explicit S3dDocument(QObject* parent = nullptr);

    const QUuid& projectId() const;
    const QString& projectName() const;
    void setProjectName(QString name);
    const QString& filePath() const;
    void setFilePath(QString path);
    bool isDirty() const;
    quint64 revision() const;
    void markSaved();
    void markDirty();
    const SUnitSystem& unitSystem() const;
    SResult<void> setUnits(SLengthUnit length_unit, SAngleUnit angle_unit);
    SResult<void> setLengthUnit(SLengthUnit unit);
    SResult<void> setAngleUnit(SAngleUnit unit);
    const std::vector<SCoordinateSystem>& coordinateSystems() const;
    SResult<QUuid> addCoordinateSystem(SCoordinateSystem coordinate_system);
    SResult<void> removeCoordinateSystem(const QUuid& id);

    const std::vector<SSceneObject>& objects() const;
    const SSceneObject* findObject(const SObjectId& id) const;
    SSceneObject* findObject(const SObjectId& id);

    SResult<SObjectId> addObject(SSceneObject object, QString operation_name,
                                 QString parameter_summary = {});
    SResult<QList<SObjectId>> addObjects(QList<SSceneObject> objects, QString operation_name,
                                         QString parameter_summary = {});
    SResult<QList<SObjectId>> copyObjects(const QList<SObjectId>& ids, SCopyMode mode,
                                          QString operation_name, QString parameter_summary = {});
    SResult<SObjectId> addImportedObject(SSceneObject object, SCoordinateSystem coordinate_system,
                                         QString operation_name, QString parameter_summary = {});
    SResult<SObjectId> addDerivedObject(const QList<SObjectId>& inputs, SSceneObject result,
                                        QString operation_name, bool replace_inputs = false,
                                        QString parameter_summary = {});
    SResult<QList<SObjectId>> addDerivedObjects(const QList<SObjectId>& inputs,
                                                QList<SSceneObject> results, QString operation_name,
                                                bool replace_inputs = false,
                                                QString parameter_summary = {});
    SResult<void> removeObjects(const QList<SObjectId>& ids);
    SResult<void> renameObject(const SObjectId& id, QString name);
    SResult<void> setObjectVisible(const SObjectId& id, bool visible);
    SResult<void> setObjectLocked(const SObjectId& id, bool locked);
    SResult<void> setObjectFrozen(const SObjectId& id, bool frozen);
    SResult<void> setObjectTransform(const SObjectId& id, const QMatrix4x4& transform,
                                     QString operation_name = {});
    int presentationGroupMemberCount(const QUuid& group_id) const;
    SResult<void> setObjectParent(const SObjectId& id, const SObjectId& parent_id);
    SResult<void> isolateObjects(const QList<SObjectId>& ids);
    SResult<void> showAllObjects();
    SResult<void> setDisplayStyle(const SObjectId& id, const SDisplayStyle& style);

    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;
    void undo();
    void redo();

    SResult<void> createSnapshot(QString name);
    SResult<void> restoreSnapshot(const QString& name);
    QStringList snapshotNames() const;
    QList<SSnapshotRecord> snapshots() const;
    const QList<SOperationRecord>& history() const;

    void newDocument();
    void replaceAll(QUuid project_id, QString project_name, std::vector<SSceneObject> objects,
                    QList<SOperationRecord> history, const SUnitSystem& units,
                    std::vector<SCoordinateSystem> coordinate_systems = {},
                    QList<SSnapshotRecord> snapshots = {});

  signals:
    void documentChanged();
    void dirtyChanged(bool dirty);
    void historyChanged();

  private:
    friend class SDocumentTransaction;

    struct SDocumentState
    {
        std::vector<SSceneObject> objects;
        QString name;
        SUnitSystem units;
        std::vector<SCoordinateSystem> coordinate_systems;
    };

    struct SUndoEntry
    {
        QString name;
        SDocumentState before;
        SDocumentState after;
    };

    SResult<void> commit(QString operation_name, const std::function<SResult<void>()>& mutation,
                         const QList<SObjectId>& affected_ids = {}, QString parameter_summary = {});
    SDocumentState captureState() const;
    void restoreState(const SDocumentState& state);
    void setDirty(bool dirty);

    QUuid m_project_id = QUuid::createUuid();
    QString m_project_name = QStringLiteral("未命名项目");
    QString m_file_path;
    SUnitSystem m_units;
    std::vector<SCoordinateSystem> m_coordinate_systems;
    std::vector<SSceneObject> m_objects;
    QList<SUndoEntry> m_undo_entries;
    int m_undo_index = 0;
    QMap<QString, SSnapshotRecord> m_snapshots;
    QList<SOperationRecord> m_history;
    bool m_dirty = false;
    quint64 m_revision = 0;
};
} // namespace smartGraphics3D
