#include "s_clip_plane_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace smartGraphics3D
{
namespace
{
constexpr int kMaximumClipPlanes = 3;
constexpr int kSliderScale = 10;

QVector3D normalForType(int type_index)
{
    switch (type_index)
    {
    case 0:
        return QVector3D(1.0F, 0.0F, 0.0F);
    case 1:
        return QVector3D(0.0F, 1.0F, 0.0F);
    case 2:
        return QVector3D(0.0F, 0.0F, 1.0F);
    default:
        return QVector3D();
    }
}

int typeForNormal(const QVector3D& normal)
{
    const QVector3D normalized = normal.normalized();
    if ((normalized - QVector3D(1.0F, 0.0F, 0.0F)).lengthSquared() < 1.0e-6F)
    {
        return 0;
    }
    if ((normalized - QVector3D(0.0F, 1.0F, 0.0F)).lengthSquared() < 1.0e-6F)
    {
        return 1;
    }
    if ((normalized - QVector3D(0.0F, 0.0F, 1.0F)).lengthSquared() < 1.0e-6F)
    {
        return 2;
    }
    return 3;
}
} // namespace

SClipPlaneDialog::SClipPlaneDialog(const SUnitSystem& units, const QList<SClipPlane>& initial,
                                   SPreviewCallback preview, QWidget* parent)
    : QDialog(parent), m_units(units), m_preview(std::move(preview))
{
    setWindowTitle(tr("动态剖切设置"));
    setModal(true);
    setMinimumWidth(480);
    auto* layout = new QVBoxLayout(this);
    auto* description =
        new QLabel(tr("拖动偏移滑块可实时检查剖切结果；“应用”前不会写入项目。"), this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* count_layout = new QFormLayout();
    m_count = new QSpinBox(this);
    m_count->setObjectName(QStringLiteral("clipPlaneCount"));
    m_count->setRange(0, kMaximumClipPlanes);
    m_count->setValue(qMin(initial.size(), kMaximumClipPlanes));
    count_layout->addRow(tr("剖切面数量"), m_count);
    layout->addLayout(count_layout);

    m_tabs = new QTabWidget(this);
    for (int index = 0; index < kMaximumClipPlanes; ++index)
    {
        auto* page = new QWidget(m_tabs);
        auto* form = new QFormLayout(page);
        SPlaneEditors editors;
        editors.type = new QComboBox(page);
        editors.type->addItems({tr("X 平面"), tr("Y 平面"), tr("Z 平面"), tr("任意平面")});
        form->addRow(tr("平面类型"), editors.type);

        editors.normal_x = new QDoubleSpinBox(page);
        editors.normal_y = new QDoubleSpinBox(page);
        editors.normal_z = new QDoubleSpinBox(page);
        for (QDoubleSpinBox* editor : {editors.normal_x, editors.normal_y, editors.normal_z})
        {
            editor->setRange(-1.0e6, 1.0e6);
            editor->setDecimals(6);
        }
        form->addRow(tr("法向 X"), editors.normal_x);
        form->addRow(tr("法向 Y"), editors.normal_y);
        form->addRow(tr("法向 Z"), editors.normal_z);

        editors.offset = new QDoubleSpinBox(page);
        editors.offset->setRange(-100000.0, 100000.0);
        editors.offset->setDecimals(3);
        editors.offset->setSuffix(QStringLiteral(" %1").arg(units.lengthSuffix()));
        form->addRow(tr("精确偏移"), editors.offset);
        editors.offset_slider = new QSlider(Qt::Horizontal, page);
        editors.offset_slider->setObjectName(QStringLiteral("clipOffsetSlider%1").arg(index + 1));
        editors.offset_slider->setRange(-1000 * kSliderScale, 1000 * kSliderScale);
        form->addRow(tr("拖动偏移"), editors.offset_slider);
        editors.flipped = new QCheckBox(tr("翻转剖切方向"), page);
        form->addRow({}, editors.flipped);

        const SClipPlane definition = index < initial.size() ? initial.at(index) : SClipPlane();
        const int type_index = typeForNormal(definition.normal);
        editors.type->setCurrentIndex(type_index);
        editors.normal_x->setValue(definition.normal.x());
        editors.normal_y->setValue(definition.normal.y());
        editors.normal_z->setValue(definition.normal.z());
        const double display_offset = units.fromMillimeters(definition.offset);
        editors.offset->setValue(display_offset);
        editors.offset_slider->setValue(
            qRound(qBound(-1000.0, display_offset, 1000.0) * kSliderScale));
        editors.flipped->setChecked(definition.flipped);

        m_editors.push_back(editors);
        m_tabs->addTab(page, tr("剖切面 %1").arg(index + 1));
        connect(editors.type, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this, index]()
                {
                    updateNormalEditors(index);
                    updatePreview();
                });
        for (QDoubleSpinBox* editor :
             {editors.normal_x, editors.normal_y, editors.normal_z, editors.offset})
        {
            connect(editor, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                    [this]()
                    {
                        updatePreview();
                    });
        }
        connect(editors.offset_slider, &QSlider::valueChanged, this,
                [this, index](int value)
                {
                    m_editors.at(index).offset->setValue(static_cast<double>(value) / kSliderScale);
                });
        connect(editors.flipped, &QCheckBox::toggled, this,
                [this]()
                {
                    updatePreview();
                });
        updateNormalEditors(index);
    }
    layout->addWidget(m_tabs);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Reset | QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("应用"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    buttons->button(QDialogButtonBox::Reset)->setText(tr("关闭全部"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SClipPlaneDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this,
            [this]()
            {
                m_count->setValue(0);
            });
    layout->addWidget(buttons);

    connect(m_count, qOverload<int>(&QSpinBox::valueChanged), this,
            [this]()
            {
                updatePageVisibility();
                updatePreview();
            });
    updatePageVisibility();
    updatePreview();
}

QList<SClipPlane> SClipPlaneDialog::planes() const
{
    QList<SClipPlane> result;
    for (int index = 0; index < m_count->value(); ++index)
    {
        const SPlaneEditors& editors = m_editors.at(index);
        SClipPlane plane;
        plane.normal = normalForType(editors.type->currentIndex());
        if (editors.type->currentIndex() == 3)
        {
            plane.normal = QVector3D(static_cast<float>(editors.normal_x->value()),
                                     static_cast<float>(editors.normal_y->value()),
                                     static_cast<float>(editors.normal_z->value()));
        }
        if (plane.normal.lengthSquared() < 1.0e-12F)
        {
            plane.normal = QVector3D(0.0F, 0.0F, 1.0F);
        }
        plane.offset = m_units.toMillimeters(editors.offset->value());
        plane.flipped = editors.flipped->isChecked();
        result.push_back(plane);
    }
    return result;
}

void SClipPlaneDialog::accept()
{
    for (int index = 0; index < m_count->value(); ++index)
    {
        const SPlaneEditors& editors = m_editors.at(index);
        if (editors.type->currentIndex() != 3)
        {
            continue;
        }
        const QVector3D normal(static_cast<float>(editors.normal_x->value()),
                               static_cast<float>(editors.normal_y->value()),
                               static_cast<float>(editors.normal_z->value()));
        if (normal.lengthSquared() < 1.0e-12F)
        {
            QMessageBox::warning(this, tr("剖切参数无效"),
                                 tr("剖切面 %1 的法向量不能为零，请输入有效方向。").arg(index + 1));
            m_tabs->setCurrentIndex(index);
            return;
        }
    }
    QDialog::accept();
}

void SClipPlaneDialog::updatePageVisibility()
{
    for (int index = 0; index < m_tabs->count(); ++index)
    {
        m_tabs->setTabEnabled(index, index < m_count->value());
    }
}

void SClipPlaneDialog::updateNormalEditors(int page_index)
{
    const SPlaneEditors& editors = m_editors.at(page_index);
    const bool arbitrary = editors.type->currentIndex() == 3;
    editors.normal_x->setEnabled(arbitrary);
    editors.normal_y->setEnabled(arbitrary);
    editors.normal_z->setEnabled(arbitrary);
    if (!arbitrary)
    {
        const QVector3D normal = normalForType(editors.type->currentIndex());
        editors.normal_x->setValue(normal.x());
        editors.normal_y->setValue(normal.y());
        editors.normal_z->setValue(normal.z());
    }
}

void SClipPlaneDialog::updatePreview()
{
    if (m_preview)
    {
        m_preview(planes());
    }
}
} // namespace smartGraphics3D
