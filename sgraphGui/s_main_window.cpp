#include "s_main_window.h"

#include "s_dialog_size_policy.h"
#include "s_icon_factory.h"
#include "s_occ_viewport.h"
#include "s_ribbon_widget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

namespace smartGraphics3D
{
SMainWindow::SMainWindow(QWidget* parent)
    : QMainWindow(parent), m_kernel(createKernelService()), m_document(this), m_task_manager(this)
{
    installDialogSizePolicy();
    setObjectName(QStringLiteral("sgraphMainWindow"));
    setWindowTitle(QStringLiteral("smartGraphics3D"));
    setWindowIcon(applicationIcon());
    resize(1600, 960);
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks |
                   QMainWindow::AnimatedDocks);

    createActions();
    createRibbon();
    createWorkspace();
    createStatusBar();
    connectSignals();
    loadInterfacePreferences();
    refreshSceneTree();
    refreshProperties();
    refreshHistory();
    refreshWindowTitle();

    m_auto_save_timer = new QTimer(this);
    m_auto_save_timer->setInterval(5 * 60 * 1000);
    connect(m_auto_save_timer, &QTimer::timeout, this, &SMainWindow::autoSave);
    m_auto_save_timer->start();
    if (!qEnvironmentVariableIsSet("SMARTGRAPHICS3D_DISABLE_RECOVERY"))
    {
        QTimer::singleShot(0, this, &SMainWindow::checkRecovery);
    }
}

SMainWindow::~SMainWindow() = default;

