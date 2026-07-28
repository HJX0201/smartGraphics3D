#include "s_main_window.h"
#include "s_occ_viewport.h"

#include <QAbstractItemModel>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <functional>

namespace smartGraphics3D
{
void SMainWindow::connectSignals()
{
    connect(&m_document, &S3dDocument::documentChanged, this,
            [this]()
            {
                refreshSceneTree();
                refreshProperties();
                refreshHistory();
                refreshWindowTitle();
                m_undo_action->setEnabled(m_document.canUndo());
                m_redo_action->setEnabled(m_document.canRedo());
            });
    connect(&m_document, &S3dDocument::dirtyChanged, this,
            [this](bool dirty)
            {
                m_dirty_label->setText(dirty ? tr("未保存") : tr("已保存"));
                refreshWindowTitle();
            });
    connect(m_viewport, &SOccViewport::selectionChanged, this,
            [this](const QList<SObjectId>& ids)
            {
                if (m_synchronizing_selection)
                {
                    return;
                }
                m_synchronizing_selection = true;
                selectTreeItems(ids);
                if (m_sync_selections)
                {
                    for (SOccViewport* viewport : m_viewports)
                    {
                        if (viewport != m_viewport)
                        {
                            viewport->setSelectedObjects(ids);
                        }
                    }
                }
                m_synchronizing_selection = false;
                refreshProperties();
            });
    connect(m_viewport, &SOccViewport::subSelectionChanged, this,
            [this](const QList<SSelection>& selections)
            {
                m_sub_selections = selections;
            });
    connect(m_viewport, &SOccViewport::cursorPositionChanged, this,
            [this](double x, double y, double z)
            {
                m_coordinate_label->setText(tr("X %1  Y %2  Z %3 %4")
                                                .arg(x, 0, 'f', 3)
                                                .arg(y, 0, 'f', 3)
                                                .arg(z, 0, 'f', 3)
                                                .arg(m_document.unitSystem().lengthSuffix()));
            });
    connect(m_viewport, &SOccViewport::frameRendered, this,
            [this](double fps)
            {
                m_fps_label->setText(tr("%1 FPS").arg(fps, 0, 'f', 0));
            });
    connect(m_viewport, &SOccViewport::cameraChanged, this,
            [this]()
            {
                if (!m_sync_cameras || m_synchronizing_camera)
                {
                    return;
                }
                m_synchronizing_camera = true;
                for (SOccViewport* viewport : m_viewports)
                {
                    if (viewport != m_viewport)
                    {
                        viewport->copyCameraFrom(*m_viewport);
                    }
                }
                m_synchronizing_camera = false;
            });
    connect(m_scene_tree, &QTreeWidget::itemSelectionChanged, this,
            [this]()
            {
                if (m_refreshing_tree)
                {
                    return;
                }
                const QList<SObjectId> ids = selectedObjectIds();
                if (!m_synchronizing_selection)
                {
                    m_synchronizing_selection = true;
                    if (m_sync_selections)
                    {
                        for (SOccViewport* viewport : m_viewports)
                        {
                            viewport->setSelectedObjects(ids);
                        }
                    }
                    else
                    {
                        m_viewport->setSelectedObjects(ids);
                    }
                    m_synchronizing_selection = false;
                }
                refreshProperties();
            });
    connect(m_scene_tree, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int column)
            {
                if (m_refreshing_tree)
                {
                    return;
                }
                const SObjectId id(item->data(0, Qt::UserRole).toString());
                if (column != 0)
                {
                    return;
                }
                const SSceneObject* object = m_document.findObject(id);
                if (object && object->name != item->text(0))
                {
                    m_document.renameObject(id, item->text(0));
                    return;
                }
                if (object && object->visible != (item->checkState(0) == Qt::Checked))
                {
                    m_document.setObjectVisible(id, item->checkState(0) == Qt::Checked);
                }
            });
    connect(m_scene_filter, &QLineEdit::textChanged, this,
            [this]()
            {
                refreshSceneTree();
            });
    connect(m_scene_tree->model(), &QAbstractItemModel::rowsMoved, this,
            [this]()
            {
                if (m_refreshing_tree)
                {
                    return;
                }
                QList<QPair<SObjectId, SObjectId>> hierarchy;
                std::function<void(QTreeWidgetItem*, SObjectId)> collect =
                    [&collect, &hierarchy](QTreeWidgetItem* item, SObjectId parent_id)
                {
                    const SObjectId id(item->data(0, Qt::UserRole).toString());
                    hierarchy.push_back({id, parent_id});
                    for (int index = 0; index < item->childCount(); ++index)
                    {
                        collect(item->child(index), id);
                    }
                };
                for (int index = 0; index < m_scene_tree->topLevelItemCount(); ++index)
                {
                    collect(m_scene_tree->topLevelItem(index), {});
                }
                QTimer::singleShot(0, this,
                                   [this, hierarchy]()
                                   {
                                       for (const auto& pair : hierarchy)
                                       {
                                           const auto result =
                                               m_document.setObjectParent(pair.first, pair.second);
                                           if (!result)
                                           {
                                               showFailure(tr("调整场景层级失败"), result);
                                               break;
                                           }
                                       }
                                   });
            });
    connect(m_scene_tree, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& position)
            {
                QMenu menu(this);
                menu.addAction(tr("普通复制"), this, &SMainWindow::duplicateSelection);
                menu.addAction(tr("实例复制"), this, &SMainWindow::duplicateSelectionShared);
                menu.addAction(tr("删除"), this, &SMainWindow::deleteSelection);
                menu.addAction(tr("新建组并归入"),
                               [this]()
                               {
                                   const QList<SObjectId> ids = selectedObjectIds();
                                   bool accepted = false;
                                   const QString name = QInputDialog::getText(
                                       this, tr("新建场景组"), tr("组名称"), QLineEdit::Normal,
                                       tr("新建组"), &accepted);
                                   if (!accepted || name.trimmed().isEmpty())
                                   {
                                       return;
                                   }
                                   SSceneObject group;
                                   group.name = name.trimmed();
                                   group.type = SObjectType::Group;
                                   group.source = tr("用户创建");
                                   const auto added = m_document.addObject(group, tr("创建场景组"));
                                   if (!added)
                                   {
                                       return;
                                   }
                                   for (const SObjectId& id : ids)
                                   {
                                       m_document.setObjectParent(id, added.value());
                                   }
                               });
                menu.addAction(tr("锁定/解锁"),
                               [this]()
                               {
                                   const QList<SObjectId> ids = selectedObjectIds();
                                   for (const SObjectId& id : ids)
                                   {
                                       const SSceneObject* object = m_document.findObject(id);
                                       if (object)
                                       {
                                           m_document.setObjectLocked(id, !object->locked);
                                       }
                                   }
                               });
                menu.addAction(tr("冻结/解冻"),
                               [this]()
                               {
                                   const QList<SObjectId> ids = selectedObjectIds();
                                   for (const SObjectId& id : ids)
                                   {
                                       const SSceneObject* object = m_document.findObject(id);
                                       if (object)
                                       {
                                           m_document.setObjectFrozen(id, !object->frozen);
                                       }
                                   }
                               });
                menu.addAction(tr("独显"),
                               [this]()
                               {
                                   m_document.isolateObjects(selectedObjectIds());
                               });
                menu.addAction(tr("显示全部"),
                               [this]()
                               {
                                   m_document.showAllObjects();
                               });
                menu.addSeparator();
                menu.addAction(tr("适合选中"), m_viewport, &SOccViewport::fitSelection);
                menu.exec(m_scene_tree->viewport()->mapToGlobal(position));
            });
    connect(m_command_input, &QLineEdit::returnPressed, this,
            [this]()
            {
                const QString command = m_command_input->text().trimmed();
                if (!command.isEmpty())
                {
                    m_console->append(QStringLiteral("> %1").arg(command));
                    executeCommand(command);
                    m_command_input->clear();
                }
            });
    connectTaskSignals();
}
} // namespace smartGraphics3D
