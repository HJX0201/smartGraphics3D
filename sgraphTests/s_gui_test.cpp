#include "s_clip_plane_dialog.h"
#include "s_dialog_size_policy.h"
#include "s_icon_factory.h"
#include "s_interface_scale.h"
#include "s_main_window.h"
#include "s_occ_viewport.h"
#include "s_parameter_dialog.h"
#include "s_ribbon_widget.h"
#include "s_task_manager.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QThread>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <memory>

namespace smartGraphics3D
{
namespace
{
QAction* actionByText(QObject& object, const QString& text)
{
    for (QAction* action : object.findChildren<QAction*>())
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

QToolButton* ribbonButtonByText(QObject& object, const QString& text)
{
    for (QToolButton* button : object.findChildren<QToolButton*>())
    {
        if (button->defaultAction() && button->defaultAction()->text() == text)
        {
            return button;
        }
    }
    return nullptr;
}

QByteArray iconDigest(const QIcon& icon)
{
    const QImage image =
        icon.pixmap(QSize(32, 32)).toImage().convertToFormat(QImage::Format_ARGB32);
    const auto* bytes = reinterpret_cast<const char*>(image.constBits());
    return QCryptographicHash::hash(QByteArray(bytes, static_cast<int>(image.sizeInBytes())),
                                    QCryptographicHash::Sha256);
}
} // namespace

class SGuiTest final : public QObject
{
    Q_OBJECT

  private slots:
    void buildsRequiredRibbonAndPanels()
    {
        SMainWindow window;
        auto* ribbon = window.findChild<SRibbonWidget*>(QStringLiteral("sgraphRibbon"));
        QVERIFY(ribbon);
        QCOMPARE(
            ribbon->pageNames(),
            QStringList({QStringLiteral("文件"), QStringLiteral("建模"), QStringLiteral("修改"),
                         QStringLiteral("测量"), QStringLiteral("视图"), QStringLiteral("帮助")}));
        QCOMPARE(ribbon->pageRowCount(QStringLiteral("视图")), 2);
        QVERIFY(window.findChild<QTreeWidget*>(QStringLiteral("sceneTree")));
        QVERIFY(window.findChild<QTreeWidget*>(QStringLiteral("propertyTree")));
        QVERIFY(window.findChild<QTabWidget*>(QStringLiteral("bottomTabs")));
        QVERIFY(window.findChild<QLineEdit*>(QStringLiteral("commandInput")));
        QVERIFY(window.findChild<QAction*>(QStringLiteral("focusCommandAction")));
        QVERIFY(actionByText(window, QStringLiteral("撤销")));
        QVERIFY(!actionByText(window, QStringLiteral("撤销"))->isEnabled());
        QVERIFY(actionByText(window, QStringLiteral("着色实体")));
        QVERIFY(actionByText(window, QStringLiteral("隐藏线")));
        QVERIFY(actionByText(window, QStringLiteral("适合选中")));
        QVERIFY(actionByText(window, QStringLiteral("自由旋转")));
        QVERIFY(actionByText(window, QStringLiteral("自由旋转"))->isCheckable());
        QVERIFY(actionByText(window, QStringLiteral("全选")));
        QVERIFY(actionByText(window, QStringLiteral("反选")));
        QVERIFY(actionByText(window, QStringLiteral("项目单位")));
        QVERIFY(actionByText(window, QStringLiteral("用户坐标系")));
        QVERIFY(actionByText(window, QStringLiteral("对象坐标系")));
        QVERIFY(actionByText(window, QStringLiteral("坐标系列表")));
        QVERIFY(actionByText(window, QStringLiteral("快照另存分支")));
        QVERIFY(actionByText(window, QStringLiteral("界面比例")));
        QVERIFY(actionByText(window, QStringLiteral("实例复制")));
        QVERIFY(!actionByText(window, QStringLiteral("界面比例"))->icon().isNull());
        QVERIFY(!window.windowIcon().isNull());
        QCOMPARE(actionByText(window, QStringLiteral("长方体"))->property("commandId").toString(),
                 QStringLiteral("BOX"));
        QCOMPARE(actionByText(window, QStringLiteral("适合窗口"))->property("commandId").toString(),
                 QStringLiteral("FIT"));
        QVERIFY(!actionByText(window, QStringLiteral("长方体"))->icon().isNull());
        QVERIFY(iconDigest(actionByText(window, QStringLiteral("长方体"))->icon()) !=
                iconDigest(actionByText(window, QStringLiteral("球体"))->icon()));
        auto* coordinate_system =
            window.findChild<QLabel*>(QStringLiteral("coordinateSystemLabel"));
        QVERIFY(coordinate_system);
        QVERIFY(coordinate_system->text().contains(QStringLiteral("世界坐标系")));
        QVERIFY(coordinate_system->text().contains(QStringLiteral("mm")));
    }

