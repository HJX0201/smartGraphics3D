#include "s_icon_factory.h"
#include "s_main_window.h"
#include "s_ribbon_widget.h"
#include "s_version.h"

#include <QAction>
#include <QActionGroup>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>

namespace smartGraphics3D
{
namespace
{
QString selectionModeName(SSelectionMode mode)
{
    switch (mode)
    {
    case SSelectionMode::Object:
        return QObject::tr("对象选择");
    case SSelectionMode::Solid:
        return QObject::tr("实体选择");
    case SSelectionMode::Face:
        return QObject::tr("面选择");
    case SSelectionMode::Edge:
        return QObject::tr("边选择");
    case SSelectionMode::Vertex:
        return QObject::tr("顶点选择");
    }
    return QObject::tr("对象选择");
}
} // namespace

void SMainWindow::createRibbon()
{
    m_ribbon = new SRibbonWidget(this);
    setMenuWidget(m_ribbon);
    for (QAction* action : actions())
    {
        const QString page = action->property("ribbonPage").toString();
        if (page.isEmpty())
        {
            continue;
        }

        QString group = tr("常用");
        if (page == tr("建模"))
        {
            group = tr("基本体");
        }
        else if (page == tr("修改"))
        {
            group =
                action == m_undo_action || action == m_redo_action ? tr("历史") : tr("直接编辑");
        }
        m_ribbon->addAction(page, group, action);
    }

    createViewRibbon();

    QAction* diagnostics_action = makeAction(tr("导出诊断包"), SIconId::Diagnostics);
    connect(diagnostics_action, &QAction::triggered, this, &SMainWindow::exportDiagnostics);
    m_ribbon->addAction(tr("帮助"), tr("支持"), diagnostics_action);

    QAction* interface_scale_action = makeAction(tr("界面比例"), SIconId::InterfaceScale);
    interface_scale_action->setObjectName(QStringLiteral("interfaceScaleAction"));
    connect(interface_scale_action, &QAction::triggered, this,
            &SMainWindow::showInterfaceScaleDialog);
    m_ribbon->addAction(tr("帮助"), tr("界面"), interface_scale_action);

    QAction* about_action = makeAction(tr("关于"), SIconId::About);
    connect(about_action, &QAction::triggered, this,
            [this]()
            {
                QMessageBox::about(this, tr("关于 smartGraphics3D"),
                                   tr("smartGraphics3D %1\nC++17 / Qt 5.12.10 / OpenCascade 7.7.0")
                                       .arg(QString::fromLatin1(SMARTGRAPHICS3D_VERSION)));
            });
    m_ribbon->addAction(tr("帮助"), tr("帮助"), about_action);
}

void SMainWindow::createViewRibbon()
{
    const QString page = tr("视图");
    constexpr int kCompactRow = 0;
    constexpr int kStandardRow = 1;

    auto add_compact = [this, &page](const QString& group, const QString& text, SIconId icon_id,
                                     const std::function<void()>& function,
                                     const QString& command_id = QString())
    {
        QAction* action = makeAction(text, icon_id);
        if (command_id.isEmpty())
        {
            connect(action, &QAction::triggered, this, function);
        }
        else
        {
            bindCommand(action, command_id, function);
        }
        m_ribbon->addAction(page, group, action, kCompactRow, SRibbonButtonSize::Compact, false);
        return action;
    };

    add_compact(
        tr("导航"), tr("适合窗口"), SIconId::FitAll,
        [this]()
        {
            m_viewport->fitAll();
        },
        QStringLiteral("FIT"));
    add_compact(tr("导航"), tr("适合选中"), SIconId::FitSelection,
                [this]()
                {
                    m_viewport->fitSelection();
                });
    add_compact(tr("导航"), tr("轴测"), SIconId::ViewIsometric,
                [this]()
                {
                    m_viewport->setStandardView(SStandardView::Isometric);
                });

    const struct
    {
        const char* text;
        SIconId icon_id;
        SStandardView view;
    } standard_views[] = {
        {"前视", SIconId::ViewFront, SStandardView::Front},
        {"后视", SIconId::ViewBack, SStandardView::Back},
        {"左视", SIconId::ViewLeft, SStandardView::Left},
        {"右视", SIconId::ViewRight, SStandardView::Right},
        {"顶视", SIconId::ViewTop, SStandardView::Top},
        {"底视", SIconId::ViewBottom, SStandardView::Bottom},
    };
    for (const auto& definition : standard_views)
    {
        add_compact(tr("标准方向"), tr(definition.text), definition.icon_id,
                    [this, view = definition.view]()
                    {
                        m_viewport->setStandardView(view);
                    });
    }

    auto* display_group = new QActionGroup(this);
    display_group->setExclusive(true);
    const struct
    {
        const char* text;
        SIconId icon_id;
        SDisplayMode mode;
    } display_modes[] = {
        {"线框", SIconId::DisplayWireframe, SDisplayMode::Wireframe},
        {"着色实体", SIconId::DisplayShaded, SDisplayMode::Shaded},
        {"着色边线", SIconId::DisplayShadedEdges, SDisplayMode::ShadedWithEdges},
        {"隐藏线", SIconId::DisplayHiddenLine, SDisplayMode::HiddenLine},
        {"半透明", SIconId::DisplayTransparent, SDisplayMode::Transparent},
    };
    for (const auto& definition : display_modes)
    {
        QAction* action = makeAction(tr(definition.text), definition.icon_id);
        action->setCheckable(true);
        action->setChecked(definition.mode == SDisplayMode::ShadedWithEdges);
        display_group->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, mode = definition.mode]()
                {
                    for (SOccViewport* viewport : m_viewports)
                    {
                        viewport->setDisplayMode(mode);
                    }
                });
        m_ribbon->addAction(page, tr("显示模式"), action, kCompactRow, SRibbonButtonSize::Compact,
                            false);
    }

    auto add_standard = [this, &page](const QString& group, const QString& text, SIconId icon_id,
                                      const std::function<void()>& function)
    {
        QAction* action = makeAction(text, icon_id);
        connect(action, &QAction::triggered, this, function);
        m_ribbon->addAction(page, group, action, kStandardRow, SRibbonButtonSize::Standard, true);
        return action;
    };

    QAction* projection_action = makeAction(tr("正交/透视"), SIconId::Projection);
    projection_action->setCheckable(true);
    projection_action->setToolTip(tr("选中时使用透视投影，未选中时使用正交投影"));
    connect(projection_action, &QAction::toggled, this,
            [this](bool perspective)
            {
                for (SOccViewport* viewport : m_viewports)
                {
                    viewport->setPerspective(perspective);
                }
            });
    m_ribbon->addAction(page, tr("视图工具"), projection_action, kStandardRow,
                        SRibbonButtonSize::Standard, true);

    add_standard(tr("视图工具"), tr("保存视角"), SIconId::ViewSave,
                 [this]()
                 {
                     bool accepted = false;
                     const QString name =
                         QInputDialog::getText(this, tr("保存视角"), tr("视角名称"),
                                               QLineEdit::Normal, tr("用户视角"), &accepted);
                     if (accepted && !name.trimmed().isEmpty())
                     {
                         m_viewport->saveView(name);
                     }
                 });
    add_standard(
        tr("视图工具"), tr("恢复视角"), SIconId::ViewRestore,
        [this]()
        {
            const QStringList names = m_viewport->savedViewNames();
            if (names.isEmpty())
            {
                QMessageBox::information(this, tr("恢复视角"), tr("尚未保存任何用户视角。"));
                return;
            }
            bool accepted = false;
            const QString name =
                QInputDialog::getItem(this, tr("恢复视角"), tr("视角"), names, 0, false, &accepted);
            if (accepted)
            {
                m_viewport->restoreView(name);
            }
        });
    add_standard(tr("视图工具"), tr("旋转中心"), SIconId::RotationCenter,
                 [this]()
                 {
                     m_viewport->setRotationCenterFromSelection();
                 });

    QAction* free_rotation_action = makeAction(tr("自由旋转"), SIconId::FreeRotation);
    free_rotation_action->setCheckable(true);
    free_rotation_action->setToolTip(tr("允许视图滚转；关闭时使用稳定的目标轨道旋转"));
    connect(free_rotation_action, &QAction::toggled, this,
            [this](bool enabled)
            {
                for (SOccViewport* viewport : m_viewports)
                {
                    viewport->setFreeRotation(enabled);
                }
            });
    m_ribbon->addAction(page, tr("视图工具"), free_rotation_action, kStandardRow,
                        SRibbonButtonSize::Standard, true);
    add_standard(tr("视图工具"), tr("剖切设置"), SIconId::ClipPlanes,
                 [this]()
                 {
                     configureClipPlanes();
                 });

    QAction* grid_action = makeAction(tr("显示网格"), SIconId::Grid);
    grid_action->setObjectName(QStringLiteral("gridVisibilityAction"));
    grid_action->setCheckable(true);
    grid_action->setChecked(true);
    grid_action->setToolTip(tr("显示或隐藏世界 XY 平面的矩形网格"));
    connect(grid_action, &QAction::toggled, this,
            [this](bool visible)
            {
                m_grid_visible = visible;
                for (SOccViewport* viewport : m_viewports)
                {
                    viewport->setGridVisible(visible);
                }
            });
    m_ribbon->addAction(page, tr("辅助显示"), grid_action, kStandardRow,
                        SRibbonButtonSize::Standard, true);

    auto* viewport_layout_group = new QActionGroup(this);
    viewport_layout_group->setExclusive(true);
    const struct
    {
        const char* text;
        SIconId icon_id;
        int layout_mode;
    } viewport_layouts[] = {
        {"单视口", SIconId::ViewportSingle, 1},
        {"左右视口", SIconId::ViewportHorizontal, 2},
        {"上下视口", SIconId::ViewportVertical, 3},
        {"四视图", SIconId::ViewportFour, 4},
    };
    for (const auto& definition : viewport_layouts)
    {
        QAction* action = makeAction(tr(definition.text), definition.icon_id);
        action->setCheckable(true);
        action->setChecked(definition.layout_mode == 1);
        viewport_layout_group->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, layout_mode = definition.layout_mode]()
                {
                    setViewportLayout(layout_mode);
                });
        m_ribbon->addAction(page, tr("视口布局"), action, kStandardRow, SRibbonButtonSize::Standard,
                            true);
    }

    QAction* sync_camera_action = makeAction(tr("同步相机"), SIconId::SyncCamera);
    sync_camera_action->setCheckable(true);
    sync_camera_action->setChecked(true);
    connect(sync_camera_action, &QAction::toggled, this,
            [this](bool enabled)
            {
                m_sync_cameras = enabled;
            });
    m_ribbon->addAction(page, tr("同步"), sync_camera_action, kStandardRow,
                        SRibbonButtonSize::Standard, true);

    QAction* sync_selection_action = makeAction(tr("同步选择"), SIconId::SyncSelection);
    sync_selection_action->setCheckable(true);
    sync_selection_action->setChecked(true);
    connect(sync_selection_action, &QAction::toggled, this,
            [this](bool enabled)
            {
                m_sync_selections = enabled;
            });
    m_ribbon->addAction(page, tr("同步"), sync_selection_action, kStandardRow,
                        SRibbonButtonSize::Standard, true);

    auto* selection_group = new QActionGroup(this);
    selection_group->setExclusive(true);
    const struct
    {
        const char* text;
        SIconId icon_id;
        SSelectionMode mode;
    } selection_modes[] = {
        {"对象", SIconId::SelectObject, SSelectionMode::Object},
        {"实体", SIconId::SelectSolid, SSelectionMode::Solid},
        {"面", SIconId::SelectFace, SSelectionMode::Face},
        {"边", SIconId::SelectEdge, SSelectionMode::Edge},
        {"顶点", SIconId::SelectVertex, SSelectionMode::Vertex},
    };
    for (const auto& definition : selection_modes)
    {
        QAction* action = makeAction(tr(definition.text), definition.icon_id);
        action->setCheckable(true);
        action->setChecked(definition.mode == SSelectionMode::Object);
        selection_group->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, mode = definition.mode]()
                {
                    for (SOccViewport* viewport : m_viewports)
                    {
                        viewport->setSelectionMode(mode);
                    }
                    m_selection_label->setText(selectionModeName(mode));
                });
        m_ribbon->addAction(page, tr("选择过滤"), action, kStandardRow, SRibbonButtonSize::Standard,
                            true);
    }

    QAction* select_all_action = add_standard(tr("选择过滤"), tr("全选"), SIconId::SelectAll,
                                              [this]()
                                              {
                                                  m_viewport->selectAllObjects();
                                              });
    select_all_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+A")));
    select_all_action->setShortcutContext(Qt::ApplicationShortcut);
    add_standard(tr("选择过滤"), tr("反选"), SIconId::SelectInvert,
                 [this]()
                 {
                     m_viewport->invertObjectSelection();
                 });

    add_standard(tr("坐标与单位"), tr("项目单位"), SIconId::ProjectUnit,
                 [this]()
                 {
                     setLengthUnit();
                 });
    add_standard(tr("坐标与单位"), tr("用户坐标系"), SIconId::UserCoordinateSystem,
                 [this]()
                 {
                     createUserCoordinateSystem();
                 });
    add_standard(tr("坐标与单位"), tr("对象坐标系"), SIconId::ObjectCoordinateSystem,
                 [this]()
                 {
                     createObjectCoordinateSystem();
                 });
    add_standard(tr("坐标与单位"), tr("坐标系列表"), SIconId::CoordinateSystemList,
                 [this]()
                 {
                     showCoordinateSystems();
                 });
}
} // namespace smartGraphics3D
