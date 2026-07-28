#include "s_ribbon_widget.h"

#include "s_interface_scale.h"

#include <QAction>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace smartGraphics3D
{
namespace
{
int scaledPixels(int pixels, int percent)
{
    return qMax(1, qRound(static_cast<double>(pixels) * percent / 100.0));
}

QString buttonSizeName(SRibbonButtonSize size)
{
    switch (size)
    {
    case SRibbonButtonSize::Compact:
        return QStringLiteral("compact");
    case SRibbonButtonSize::Standard:
        return QStringLiteral("standard");
    case SRibbonButtonSize::Primary:
        return QStringLiteral("primary");
    }
    return QStringLiteral("primary");
}
} // namespace

SRibbonWidget::SRibbonWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("sgraphRibbon"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setExpanding(false);
    layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this,
            [this](int)
            {
                updateRibbonHeight();
            });
    updateRibbonHeight();
}

void SRibbonWidget::addAction(const QString& page_name, const QString& group_name, QAction* action)
{
    addAction(page_name, group_name, action, 0, SRibbonButtonSize::Primary, true);
}

void SRibbonWidget::addAction(const QString& page_name, const QString& group_name, QAction* action,
                              int row_index, SRibbonButtonSize button_size, bool show_group_title)
{
    SPage& page = ensurePage(page_name);
    SRow& row = ensureRow(page, qMax(0, row_index));
    QHBoxLayout* group_layout = ensureGroup(row, group_name, show_group_title);
    auto* button = new QToolButton(row.widget);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(true);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setProperty("ribbonPage", page_name);
    button->setProperty("ribbonRow", row_index);
    button->setProperty("ribbonButtonSize", buttonSizeName(button_size));
    m_buttons.push_back({button, button_size});
    applyButtonMetrics(m_buttons.back());
    group_layout->addWidget(button);
    updatePageMinimumWidth(page);
    updateRibbonHeight();
}

QStringList SRibbonWidget::pageNames() const
{
    QStringList names;
    for (int index = 0; index < m_tabs->count(); ++index)
    {
        names.push_back(m_tabs->tabText(index));
    }
    return names;
}

int SRibbonWidget::pageRowCount(const QString& page_name) const
{
    const auto iterator = m_pages.constFind(page_name);
    return iterator == m_pages.cend() ? 0 : iterator->rows.size();
}

void SRibbonWidget::setInterfaceScalePercent(int percent)
{
    m_interface_scale_percent =
        isSupportedInterfaceScalePercent(percent) ? percent : kDefaultInterfaceScalePercent;

    for (SButton& button : m_buttons)
    {
        applyButtonMetrics(button);
    }

    const int render_percent = interfaceScaleRenderPercent(m_interface_scale_percent);
    for (SPage& page : m_pages)
    {
        page.layout->setContentsMargins(
            scaledPixels(5, render_percent), scaledPixels(2, render_percent),
            scaledPixels(5, render_percent), scaledPixels(2, render_percent));
        page.layout->setSpacing(scaledPixels(1, render_percent));
        for (SRow& row : page.rows)
        {
            row.layout->setContentsMargins(0, 0, 0, 0);
            row.layout->setSpacing(scaledPixels(2, render_percent));
            for (QHBoxLayout* group_layout : row.groups)
            {
                const bool show_title = group_layout->property("ribbonShowTitle").toBool();
                group_layout->setContentsMargins(scaledPixels(3, render_percent),
                                                 scaledPixels(1, render_percent),
                                                 scaledPixels(3, render_percent),
                                                 scaledPixels(show_title ? 8 : 1, render_percent));
                group_layout->setSpacing(scaledPixels(1, render_percent));
            }
        }
        updatePageMinimumWidth(page);
    }
    updateRibbonHeight();
    updateGeometry();
}

int SRibbonWidget::interfaceScalePercent() const
{
    return m_interface_scale_percent;
}