    void interfaceScaleSettingsValidateBoundariesAndCorruption()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("appearance.ini")),
                           QSettings::IniFormat);

        const SResult<int> default_result = readInterfaceScalePercent(settings);
        QVERIFY(default_result);
        QCOMPARE(default_result.value(), kDefaultInterfaceScalePercent);

        QCOMPARE(supportedInterfaceScalePercents(), QList<int>({75, 90, 100, 125, 150}));
        QVERIFY(writeInterfaceScalePercent(settings, 75));
        QCOMPARE(readInterfaceScalePercent(settings).value(), 75);
        QVERIFY(writeInterfaceScalePercent(settings, 150));
        QCOMPARE(readInterfaceScalePercent(settings).value(), 150);

        const SResult<void> invalid_write = writeInterfaceScalePercent(settings, 200);
        QVERIFY(!invalid_write);
        QCOMPARE(invalid_write.errorCode(), SErrorCode::InvalidArgument);

        settings.clear();
        settings.setValue(QStringLiteral("appearance/interfaceScalePercent"), 125);
        const SResult<int> migrated_read = readInterfaceScalePercent(settings);
        QVERIFY(migrated_read);
        QCOMPARE(migrated_read.value(), 100);
        QCOMPARE(settings.value(QStringLiteral("appearance/interfaceScaleVersion")).toInt(),
                 kInterfaceScaleSettingsVersion);
        QCOMPARE(settings.value(QStringLiteral("appearance/interfaceScalePercent")).toInt(), 100);

        settings.clear();
        settings.setValue(QStringLiteral("appearance/interfaceScalePercent"), 100);
        QCOMPARE(readInterfaceScalePercent(settings).value(), 75);

