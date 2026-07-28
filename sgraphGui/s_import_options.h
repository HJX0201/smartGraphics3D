#pragma once

#include "s_unit_system.h"

#include <QString>

class QWidget;

namespace smartGraphics3D
{
struct SImportOptions
{
    bool accepted = true;
    double scale_to_millimeters = 1.0;
    QString source_unit;
    bool unit_was_embedded = false;
};

SImportOptions requestImportOptions(QWidget* parent, const QString& file_path,
                                    const SUnitSystem& project_units);
} // namespace smartGraphics3D