SRibbonWidget::SPage& SRibbonWidget::ensurePage(const QString& page_name)
{
    auto iterator = m_pages.find(page_name);
    if (iterator != m_pages.end())
    {
        return iterator.value();
    }

    SPage page;
    page.widget = new QWidget(m_tabs);
    page.widget->setProperty("ribbonPageName", page_name);
    auto* page_layout = new QVBoxLayout(page.widget);
    page_layout->setContentsMargins(0, 0, 0, 0);
    page_layout->setSpacing(0);

    page.scroll_area = new QScrollArea(page.widget);
    page.scroll_area->setObjectName(QStringLiteral("ribbonScrollArea"));
    page.scroll_area->setFrameShape(QFrame::NoFrame);
    page.scroll_area->setWidgetResizable(true);
    page.scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    page.scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    page_layout->addWidget(page.scroll_area);

    page.content_widget = new QWidget(page.scroll_area);
    page.content_widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    page.layout = new QVBoxLayout(page.content_widget);
    const int render_percent = interfaceScaleRenderPercent(m_interface_scale_percent);
    page.layout->setContentsMargins(
        scaledPixels(5, render_percent), scaledPixels(2, render_percent),
        scaledPixels(5, render_percent), scaledPixels(2, render_percent));
    page.layout->setSpacing(scaledPixels(1, render_percent));
    page.scroll_area->setWidget(page.content_widget);

    m_tabs->addTab(page.widget, page_name);
    iterator = m_pages.insert(page_name, page);
    return iterator.value();
}

SRibbonWidget::SRow& SRibbonWidget::ensureRow(SPage& page, int row_index)
{
    auto iterator = page.rows.find(row_index);
    if (iterator != page.rows.end())
    {
        return iterator.value();
    }

    SRow row;
    row.widget = new QWidget(page.content_widget);
    row.widget->setProperty("ribbonRow", row_index);
    row.layout = new QHBoxLayout(row.widget);
    row.layout->setContentsMargins(0, 0, 0, 0);
    row.layout->setSpacing(scaledPixels(2, interfaceScaleRenderPercent(m_interface_scale_percent)));
    row.layout->addStretch();

    auto insertion = page.rows.lowerBound(row_index);
    int layout_index = 0;
    for (auto existing = page.rows.cbegin(); existing != insertion; ++existing)
    {
        ++layout_index;
    }
    page.layout->insertWidget(layout_index, row.widget);
    iterator = page.rows.insert(row_index, row);
    return iterator.value();
}

QHBoxLayout* SRibbonWidget::ensureGroup(SRow& row, const QString& group_name, bool show_title)
{
    auto iterator = row.groups.find(group_name);
    if (iterator != row.groups.end())
    {
        return iterator.value();
    }

    auto* group = new QGroupBox(show_title ? group_name : QString(), row.widget);
    group->setProperty("ribbonGroupName", group_name);
    auto* group_layout = new QHBoxLayout(group);
    group_layout->setProperty("ribbonShowTitle", show_title);
    const int render_percent = interfaceScaleRenderPercent(m_interface_scale_percent);
    group_layout->setContentsMargins(
        scaledPixels(3, render_percent), scaledPixels(1, render_percent),
        scaledPixels(3, render_percent), scaledPixels(show_title ? 8 : 1, render_percent));
    group_layout->setSpacing(scaledPixels(1, render_percent));
    row.layout->insertWidget(row.layout->count() - 1, group);
    row.groups.insert(group_name, group_layout);
    return group_layout;
}

void SRibbonWidget::applyButtonMetrics(SButton& button)
{
    const int render_percent = interfaceScaleRenderPercent(m_interface_scale_percent);
    int icon_size = 28;
    int minimum_width = 62;
    int minimum_height = 68;
    if (button.size == SRibbonButtonSize::Compact)
    {
        icon_size = 16;
        minimum_width = 44;
        minimum_height = 38;
    }
    else if (button.size == SRibbonButtonSize::Standard)
    {
        icon_size = 22;
        minimum_width = 52;
        minimum_height = 54;
    }

    const int scaled_icon_size = scaledPixels(icon_size, render_percent);
    button.widget->setIconSize(QSize(scaled_icon_size, scaled_icon_size));
    button.widget->setMinimumSize(scaledPixels(minimum_width, render_percent),
                                  scaledPixels(minimum_height, render_percent));
}

void SRibbonWidget::updatePageMinimumWidth(SPage& page)
{
    int minimum_width = 0;
    for (SRow& row : page.rows)
    {
        row.widget->adjustSize();
        minimum_width = qMax(minimum_width, row.layout->minimumSize().width());
    }
    const QMargins margins = page.layout->contentsMargins();
    page.content_widget->setMinimumWidth(minimum_width + margins.left() + margins.right());
}

void SRibbonWidget::updateRibbonHeight()
{
    int row_count = 1;
    if (m_tabs && m_tabs->currentWidget())
    {
        const QString page_name = m_tabs->currentWidget()->property("ribbonPageName").toString();
        row_count = qMax(1, pageRowCount(page_name));
    }
    const int base_height = row_count > 1 ? 148 : 132;
    setFixedHeight(
        scaledPixels(base_height, interfaceScaleRenderPercent(m_interface_scale_percent)));
}
} // namespace smartGraphics3D
