#pragma once

#include "s_result.h"

#include <QDialog>
#include <QList>

class QButtonGroup;
class QHBoxLayout;
class QSettings;
class QVBoxLayout;

namespace smartGraphics3D
{
constexpr int kDefaultInterfaceScalePercent = 100;
constexpr int kInterfaceScaleSettingsVersion = 2;

[[nodiscard]] QList<int> supportedInterfaceScalePercents();
[[nodiscard]] bool isSupportedInterfaceScalePercent(int percent);
[[nodiscard]] int interfaceScaleRenderPercent(int percent);
[[nodiscard]] SResult<int> readInterfaceScalePercent(QSettings& settings);
[[nodiscard]] SResult<void> writeInterfaceScalePercent(QSettings& settings, int percent);

class SInterfaceScaleDialog final : public QDialog
{
    Q_OBJECT

  public:
    explicit SInterfaceScaleDialog(int current_percent, QWidget* parent = nullptr);

    [[nodiscard]] int selectedPercent() const;

  signals:
    void scalePreviewed(int percent);

  private:
    void updateLayoutForPercent(int percent);

    QButtonGroup* m_scale_group = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QHBoxLayout* m_options_layout = nullptr;
};
} // namespace smartGraphics3D
