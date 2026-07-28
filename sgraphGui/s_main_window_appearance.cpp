#include "s_dialog_size_policy.h"
#include "s_interface_scale.h"
#include "s_main_window.h"
#include "s_ribbon_widget.h"

#include <QApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QtMath>

namespace smartGraphics3D
{
namespace
{
int scaledPixels(int pixels, int percent)
{
    return qMax(1, qRound(static_cast<double>(pixels) * percent / 100.0));
}
} // namespace

void SMainWindow::loadInterfacePreferences()
{
    QSettings settings;
    const SResult<int> result = readInterfaceScalePercent(settings);
    if (result)
    {
        applyInterfaceScale(result.value());
        return;
    }

    applyInterfaceScale(kDefaultInterfaceScalePercent);
    appendLog(QStringLiteral("WARN"), result.message(), result.details(), result.errorCode());
}

void SMainWindow::applyInterfaceScale(int percent)
{
    if (!isSupportedInterfaceScalePercent(percent))
    {
        percent = kDefaultInterfaceScalePercent;
    }
    m_interface_scale_percent = percent;
    const int render_percent = interfaceScaleRenderPercent(percent);

    setDialogSizePolicyPercent(percent);
    m_ribbon->setInterfaceScalePercent(percent);
    if (auto* scene_dock = findChild<QDockWidget*>(QStringLiteral("sceneDock")))
    {
        scene_dock->setMinimumWidth(scaledPixels(340, render_percent));
    }
    if (auto* property_dock = findChild<QDockWidget*>(QStringLiteral("propertyDock")))
    {
        property_dock->setMinimumWidth(scaledPixels(340, render_percent));
    }
    if (auto* bottom_dock = findChild<QDockWidget*>(QStringLiteral("bottomDock")))
    {
        bottom_dock->setMinimumHeight(scaledPixels(160, render_percent));
    }

    applyDarkTheme();
    updateGeometry();
}

void SMainWindow::showInterfaceScaleDialog()
{
    const int original_percent = m_interface_scale_percent;
    SInterfaceScaleDialog dialog(original_percent, this);
    connect(&dialog, &SInterfaceScaleDialog::scalePreviewed, this,
            [this](int percent)
            {
                applyInterfaceScale(percent);
            });

    if (dialog.exec() != QDialog::Accepted)
    {
        applyInterfaceScale(original_percent);
        return;
    }

    const int selected_percent = dialog.selectedPercent();
    QSettings settings;
    const SResult<void> result = writeInterfaceScalePercent(settings, selected_percent);
    if (!result)
    {
        applyInterfaceScale(original_percent);
        QMessageBox::warning(this, tr("界面比例"),
                             result.message() + QStringLiteral("\n") + result.details());
        appendLog(QStringLiteral("ERROR"), result.message(), result.details(), result.errorCode());
        return;
    }

    applyInterfaceScale(selected_percent);
    statusBar()->showMessage(tr("界面比例已保存为 %1%。").arg(selected_percent), 4000);
}

void SMainWindow::applyDarkTheme()
{
    const int render_percent = interfaceScaleRenderPercent(m_interface_scale_percent);
    const int font_size = scaledPixels(13, render_percent);
    const int small_font_size = scaledPixels(11, render_percent);
    const int compact_font_size = scaledPixels(10, render_percent);
    const int standard_font_size = scaledPixels(11, render_percent);
    const int tab_vertical_padding = scaledPixels(8, render_percent);
    const int tab_horizontal_padding = scaledPixels(18, render_percent);
    const int tool_padding = scaledPixels(3, render_percent);
    const int header_padding = scaledPixels(6, render_percent);
    const int button_vertical_padding = scaledPixels(6, render_percent);
    const int button_horizontal_padding = scaledPixels(12, render_percent);
    const int scroll_bar_size = scaledPixels(10, render_percent);
    const int scroll_handle_size = scaledPixels(24, render_percent);
    const int status_label_padding = scaledPixels(5, render_percent);

    qApp->setStyleSheet(QStringLiteral(R"(
QWidget { color:#dce7ee; background:#15212c; font-family:"Microsoft YaHei UI"; font-size:%1px; }
QMainWindow, QDockWidget { background:#0e1821; }
#sgraphRibbon { background:#182632; border-bottom:1px solid #2a3b49; }
QTabWidget::pane { border:1px solid #293a47; background:#182632; }
QTabBar::tab { background:#101b25; color:#8fa7b9; padding:%2px %3px; border:0; }
QTabBar::tab:selected { background:#1c2c39; color:#e6f3fa; border-bottom:2px solid #2196cf; }
QGroupBox { border:0; border-right:1px solid #2b3b48; margin-top:0; padding-top:0; }
QGroupBox::title { subcontrol-origin:margin; subcontrol-position:bottom center; color:#758c9d; }
QToolButton { background:transparent; border:1px solid transparent; padding:%4px; }
QToolButton[ribbonButtonSize="compact"] { font-size:%12px; padding:1px; }
QToolButton[ribbonButtonSize="standard"] { font-size:%13px; padding:2px; }
QToolButton:hover { background:#243744; border-color:#3b5567; }
QToolButton:pressed, QToolButton:checked { background:#218fc8; color:white; }
QTreeWidget, QTableWidget, QTextEdit, QLineEdit, QComboBox {
  background:#101a23; alternate-background-color:#14222d; border:1px solid #293b49;
  selection-background-color:#1f79a8; selection-color:white;
}
QHeaderView::section {
  background:#1b2a36; color:#8fa7b9; border:0; border-right:1px solid #2b3d4b;
  padding:%5px;
}
QDockWidget::title { background:#14222d; color:#a9bfce; padding:%5px; }
QStatusBar {
  background:#0d1821; color:#9db3c2; border-top:1px solid #29404f; font-size:%6px;
}
QStatusBar QLabel { padding-left:%7px; padding-right:%7px; }
QPushButton { background:#203340; border:1px solid #385365; padding:%8px %9px; }
QPushButton:hover { background:#285069; }
QPushButton:checked { background:#218fc8; border-color:#55b8e7; color:white; }
QPushButton[scaleOption="true"] { background:#172630; border-color:#334a59; }
QPushButton[scaleOption="true"]:hover { background:#234154; border-color:#4a7088; }
QPushButton[scaleOption="true"]:checked {
  background:#1f83b5; border:2px solid #65c4ef; color:white;
}
QScrollBar { background:#101a23; width:%10px; height:%10px; }
QScrollBar::handle { background:#385062; min-height:%11px; min-width:%11px; }
QToolTip { color:#e6f3fa; background:#1d2e3a; border:1px solid #3d5668; }
)")
                            .arg(font_size)
                            .arg(tab_vertical_padding)
                            .arg(tab_horizontal_padding)
                            .arg(tool_padding)
                            .arg(header_padding)
                            .arg(small_font_size)
                            .arg(status_label_padding)
                            .arg(button_vertical_padding)
                            .arg(button_horizontal_padding)
                            .arg(scroll_bar_size)
                            .arg(scroll_handle_size)
                            .arg(compact_font_size)
                            .arg(standard_font_size));
}
} // namespace smartGraphics3D
