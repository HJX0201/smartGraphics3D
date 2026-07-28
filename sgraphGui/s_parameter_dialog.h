#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <functional>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;

namespace smartGraphics3D
{
struct SParameterField
{
    QString key;
    QString label;
    QString unit;
    QString description;
    double default_value = 0.0;
    double minimum = 0.0;
    double maximum = 1.0;
    int decimals = 3;
    bool advanced = false;
};

class SParameterDialog final : public QDialog
{
  public:
    using SPreviewCallback = std::function<void(const QMap<QString, double>& values)>;

    SParameterDialog(QString title, QList<SParameterField> fields, SPreviewCallback preview,
                     QWidget* parent = nullptr);

    QMap<QString, double> values() const;

  private:
    void updatePreview();
    void resetValues();

    QList<SParameterField> m_fields;
    QMap<QString, QDoubleSpinBox*> m_editors;
    SPreviewCallback m_preview;
    QCheckBox* m_preview_enabled = nullptr;
    QLabel* m_description = nullptr;
};
} // namespace smartGraphics3D
