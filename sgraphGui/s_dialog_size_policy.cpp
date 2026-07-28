#include "s_dialog_size_policy.h"

#include "s_interface_scale.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QtMath>

namespace smartGraphics3D
{
namespace
{
constexpr auto kPolicyObjectName = "sgraphDialogSizePolicy";
constexpr auto kBaseWidthProperty = "dialogPolicyBaseWidth";
constexpr auto kBaseHeightProperty = "dialogPolicyBaseHeight";

struct SDialogMinimum
{
    int width = 480;
    int height = 280;
};

SDialogMinimum minimumForDialog(const QDialog& dialog)
{
    if (dialog.objectName() == QStringLiteral("interfaceScaleDialog"))
    {
        return {430, 220};
    }
    if (qobject_cast<const QFileDialog*>(&dialog))
    {
        return {760, 500};
    }
    if (qobject_cast<const QInputDialog*>(&dialog))
    {
        return {440, 220};
    }
    if (qobject_cast<const QMessageBox*>(&dialog))
    {
        return {460, 200};
    }
    return {};
}

int scaledMinimum(int pixels, int percent)
{
    const int layout_percent = qMax(kDefaultInterfaceScalePercent, percent);
    return qRound(static_cast<double>(pixels) * layout_percent / 100.0);
}

class SDialogSizePolicy final : public QObject
{
  public:
    explicit SDialogSizePolicy(QObject* parent) : QObject(parent)
    {
        setObjectName(QString::fromLatin1(kPolicyObjectName));
    }

    void setPercent(int percent)
    {
        m_percent =
            isSupportedInterfaceScalePercent(percent) ? percent : kDefaultInterfaceScalePercent;
        for (QWidget* widget : QApplication::topLevelWidgets())
        {
            if (auto* dialog = qobject_cast<QDialog*>(widget))
            {
                applyDialogMinimumSize(*dialog, m_percent);
            }
        }
    }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show)
        {
            if (auto* dialog = qobject_cast<QDialog*>(watched))
            {
                applyDialogMinimumSize(*dialog, m_percent);
                QTimer::singleShot(0, dialog,
                                   [dialog, this]()
                                   {
                                       applyDialogMinimumSize(*dialog, m_percent);
                                   });
            }
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    int m_percent = kDefaultInterfaceScalePercent;
};

SDialogSizePolicy* sizePolicy()
{
    if (!qApp)
    {
        return nullptr;
    }
    if (auto* existing = qApp->findChild<SDialogSizePolicy*>(QString::fromLatin1(kPolicyObjectName),
                                                             Qt::FindDirectChildrenOnly))
    {
        return existing;
    }

    auto* policy = new SDialogSizePolicy(qApp);
    qApp->installEventFilter(policy);
    return policy;
}
} // namespace

void installDialogSizePolicy()
{
    (void)sizePolicy();
}

void setDialogSizePolicyPercent(int percent)
{
    if (SDialogSizePolicy* policy = sizePolicy())
    {
        policy->setPercent(percent);
    }
}

void applyDialogMinimumSize(QDialog& dialog, int percent)
{
    const SDialogMinimum policy_minimum = minimumForDialog(dialog);
    if (!dialog.property(kBaseWidthProperty).isValid())
    {
        dialog.setProperty(kBaseWidthProperty, dialog.minimumWidth());
        dialog.setProperty(kBaseHeightProperty, dialog.minimumHeight());
    }

    int minimum_width = scaledMinimum(
        qMax(policy_minimum.width, dialog.property(kBaseWidthProperty).toInt()), percent);
    int minimum_height = scaledMinimum(
        qMax(policy_minimum.height, dialog.property(kBaseHeightProperty).toInt()), percent);
    QScreen* screen =
        dialog.windowHandle() ? dialog.windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (screen)
    {
        const QSize available = screen->availableGeometry().size();
        minimum_width = qMin(minimum_width, available.width() * 9 / 10);
        minimum_height = qMin(minimum_height, available.height() * 9 / 10);
    }
    dialog.setMinimumSize(minimum_width, minimum_height);
}
} // namespace smartGraphics3D
