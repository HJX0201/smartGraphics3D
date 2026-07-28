#include "s_parameter_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace smartGraphics3D
{
SParameterDialog::SParameterDialog(QString title, QList<SParameterField> fields,
                                   SPreviewCallback preview, QWidget* parent)
    : QDialog(parent), m_fields(std::move(fields)), m_preview(std::move(preview))
{
    setWindowTitle(std::move(title));
    setModal(true);
    setMinimumWidth(460);
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    auto* basic_page = new QWidget(tabs);
    auto* advanced_page = new QWidget(tabs);
    auto* basic_form = new QFormLayout(basic_page);
    auto* advanced_form = new QFormLayout(advanced_page);

    for (const SParameterField& field : m_fields)
    {
        auto* editor = new QDoubleSpinBox(this);
        editor->setObjectName(QStringLiteral("parameter_%1").arg(field.key));
        editor->setRange(field.minimum, field.maximum);
        editor->setDecimals(field.decimals);
        editor->setValue(field.default_value);
        editor->setSuffix(field.unit.isEmpty() ? QString() : QStringLiteral(" %1").arg(field.unit));
        editor->setToolTip(tr("%1\n合法范围：%2 – %3 %4\n默认值：%5 %4")
                               .arg(field.description)
                               .arg(field.minimum)
                               .arg(field.maximum)
                               .arg(field.unit)
                               .arg(field.default_value));
        m_editors.insert(field.key, editor);
        (field.advanced ? advanced_form : basic_form)->addRow(field.label, editor);
        connect(editor, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this, field](double)
                {
                    m_description->setText(field.description);
                    updatePreview();
                });
    }

    tabs->addTab(basic_page, tr("基本参数"));
    tabs->addTab(advanced_page, tr("高级参数"));
    if (advanced_form->rowCount() == 0)
    {
        tabs->removeTab(1);
        advanced_page->deleteLater();
    }
    layout->addWidget(tabs);

    m_description = new QLabel(tr("修改参数可查看临时预览；应用前不会改变项目。"), this);
    m_description->setWordWrap(true);
    m_description->setMinimumHeight(42);
    layout->addWidget(m_description);

    m_preview_enabled = new QCheckBox(tr("实时预览"), this);
    m_preview_enabled->setChecked(true);
    connect(m_preview_enabled, &QCheckBox::toggled, this,
            [this]()
            {
                updatePreview();
            });
    layout->addWidget(m_preview_enabled);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Reset | QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("应用"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    buttons->button(QDialogButtonBox::Reset)->setText(tr("恢复默认值"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
            &SParameterDialog::resetValues);
    layout->addWidget(buttons);

    updatePreview();
}

QMap<QString, double> SParameterDialog::values() const
{
    QMap<QString, double> result;
    for (auto iterator = m_editors.constBegin(); iterator != m_editors.constEnd(); ++iterator)
    {
        result.insert(iterator.key(), iterator.value()->value());
    }
    return result;
}

void SParameterDialog::updatePreview()
{
    if (m_preview && m_preview_enabled && m_preview_enabled->isChecked())
    {
        m_preview(values());
    }
}

void SParameterDialog::resetValues()
{
    for (const SParameterField& field : m_fields)
    {
        if (QDoubleSpinBox* editor = m_editors.value(field.key, nullptr))
        {
            editor->setValue(field.default_value);
        }
    }
    updatePreview();
}
} // namespace smartGraphics3D
