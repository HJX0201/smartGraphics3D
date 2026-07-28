#pragma once

#include <QMap>
#include <QWidget>

class QAction;
class QHBoxLayout;
class QScrollArea;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

namespace smartGraphics3D
{
enum class SRibbonButtonSize
{
    Compact,
    Standard,
    Primary
};

class SRibbonWidget final : public QWidget
{
    Q_OBJECT

  public:
    explicit SRibbonWidget(QWidget* parent = nullptr);

    void addAction(const QString& page_name, const QString& group_name, QAction* action);
    void addAction(const QString& page_name, const QString& group_name, QAction* action,
                   int row_index, SRibbonButtonSize button_size, bool show_group_title);
    [[nodiscard]] QStringList pageNames() const;
    [[nodiscard]] int pageRowCount(const QString& page_name) const;
    void setInterfaceScalePercent(int percent);
    [[nodiscard]] int interfaceScalePercent() const;

  private:
    struct SRow
    {
        QWidget* widget = nullptr;
        QHBoxLayout* layout = nullptr;
        QMap<QString, QHBoxLayout*> groups;
    };

    struct SPage
    {
        QWidget* widget = nullptr;
        QScrollArea* scroll_area = nullptr;
        QWidget* content_widget = nullptr;
        QVBoxLayout* layout = nullptr;
        QMap<int, SRow> rows;
    };

    struct SButton
    {
        QToolButton* widget = nullptr;
        SRibbonButtonSize size = SRibbonButtonSize::Primary;
    };

    SPage& ensurePage(const QString& page_name);
    SRow& ensureRow(SPage& page, int row_index);
    QHBoxLayout* ensureGroup(SRow& row, const QString& group_name, bool show_title);
    void applyButtonMetrics(SButton& button);
    void updatePageMinimumWidth(SPage& page);
    void updateRibbonHeight();

    QTabWidget* m_tabs = nullptr;
    QMap<QString, SPage> m_pages;
    QList<SButton> m_buttons;
    int m_interface_scale_percent = 100;
};
} // namespace smartGraphics3D