void SMainWindow::closeEvent(QCloseEvent* event)
{
    if (confirmSaveChanges())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void SMainWindow::createActions()
{
    auto* new_action = makeAction(tr("新建"), SIconId::FileNew, QStringLiteral("Ctrl+N"));
    auto* open_action = makeAction(tr("打开"), SIconId::FileOpen, QStringLiteral("Ctrl+O"));
    auto* save_action = makeAction(tr("保存"), SIconId::FileSave, QStringLiteral("Ctrl+S"));
    auto* save_as_action = makeAction(tr("另存为"), SIconId::FileSaveAs);
    auto* import_action = makeAction(tr("导入 CAD"), SIconId::FileImport);
    auto* export_action = makeAction(tr("导出选中"), SIconId::FileExport);
    auto* archive_action = makeAction(tr("项目归档"), SIconId::ProjectArchive);
    connect(new_action, &QAction::triggered, this, &SMainWindow::newProject);
    bindCommand(open_action, QStringLiteral("OPEN"),
                [this]()
                {
                    openProject();
                });
    bindCommand(save_action, QStringLiteral("SAVE"),
                [this]()
                {
                    saveProjectInBackground();
                });
    connect(save_as_action, &QAction::triggered, this, &SMainWindow::saveProjectAsInBackground);
    bindCommand(import_action, QStringLiteral("IMPORT"),
                [this]()
                {
                    importCad();
                });
    connect(export_action, &QAction::triggered, this, &SMainWindow::exportSelected);
    connect(archive_action, &QAction::triggered, this, &SMainWindow::archiveProject);

    auto* box_action = makeAction(tr("长方体"), SIconId::PrimitiveBox);
    auto* cylinder_action = makeAction(tr("圆柱"), SIconId::PrimitiveCylinder);
    auto* cone_action = makeAction(tr("圆锥"), SIconId::PrimitiveCone);
    auto* sphere_action = makeAction(tr("球体"), SIconId::PrimitiveSphere);
    auto* torus_action = makeAction(tr("圆环体"), SIconId::PrimitiveTorus);
    bindCommand(box_action, QStringLiteral("BOX"),
                [this]()
                {
                    createBox();
                });
    bindCommand(cylinder_action, QStringLiteral("CYLINDER"),
                [this]()
                {
                    createCylinder();
                });
    connect(cone_action, &QAction::triggered, this, &SMainWindow::createCone);
    bindCommand(sphere_action, QStringLiteral("SPHERE"),
                [this]()
                {
                    createSphere();
                });
    connect(torus_action, &QAction::triggered, this, &SMainWindow::createTorus);

    auto* union_action = makeAction(tr("并集"), SIconId::BooleanUnion);
    auto* difference_action = makeAction(tr("差集"), SIconId::BooleanDifference);
    auto* intersection_action = makeAction(tr("交集"), SIconId::BooleanIntersection);
    auto* fillet_action = makeAction(tr("圆角"), SIconId::Fillet);
    auto* chamfer_action = makeAction(tr("倒角"), SIconId::Chamfer);
    auto* hole_action = makeAction(tr("孔"), SIconId::Hole);
    auto* transform_action = makeAction(tr("精确变换"), SIconId::Transform);
    auto* mirror_action = makeAction(tr("镜像"), SIconId::Mirror);
    auto* copy_action = makeAction(tr("普通复制"), SIconId::Copy, QStringLiteral("Ctrl+D"));
    auto* instance_copy_action =
        makeAction(tr("实例复制"), SIconId::Copy, QStringLiteral("Ctrl+Shift+D"));
    auto* linear_array_action = makeAction(tr("线性阵列"), SIconId::LinearArray);
    auto* polar_array_action = makeAction(tr("圆周阵列"), SIconId::PolarArray);
    auto* set_color_action = makeAction(tr("设置对象颜色"), SIconId::DisplayShaded);
    auto* restore_color_action = makeAction(tr("恢复导入颜色"), SIconId::SnapshotRestore);
    auto* delete_action = makeAction(tr("删除"), SIconId::Delete, QStringLiteral("Delete"));
    connect(union_action, &QAction::triggered, this,
            [this]()
            {
                runBoolean(SBooleanOperation::Union);
            });
    connect(difference_action, &QAction::triggered, this,
            [this]()
            {
                runBoolean(SBooleanOperation::Difference);
            });
    connect(intersection_action, &QAction::triggered, this,
            [this]()
            {
                runBoolean(SBooleanOperation::Intersection);
            });
    connect(fillet_action, &QAction::triggered, this, &SMainWindow::runFillet);
    connect(chamfer_action, &QAction::triggered, this, &SMainWindow::runChamfer);
    connect(hole_action, &QAction::triggered, this, &SMainWindow::runHole);
    connect(transform_action, &QAction::triggered, this, &SMainWindow::runTransform);
    connect(mirror_action, &QAction::triggered, this, &SMainWindow::runMirror);
    connect(copy_action, &QAction::triggered, this, &SMainWindow::duplicateSelection);
    connect(instance_copy_action, &QAction::triggered, this,
            &SMainWindow::duplicateSelectionShared);
    connect(linear_array_action, &QAction::triggered, this, &SMainWindow::runLinearArray);
    connect(polar_array_action, &QAction::triggered, this, &SMainWindow::runPolarArray);
    connect(set_color_action, &QAction::triggered, this, &SMainWindow::setSelectionColor);
    connect(restore_color_action, &QAction::triggered, this,
            &SMainWindow::restoreSelectionImportedColors);
    connect(delete_action, &QAction::triggered, this, &SMainWindow::deleteSelection);

    m_undo_action = makeAction(tr("撤销"), SIconId::Undo, QStringLiteral("Ctrl+Z"));
    m_redo_action = makeAction(tr("重做"), SIconId::Redo, QStringLiteral("Ctrl+Y"));
    m_undo_action->setEnabled(false);
    m_redo_action->setEnabled(false);
    bindCommand(m_undo_action, QStringLiteral("UNDO"),
                [this]()
                {
                    m_document.undo();
                });
    bindCommand(m_redo_action, QStringLiteral("REDO"),
                [this]()
                {
                    m_document.redo();
                });

    auto* measure_action = makeAction(tr("实体统计"), SIconId::MeasureStatistics);
    auto* sub_measure_action = makeAction(tr("点/边/面测量"), SIconId::MeasureSubElement);
    auto* distance_action = makeAction(tr("距离"), SIconId::MeasureDistance);
    auto* angle_action = makeAction(tr("角度"), SIconId::MeasureAngle);
    auto* section_action = makeAction(tr("生成截面"), SIconId::MeasureSection);
    auto* export_measurement_action = makeAction(tr("导出测量"), SIconId::MeasureExport);
    auto* screenshot_action = makeAction(tr("视口截图"), SIconId::ViewportScreenshot);
    auto* snapshot_action = makeAction(tr("创建快照"), SIconId::SnapshotCreate);
    auto* restore_action = makeAction(tr("恢复快照"), SIconId::SnapshotRestore);
    auto* branch_action = makeAction(tr("快照另存分支"), SIconId::SnapshotBranch);
    connect(measure_action, &QAction::triggered, this, &SMainWindow::measureSelection);
    connect(sub_measure_action, &QAction::triggered, this, &SMainWindow::measureSubSelection);
    connect(distance_action, &QAction::triggered, this, &SMainWindow::measureDistance);
    connect(angle_action, &QAction::triggered, this, &SMainWindow::measureAngle);
    connect(section_action, &QAction::triggered, this, &SMainWindow::createSection);
    connect(export_measurement_action, &QAction::triggered, this, &SMainWindow::exportMeasurements);
    connect(screenshot_action, &QAction::triggered, this, &SMainWindow::exportViewportImage);
    connect(snapshot_action, &QAction::triggered, this, &SMainWindow::createSnapshot);
    connect(restore_action, &QAction::triggered, this, &SMainWindow::restoreSnapshot);
    connect(branch_action, &QAction::triggered, this, &SMainWindow::saveSnapshotBranch);

    const QList<QAction*> file_actions = {new_action,     open_action,   save_action,
                                          save_as_action, import_action, export_action,
                                          archive_action};
    for (QAction* action : file_actions)
    {
        action->setProperty("ribbonPage", tr("文件"));
    }
    const QList<QAction*> primitive_actions = {box_action, cylinder_action, cone_action,
                                               sphere_action, torus_action};
    for (QAction* action : primitive_actions)
    {
        action->setProperty("ribbonPage", tr("建模"));
    }
    const QList<QAction*> modify_actions = {
        union_action,     difference_action,    intersection_action, fillet_action,
        chamfer_action,   hole_action,          transform_action,    mirror_action,
        copy_action,      instance_copy_action, linear_array_action, polar_array_action,
        set_color_action, restore_color_action, delete_action,       m_undo_action,
        m_redo_action};
    for (QAction* action : modify_actions)
    {
        action->setProperty("ribbonPage", tr("修改"));
    }
    for (QAction* action : {measure_action, sub_measure_action, distance_action, angle_action,
                            section_action, export_measurement_action, screenshot_action,
                            snapshot_action, restore_action, branch_action})
    {
        action->setProperty("ribbonPage", tr("测量"));
    }
}

void SMainWindow::createWorkspace()
{
    m_viewport_container = new QWidget(this);
    m_viewport_container->setObjectName(QStringLiteral("viewportContainer"));
    m_viewport_layout = new QGridLayout(m_viewport_container);
    m_viewport_layout->setContentsMargins(0, 0, 0, 0);
    m_viewport_layout->setSpacing(2);
    m_viewport = new SOccViewport(m_viewport_container);
    m_viewport->setDocument(&m_document);
    m_viewport_layout->addWidget(m_viewport, 0, 0);
    m_viewports.push_back(m_viewport);
    setCentralWidget(m_viewport_container);

    auto* scene_dock = new QDockWidget(tr("项目与场景树"), this);
    scene_dock->setObjectName(QStringLiteral("sceneDock"));
    scene_dock->setMinimumWidth(340);
    auto* scene_panel = new QWidget(scene_dock);
    auto* scene_layout = new QVBoxLayout(scene_panel);
    scene_layout->setContentsMargins(3, 3, 3, 3);
    scene_layout->setSpacing(3);
    m_scene_filter = new QLineEdit(scene_panel);
    m_scene_filter->setPlaceholderText(tr("搜索名称或对象类型…"));
    m_scene_tree = new QTreeWidget(scene_panel);
    m_scene_tree->setObjectName(QStringLiteral("sceneTree"));
    m_scene_tree->setColumnCount(3);
    m_scene_tree->setHeaderLabels({tr("对象"), tr("阶段"), tr("状态")});
    m_scene_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_scene_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_scene_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_scene_tree->setDefaultDropAction(Qt::MoveAction);
    m_scene_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    scene_layout->addWidget(m_scene_filter);
    scene_layout->addWidget(m_scene_tree);
    scene_dock->setWidget(scene_panel);
    addDockWidget(Qt::RightDockWidgetArea, scene_dock);

    auto* property_dock = new QDockWidget(tr("属性与质量信息"), this);
    property_dock->setObjectName(QStringLiteral("propertyDock"));
    property_dock->setMinimumWidth(340);
    m_property_tree = new QTreeWidget(property_dock);
    m_property_tree->setObjectName(QStringLiteral("propertyTree"));
    m_property_tree->setColumnCount(2);
    m_property_tree->setHeaderLabels({tr("属性"), tr("值")});
    m_property_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_property_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    property_dock->setWidget(m_property_tree);
    addDockWidget(Qt::RightDockWidgetArea, property_dock);
    splitDockWidget(scene_dock, property_dock, Qt::Vertical);

    auto* bottom_dock = new QDockWidget(tr("命令、任务、日志与历史"), this);
    bottom_dock->setObjectName(QStringLiteral("bottomDock"));
    m_bottom_tabs = new QTabWidget(bottom_dock);
    m_bottom_tabs->setObjectName(QStringLiteral("bottomTabs"));
    auto* command_page = new QWidget(m_bottom_tabs);
    auto* command_layout = new QVBoxLayout(command_page);
    command_layout->setContentsMargins(3, 3, 3, 3);
    m_console = new QTextEdit(command_page);
    m_console->setReadOnly(true);
    m_console->append(tr("smartGraphics3D 已就绪。输入 BOX、FIT、UNDO、SAVE 或 HELP。"));
    m_command_input = new QLineEdit(command_page);
    m_command_input->setObjectName(QStringLiteral("commandInput"));
    m_command_input->setPlaceholderText(tr("输入命令或按 Ctrl+K 搜索…"));
    auto* focus_command_action = new QAction(this);
    focus_command_action->setObjectName(QStringLiteral("focusCommandAction"));
    focus_command_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    focus_command_action->setShortcutContext(Qt::ApplicationShortcut);
    connect(focus_command_action, &QAction::triggered, m_command_input,
            qOverload<>(&QLineEdit::setFocus));
    addAction(focus_command_action);
    command_layout->addWidget(m_console);
    command_layout->addWidget(m_command_input);
    m_bottom_tabs->addTab(command_page, tr("命令"));

    m_task_table = new QTableWidget(0, 6, m_bottom_tabs);
    m_task_table->setObjectName(QStringLiteral("taskTable"));
    m_task_table->setHorizontalHeaderLabels(
        {tr("任务"), tr("步骤"), tr("进度"), tr("耗时"), tr("状态"), tr("操作")});
    m_task_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bottom_tabs->addTab(m_task_table, tr("任务"));

    m_log_view = new QTextEdit(m_bottom_tabs);
    m_log_view->setReadOnly(true);
    m_bottom_tabs->addTab(m_log_view, tr("日志"));

    m_history_table = new QTableWidget(0, 6, m_bottom_tabs);
    m_history_table->setHorizontalHeaderLabels(
        {tr("时间"), tr("操作"), tr("对象"), tr("参数摘要"), tr("状态"), tr("可撤销")});
    m_history_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bottom_tabs->addTab(m_history_table, tr("操作历史"));
    bottom_dock->setWidget(m_bottom_tabs);
    addDockWidget(Qt::BottomDockWidgetArea, bottom_dock);
    bottom_dock->setMinimumHeight(160);
}

void SMainWindow::createStatusBar()
{
    m_coordinate_label = new QLabel(tr("X 0.000  Y 0.000  Z 0.000 mm"), this);
    m_coordinate_label->setObjectName(QStringLiteral("cursorCoordinateLabel"));
    m_coordinate_system_label = new QLabel(tr("世界坐标系 · mm"), this);
    m_coordinate_system_label->setObjectName(QStringLiteral("coordinateSystemLabel"));
    m_selection_label = new QLabel(tr("对象选择"), this);
    m_statistics_label = new QLabel(tr("对象 0  面 0"), this);
    m_fps_label = new QLabel(tr("0 FPS"), this);
    m_task_label = new QLabel(tr("无后台任务"), this);
    m_dirty_label = new QLabel(tr("已保存"), this);
    statusBar()->addWidget(new QLabel(tr("就绪"), this));
    statusBar()->addPermanentWidget(m_coordinate_label);
    statusBar()->addPermanentWidget(m_coordinate_system_label);
    statusBar()->addPermanentWidget(m_selection_label);
    statusBar()->addPermanentWidget(m_statistics_label);
    statusBar()->addPermanentWidget(m_fps_label);
    statusBar()->addPermanentWidget(m_task_label);
    statusBar()->addPermanentWidget(m_dirty_label);
}

} // namespace smartGraphics3D
