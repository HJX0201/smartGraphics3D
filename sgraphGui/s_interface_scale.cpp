#include "s_interface_scale.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QtMath>

namespace smartGraphics3D
{
namespace
{
constexpr auto kInterfaceScaleKey = "appearance/interfaceScalePercent";
constexpr auto kInterfaceScaleVersionKey = "appearance/interfaceScaleVersion";
constexpr int kDialogBaseWidth = 430;
constexpr int kDialogBaseButtonHeight = 48;

QList<int> legacyInterfaceScalePercents()
{
    return {75, 80, 90, 100, 110, 125, 150, 175, 200};
}

int nearestSupportedPercent(int percent)
{
    int nearest = supportedInterfaceScalePercents().front();
    int nearest_distance = qAbs(nearest - percent);
    for (int candidate : supportedInterfaceScalePercents())
    {
        const int distance = qAbs(candidate - percent);
        if (distance < nearest_distance)
        {
            nearest = candidate;
            nearest_distance = distance;
        }
    }
    return nearest;
}
} // namespace

QList<int> supportedInterfaceScalePercents()
{
    return {75, 90, 100, 125, 150};
}

bool isSupportedInterfaceScalePercent(int percent)
{
    return supportedInterfaceScalePercents().contains(percent);
}

int interfaceScaleRenderPercent(int percent)
{
    return qRound(static_cast<double>(percent) * 1.25);
}

SResult<int> readInterfaceScalePercent(QSettings& settings)
{
    if (!settings.contains(QString::fromLatin1(kInterfaceScaleKey)))
    {
        return SResult<int>::success(kDefaultInterfaceScalePercent);
    }

    bool converted = false;
    const int percent = settings.value(QString::fromLatin1(kInterfaceScaleKey)).toInt(&converted);
    if (!converted)
    {
        return SResult<int>::failure(SErrorCode::CorruptData, QObject::tr("保存的界面比例无效"),
                                     QObject::tr("界面比例设置不是有效数字。"));
    }

    const int version =
        settings.value(QString::fromLatin1(kInterfaceScaleVersionKey), 1).toInt(&converted);
    if (!converted || version < 1 || version > kInterfaceScaleSettingsVersion)
    {
        return SResult<int>::failure(SErrorCode::CorruptData, QObject::tr("保存的界面比例无效"),
                                     QObject::tr("界面比例设置版本不受支持。"));
    }

    if (version == kInterfaceScaleSettingsVersion)
    {
        if (!isSupportedInterfaceScalePercent(percent))
        {
            return SResult<int>::failure(
                SErrorCode::CorruptData, QObject::tr("保存的界面比例无效"),
                QObject::tr("界面比例必须是 75%、90%、100%、125% 或 150%。"));
        }
        return SResult<int>::success(percent);
    }

    if (!legacyInterfaceScalePercents().contains(percent))
    {
        return SResult<int>::failure(SErrorCode::CorruptData, QObject::tr("保存的界面比例无效"),
                                     QObject::tr("旧版界面比例不是受支持的预设档位。"));
    }

    const int migrated_percent =
        nearestSupportedPercent(qRound(static_cast<double>(percent) * 0.8));
    settings.setValue(QString::fromLatin1(kInterfaceScaleKey), migrated_percent);
    settings.setValue(QString::fromLatin1(kInterfaceScaleVersionKey),
                      kInterfaceScaleSettingsVersion);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        return SResult<int>::failure(SErrorCode::FileFailure, QObject::tr("无法迁移界面比例设置"),
                                     QObject::tr("系统设置存储不可写。"));
    }
    return SResult<int>::success(migrated_percent);
}

SResult<void> writeInterfaceScalePercent(QSettings& settings, int percent)
{
    if (!isSupportedInterfaceScalePercent(percent))
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, QObject::tr("无法保存界面比例"),
                                      QObject::tr("%1% 不是受支持的界面比例。").arg(percent));
    }

    settings.setValue(QString::fromLatin1(kInterfaceScaleKey), percent);
    settings.setValue(QString::fromLatin1(kInterfaceScaleVersionKey),
                      kInterfaceScaleSettingsVersion);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        return SResult<void>::failure(SErrorCode::FileFailure, QObject::tr("无法保存界面比例设置"),
                                      QObject::tr("系统设置存储不可写。"));
    }
    return SResult<void>::success();
}

