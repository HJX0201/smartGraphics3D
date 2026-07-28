#pragma once

#include "s_3d_document.h"
#include "s_command_registry.h"
#include "s_kernel_service.h"
#include "s_occ_viewport.h"
#include "s_parameter_dialog.h"
#include "s_project_codec.h"
#include "s_standard_cad_codec.h"
#include "s_task_manager.h"

#include <QMainWindow>
#include <QMap>
#include <functional>
#include <memory>

class QAction;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QLineEdit;
class QGridLayout;
class QTableWidget;
class QTabWidget;
class QTextEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace smartGraphics3D
{
class SRibbonWidget;
enum class SIconId;

class SMainWindow final : public QMainWindow
{
    Q_OBJECT

  public:
    explicit SMainWindow(QWidget* parent = nullptr);
    ~SMainWindow() override;

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    void createActions();
    void createRibbon();
    void createViewRibbon();
    void createWorkspace();
    void createStatusBar();
    void connectSignals();
    void connectTaskSignals();
    void loadInterfacePreferences();
    void applyInterfaceScale(int percent);
    void applyDarkTheme();
    void showInterfaceScaleDialog();
    QAction* makeAction(const QString& text, SIconId icon_id, const QString& shortcut = {});
    void bindCommand(QAction* action, const QString& command_id,
                     const std::function<void()>& callback);
    void refreshSceneTree();
    void refreshProperties();
    void refreshHistory();
    void refreshWindowTitle();
    void appendLog(const QString& level, const QString& message, const QString& details = {},
                   SErrorCode error_code = SErrorCode::None);
    void showFailure(const QString& action, const SResult<void>& result);
    QList<SObjectId> selectedObjectIds() const;
    const SSceneObject* singleSelectedObject() const;

    bool confirmSaveChanges();
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    void saveProjectInBackground();
    void saveProjectAsInBackground();
    void queueProjectSave(const QString& file_path, bool mark_saved, const QString& task_name);
    void importCad();
    void exportSelected();
    void archiveProject();
    void autoSave();
    void checkRecovery();

    void createBox();
    void createCylinder();
    void createCone();
    void createSphere();
    void createTorus();
    void createPrimitiveDialog(
        const QString& title, const QList<SParameterField>& fields,
        std::function<SResult<SKernelShape>(const QMap<QString, double>&)> generator);
    void runBoolean(SBooleanOperation operation);
    void runFillet();
    void runChamfer();
    void runHole();
    void runTransform();
    void runMirror();
    void runLinearArray();
    void runPolarArray();
    void duplicateSelection();
    void duplicateSelectionShared();
    void duplicateSelectionWithMode(SCopyMode mode);
    void setSelectionColor();
    void restoreSelectionImportedColors();
    void deleteSelection();
    void measureSelection();
    void measureSubSelection();
    void measureDistance();
    void measureAngle();
    void createSection();
    void exportMeasurements();
    void exportViewportImage();
    void createSnapshot();
    void restoreSnapshot();
    void saveSnapshotBranch();
    void executeCommand(const QString& command);
    void setViewportLayout(int layout_mode);
    void setLengthUnit();
    void createUserCoordinateSystem();
    void createObjectCoordinateSystem();
    void showCoordinateSystems();
    void configureClipPlanes();
    void exportDiagnostics();

    SResult<SObjectId> addShape(const SKernelShape& shape, const QString& name,
                                SDataStage stage = SDataStage::Working, const QString& source = {},
                                const QString& parameter_summary = {});
    void addDerivedShape(const SKernelShape& shape, const QString& name,
                         const QList<SObjectId>& inputs, const QString& operation,
                         bool replace_inputs = false);
    bool confirmShapePreview(const SKernelShape& shape, const QString& operation,
                             const QList<SObjectId>& inputs, bool& replace_inputs);
    void runShapeTask(QString task_name, QList<SObjectId> inputs, QString result_name,
                      QString parameter_summary,
                      std::function<SResult<SKernelShape>(const STaskContext&)> work,
                      bool preserve_appearance = false);
    void runMultiShapeTask(QString task_name, SObjectId input, QString result_prefix,
                           QString parameter_summary,
                           std::function<SResult<QList<SKernelShape>>(const STaskContext&)> work,
                           SCopyMode copy_mode = SCopyMode::IndependentPresentation,
                           QList<QMatrix4x4> instance_transforms = {});
    void selectTreeItems(const QList<SObjectId>& ids);
    SResult<SKernelShape> materializedShape(const SSceneObject& object) const;

    std::unique_ptr<SIKernelService> m_kernel;
    S3dDocument m_document;
    SProjectCodec m_project_codec;
    SStandardCadCodec m_cad_codec;
    STaskManager m_task_manager;
    SCommandRegistry m_command_registry;

    SRibbonWidget* m_ribbon = nullptr;
    QWidget* m_viewport_container = nullptr;
    QGridLayout* m_viewport_layout = nullptr;
    SOccViewport* m_viewport = nullptr;
    QList<SOccViewport*> m_viewports;
    QList<SSelection> m_sub_selections;
    QTreeWidget* m_scene_tree = nullptr;
    QTreeWidget* m_property_tree = nullptr;
    QTabWidget* m_bottom_tabs = nullptr;
    QTextEdit* m_console = nullptr;
    QLineEdit* m_command_input = nullptr;
    QLineEdit* m_scene_filter = nullptr;
    QTableWidget* m_task_table = nullptr;
    QTableWidget* m_history_table = nullptr;
    QTextEdit* m_log_view = nullptr;
    QLabel* m_coordinate_label = nullptr;
    QLabel* m_coordinate_system_label = nullptr;
    QLabel* m_selection_label = nullptr;
    QLabel* m_statistics_label = nullptr;
    QLabel* m_fps_label = nullptr;
    QLabel* m_task_label = nullptr;
    QLabel* m_dirty_label = nullptr;
    QTimer* m_auto_save_timer = nullptr;

    QAction* m_undo_action = nullptr;
    QAction* m_redo_action = nullptr;
    int m_interface_scale_percent = 100;
    bool m_refreshing_tree = false;
    bool m_synchronizing_selection = false;
    bool m_synchronizing_camera = false;
    bool m_sync_selections = true;
    bool m_sync_cameras = true;
    QList<SClipPlane> m_clip_planes;
    QStringList m_structured_logs;
};
} // namespace smartGraphics3D