        settings.clear();
        settings.setValue(QStringLiteral("appearance/interfaceScalePercent"),
                          QStringLiteral("invalid"));
        const SResult<int> corrupt_read = readInterfaceScalePercent(settings);
        QVERIFY(!corrupt_read);
        QCOMPARE(corrupt_read.errorCode(), SErrorCode::CorruptData);
    }

    void interfaceScaleDialogPreviewsPresetsAndRibbonResizes()
    {
        SInterfaceScaleDialog dialog(100);
        auto* scale_group =
            dialog.findChild<QButtonGroup*>(QStringLiteral("interfaceScaleButtonGroup"));
        QVERIFY(scale_group);
        QCOMPARE(scale_group->buttons().size(), 5);
        QList<int> option_values;
        for (QAbstractButton* option : scale_group->buttons())
        {
            option_values.push_back(scale_group->id(option));
            QVERIFY(option->property("scaleOption").toBool());
        }
        std::sort(option_values.begin(), option_values.end());
        QCOMPARE(option_values, QList<int>({75, 90, 100, 125, 150}));
        QVERIFY(!dialog.findChild<QComboBox*>(QStringLiteral("interfaceScaleCombo")));
        QCOMPARE(dialog.selectedPercent(), 100);
        const int default_dialog_width = dialog.minimumWidth();
        const int default_option_height = scale_group->button(100)->minimumHeight();
        QSignalSpy preview_spy(&dialog, &SInterfaceScaleDialog::scalePreviewed);
        scale_group->button(125)->click();
        QCOMPARE(dialog.selectedPercent(), 125);
        QCOMPARE(preview_spy.count(), 1);
        QVERIFY(dialog.minimumWidth() > default_dialog_width);
        QVERIFY(scale_group->button(125)->minimumHeight() > default_option_height);
        scale_group->button(150)->click();
        QCoreApplication::processEvents();
        QVERIFY(dialog.minimumWidth() >= qRound(default_dialog_width * 1.5));
        QVERIFY(scale_group->button(150)->minimumHeight() >= qRound(default_option_height * 1.5));

        SRibbonWidget ribbon;
        QCOMPARE(interfaceScaleRenderPercent(100), 125);
        const int default_height = ribbon.height();
        ribbon.setInterfaceScalePercent(150);
        QCOMPARE(ribbon.interfaceScalePercent(), 150);
        QVERIFY(ribbon.height() > default_height);
        ribbon.setInterfaceScalePercent(50);
        QCOMPARE(ribbon.interfaceScalePercent(), 100);
    }

    void dialogsHaveScaledMinimumSizes()
    {
        QInputDialog input_dialog;
        QMessageBox message_dialog;
        QFileDialog file_dialog;
        QDialog custom_dialog;

        applyDialogMinimumSize(input_dialog, 100);
        applyDialogMinimumSize(message_dialog, 100);
        applyDialogMinimumSize(file_dialog, 100);
        applyDialogMinimumSize(custom_dialog, 100);
        QCOMPARE(input_dialog.minimumSize(), QSize(440, 220));
        QCOMPARE(message_dialog.minimumSize(), QSize(460, 200));
        QCOMPARE(file_dialog.minimumSize(), QSize(760, 500));
        QCOMPARE(custom_dialog.minimumSize(), QSize(480, 280));

        applyDialogMinimumSize(input_dialog, 150);
        applyDialogMinimumSize(message_dialog, 150);
        applyDialogMinimumSize(custom_dialog, 150);
        QCOMPARE(input_dialog.minimumSize(), QSize(660, 330));
        QCOMPARE(message_dialog.minimumSize(), QSize(690, 300));
        QCOMPARE(custom_dialog.minimumSize(), QSize(720, 420));

        setDialogSizePolicyPercent(100);
        QInputDialog automatic_dialog;
        automatic_dialog.show();
        QCoreApplication::processEvents();
        QCOMPARE(automatic_dialog.minimumSize(), QSize(440, 220));
        automatic_dialog.hide();
    }

    void ribbonIconsAreDistinctWithinCommandGroups()
    {
        SMainWindow window;
        const QList<QStringList> groups = {
            {QStringLiteral("新建"), QStringLiteral("打开"), QStringLiteral("保存"),
             QStringLiteral("另存为"), QStringLiteral("导入 CAD"), QStringLiteral("导出选中"),
             QStringLiteral("项目归档")},
            {QStringLiteral("并集"), QStringLiteral("差集"), QStringLiteral("交集"),
             QStringLiteral("圆角"), QStringLiteral("倒角"), QStringLiteral("孔"),
             QStringLiteral("精确变换"), QStringLiteral("镜像"), QStringLiteral("普通复制"),
             QStringLiteral("线性阵列"), QStringLiteral("圆周阵列"), QStringLiteral("删除"),
             QStringLiteral("撤销"), QStringLiteral("重做")},
            {QStringLiteral("实体统计"), QStringLiteral("点/边/面测量"), QStringLiteral("距离"),
             QStringLiteral("角度"), QStringLiteral("生成截面"), QStringLiteral("导出测量"),
             QStringLiteral("视口截图"), QStringLiteral("创建快照"), QStringLiteral("恢复快照"),
             QStringLiteral("快照另存分支")},
            {QStringLiteral("前视"), QStringLiteral("后视"), QStringLiteral("左视"),
             QStringLiteral("右视"), QStringLiteral("顶视"), QStringLiteral("底视")},
            {QStringLiteral("线框"), QStringLiteral("着色实体"), QStringLiteral("着色边线"),
             QStringLiteral("隐藏线"), QStringLiteral("半透明")}};

        for (const QStringList& group : groups)
        {
            QSet<QByteArray> digests;
            for (const QString& text : group)
            {
                QAction* action = actionByText(window, text);
                QVERIFY2(action, qPrintable(text));
                QVERIFY2(!action->icon().isNull(), qPrintable(text));
                const QByteArray digest = iconDigest(action->icon());
                QVERIFY2(!digest.isEmpty(), qPrintable(text));
                QVERIFY2(!digests.contains(digest), qPrintable(text));
                digests.insert(digest);
            }
        }
    }

    void viewRibbonUsesTwoRowsSizesStatesAndOverflow()
    {
        SMainWindow window;
        auto* ribbon = window.findChild<SRibbonWidget*>(QStringLiteral("sgraphRibbon"));
        auto* tabs = ribbon ? ribbon->findChild<QTabWidget*>() : nullptr;
        QVERIFY(ribbon);
        QVERIFY(tabs);
        QCOMPARE(ribbon->pageRowCount(QStringLiteral("视图")), 2);

        QToolButton* front = ribbonButtonByText(*ribbon, QStringLiteral("前视"));
        QToolButton* projection = ribbonButtonByText(*ribbon, QStringLiteral("正交/透视"));
        QVERIFY(front);
        QVERIFY(projection);
        QCOMPARE(front->property("ribbonRow").toInt(), 0);
        QCOMPARE(front->property("ribbonButtonSize").toString(), QStringLiteral("compact"));
        QCOMPARE(projection->property("ribbonRow").toInt(), 1);
        QCOMPARE(projection->property("ribbonButtonSize").toString(), QStringLiteral("standard"));
        QVERIFY(front->iconSize().width() < projection->iconSize().width());

        QAction* object_action = actionByText(window, QStringLiteral("对象"));
        QAction* face_action = actionByText(window, QStringLiteral("面"));
        QVERIFY(object_action);
        QVERIFY(face_action);
        QVERIFY(object_action->isCheckable());
        QVERIFY(object_action->isChecked());
        face_action->trigger();
        QVERIFY(face_action->isChecked());
        QVERIFY(!object_action->isChecked());

        int view_index = -1;
        for (int index = 0; index < tabs->count(); ++index)
        {
            if (tabs->tabText(index) == QStringLiteral("视图"))
            {
                view_index = index;
                break;
            }
        }
        QVERIFY(view_index >= 0);
        tabs->setCurrentIndex(view_index);
        window.resize(2048, 1440);
        window.show();
        QCoreApplication::processEvents();
        auto* scroll_area =
            tabs->widget(view_index)->findChild<QScrollArea*>(QStringLiteral("ribbonScrollArea"));
        QVERIFY(scroll_area);
        QCOMPARE(scroll_area->horizontalScrollBar()->maximum(), 0);

        ribbon->setInterfaceScalePercent(150);
        window.resize(800, 720);
        QCoreApplication::processEvents();
        QVERIFY(scroll_area->horizontalScrollBar()->maximum() > 0);
    }

    void switchesMultiViewportLayoutsAndScales()
    {
        SMainWindow window;
        window.resize(1280, 720);
        QAction* four_views = actionByText(window, QStringLiteral("四视图"));
        QAction* single_view = actionByText(window, QStringLiteral("单视口"));
        QAction* sync_selection = actionByText(window, QStringLiteral("同步选择"));
        QVERIFY(four_views);
        QVERIFY(single_view);
        QVERIFY(sync_selection);
        QVERIFY(sync_selection->isChecked());
        four_views->trigger();
        QCOMPARE(window.findChildren<SOccViewport*>().size(), 4);
        window.resize(1920, 1080);
        QVERIFY(window.centralWidget()->minimumSize().width() <=
                window.centralWidget()->size().width());
        single_view->trigger();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(window.findChildren<SOccViewport*>().size(), 1);
    }

    void parameterDialogProvidesLevelsValidationResetAndPreview()
    {
        int preview_count = 0;
        SParameterDialog dialog(
            QStringLiteral("参数"),
            {{QStringLiteral("length"), QStringLiteral("长度"), QStringLiteral("mm"),
              QStringLiteral("影响几何长度"), 10.0, 1.0, 100.0, 3, false},
             {QStringLiteral("tolerance"), QStringLiteral("容差"), QStringLiteral("mm"),
              QStringLiteral("影响计算精度"), 0.1, 0.001, 1.0, 3, true}},
            [&preview_count](const QMap<QString, double>&)
            {
                ++preview_count;
            });
        auto* tabs = dialog.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        auto* length = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("parameter_length"));
        QVERIFY(length);
        QCOMPARE(length->minimum(), 1.0);
        QCOMPARE(length->maximum(), 100.0);
        length->setValue(20.0);
        QVERIFY(preview_count >= 2);
        QCOMPARE(dialog.values().value(QStringLiteral("length")), 20.0);
    }

    void commandLineUsesSameApplicationActions()
    {
        SMainWindow window;
        auto* input = window.findChild<QLineEdit*>(QStringLiteral("commandInput"));
        QVERIFY(input);
        input->setText(QStringLiteral("HELP"));
        QTest::keyClick(input, Qt::Key_Return);
        bool found_help = false;
        for (QTextEdit* edit : window.findChildren<QTextEdit*>())
        {
            found_help = found_help || edit->toPlainText().contains(QStringLiteral("命令：BOX"));
        }
        QVERIFY(found_help);
    }

    void clipPlaneDialogProvidesMultiPlaneDragPreview()
    {
        SUnitSystem units;
        int preview_count = 0;
        SClipPlaneDialog dialog(units, {{QVector3D(0.0F, 0.0F, 1.0F), 5.0, false}},
                                [&preview_count](const QList<SClipPlane>&)
                                {
                                    ++preview_count;
                                });
        auto* count = dialog.findChild<QSpinBox*>(QStringLiteral("clipPlaneCount"));
        auto* slider = dialog.findChild<QSlider*>(QStringLiteral("clipOffsetSlider1"));
        QVERIFY(count);
        QVERIFY(slider);
        QCOMPARE(count->maximum(), 3);
        slider->setValue(250);
        QVERIFY(preview_count >= 2);
        QCOMPARE(dialog.planes().size(), 1);
        QCOMPARE(dialog.planes().front().offset, 25.0);
    }

    void taskPanelCancelsWithoutBlockingViewport()
    {
        SMainWindow window;
        auto* manager = window.findChild<STaskManager*>();
        auto* table = window.findChild<QTableWidget*>(QStringLiteral("taskTable"));
        QVERIFY(manager);
        QVERIFY(table);
        QCOMPARE(table->columnCount(), 6);

        manager->run(QStringLiteral("取消测试"),
                     [](const STaskContext& context)
                     {
                         while (!context.isCancellationRequested())
                         {
                             QThread::msleep(2);
                         }
                         return SResult<void>::failure(SErrorCode::Cancelled,
                                                       QStringLiteral("已取消"));
                     });
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 1, 3000);
        auto* cancel = qobject_cast<QPushButton*>(table->cellWidget(0, 5));
        QVERIFY(cancel);
        QCOMPARE(cancel->text(), QStringLiteral("取消"));
        cancel->click();
        QTRY_VERIFY_WITH_TIMEOUT(table->item(0, 4)->text().contains(QStringLiteral("取消")), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(manager->runningTaskCount(), 0, 3000);
        QVERIFY(window.findChild<SOccViewport*>());
    }

    void taskPanelRetriesFailedTask()
    {
        SMainWindow window;
        auto* manager = window.findChild<STaskManager*>();
        auto* table = window.findChild<QTableWidget*>(QStringLiteral("taskTable"));
        QVERIFY(manager);
        QVERIFY(table);
        auto attempts = std::make_shared<std::atomic_int>(0);
        manager->run(QStringLiteral("重试测试"),
                     [attempts](const STaskContext&)
                     {
                         return ++(*attempts) == 1
                                    ? SResult<void>::failure(SErrorCode::InternalFailure,
                                                             QStringLiteral("首次失败"),
                                                             QStringLiteral("诊断详情"))
                                    : SResult<void>::success();
                     });
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(qobject_cast<QPushButton*>(table->cellWidget(0, 5)) != nullptr &&
                                     qobject_cast<QPushButton*>(table->cellWidget(0, 5))->text() ==
                                         QStringLiteral("重试"),
                                 3000);
        auto* retry = qobject_cast<QPushButton*>(table->cellWidget(0, 5));
        retry->click();
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 2, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(table->item(1, 4)->text(), QStringLiteral("成功"), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(manager->runningTaskCount(), 0, 3000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(window.findChild<SOccViewport*>());
    }
};
} // namespace smartGraphics3D

QTEST_MAIN(smartGraphics3D::SGuiTest)
#include "s_gui_test.moc"