SInterfaceScaleDialog::SInterfaceScaleDialog(int current_percent, QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("interfaceScaleDialog"));
    setWindowTitle(tr("界面比例"));
    setModal(true);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(16, 14, 16, 12);
    m_layout->setSpacing(10);

    auto* description = new QLabel(tr("按比例同步调整界面字体、Ribbon、工具按钮、状态栏和面板尺寸。"
                                      "选择后立即预览，取消可恢复原比例，保存后下次启动继续使用。"),
                                   this);
    description->setWordWrap(true);
    m_layout->addWidget(description);

    auto* scale_label = new QLabel(tr("界面比例："), this);
    m_layout->addWidget(scale_label);

    auto* scale_options = new QWidget(this);
    scale_options->setObjectName(QStringLiteral("interfaceScaleOptions"));
    m_options_layout = new QHBoxLayout(scale_options);
    m_options_layout->setContentsMargins(0, 0, 0, 0);
    m_options_layout->setSpacing(6);

    m_scale_group = new QButtonGroup(this);
    m_scale_group->setObjectName(QStringLiteral("interfaceScaleButtonGroup"));
    m_scale_group->setExclusive(true);
    const QStringList option_names = {tr("小"), tr("紧凑"), tr("标准"), tr("大"), tr("特大")};
    int option_index = 0;
    for (int percent : supportedInterfaceScalePercents())
    {
        auto* option = new QPushButton(
            tr("%1%\n%2").arg(percent).arg(option_names.at(option_index)), scale_options);
        option->setObjectName(QStringLiteral("interfaceScaleOption%1").arg(percent));
        option->setProperty("scaleOption", true);
        option->setCheckable(true);
        option->setMinimumHeight(kDialogBaseButtonHeight);
        option->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        option->setToolTip(tr("预览 %1% 界面比例").arg(percent));
        m_scale_group->addButton(option, percent);
        m_options_layout->addWidget(option);
        ++option_index;
    }
    const int normalized_percent = isSupportedInterfaceScalePercent(current_percent)
                                       ? current_percent
                                       : kDefaultInterfaceScalePercent;
    m_scale_group->button(normalized_percent)->setChecked(true);
    m_layout->addWidget(scale_options);
    m_layout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    auto* reset_button = buttons->addButton(tr("恢复默认"), QDialogButtonBox::ResetRole);
    connect(reset_button, &QPushButton::clicked, this,
            [this]()
            {
                if (QAbstractButton* default_option =
                        m_scale_group->button(kDefaultInterfaceScalePercent))
                {
                    default_option->click();
                }
            });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_layout->addWidget(buttons);

    connect(m_scale_group, qOverload<int>(&QButtonGroup::buttonClicked), this,
            [this](int percent)
            {
                emit scalePreviewed(percent);
                updateLayoutForPercent(percent);
                QTimer::singleShot(0, this,
                                   [this, percent]()
                                   {
                                       updateLayoutForPercent(percent);
                                   });
            });
    updateLayoutForPercent(normalized_percent);
}

int SInterfaceScaleDialog::selectedPercent() const
{
    const int checked_percent = m_scale_group->checkedId();
    return isSupportedInterfaceScalePercent(checked_percent) ? checked_percent
                                                             : kDefaultInterfaceScalePercent;
}

void SInterfaceScaleDialog::updateLayoutForPercent(int percent)
{
    const int normalized_percent =
        isSupportedInterfaceScalePercent(percent) ? percent : kDefaultInterfaceScalePercent;
    const int layout_percent = qMax(kDefaultInterfaceScalePercent, normalized_percent);
    const auto scaled = [layout_percent](int pixels)
    {
        return qRound(static_cast<double>(pixels) * layout_percent / 100.0);
    };

    setMinimumWidth(scaled(kDialogBaseWidth));
    m_layout->setContentsMargins(scaled(16), scaled(14), scaled(16), scaled(12));
    m_layout->setSpacing(scaled(10));
    m_options_layout->setSpacing(scaled(6));
    for (QAbstractButton* option : m_scale_group->buttons())
    {
        option->setMinimumHeight(scaled(kDialogBaseButtonHeight));
    }

    m_options_layout->invalidate();
    m_layout->invalidate();
    adjustSize();
    resize(sizeHint());
    if (QWidget* owner = parentWidget())
    {
        const QPoint owner_center = owner->mapToGlobal(owner->rect().center());
        move(owner_center.x() - width() / 2, owner_center.y() - height() / 2);
    }
}
} // namespace smartGraphics3D
