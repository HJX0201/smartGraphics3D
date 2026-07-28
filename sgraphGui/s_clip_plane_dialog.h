#pragma once

#include "s_occ_viewport.h"
#include "s_unit_system.h"

#include <QDialog>
#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QSpinBox;
class QTabWidget;

namespace smartGraphics3D
{
class SClipPlaneDialog final : public QDialog
{
  public:
    using SPreviewCallback = std::function<void(const QList<SClipPlane>&)>;

    SClipPlaneDialog(const SUnitSystem& units, const QList<SClipPlane>& initial,
                     SPreviewCallback preview, QWidget* parent = nullptr);

    QList<SClipPlane> planes() const;

  protected:
    void accept() override;

  private:
    struct SPlaneEditors
    {
        QComboBox* type = nullptr;
        QDoubleSpinBox* normal_x = nullptr;
        QDoubleSpinBox* normal_y = nullptr;
        QDoubleSpinBox* normal_z = nullptr;
        QDoubleSpinBox* offset = nullptr;
        QSlider* offset_slider = nullptr;
        QCheckBox* flipped = nullptr;
    };

    void updatePageVisibility();
    void updateNormalEditors(int page_index);
    void updatePreview();

    SUnitSystem m_units;
    SPreviewCallback m_preview;
    QSpinBox* m_count = nullptr;
    QTabWidget* m_tabs = nullptr;
    QList<SPlaneEditors> m_editors;
};
} // namespace smartGraphics3D
