#pragma once

class QDialog;

namespace smartGraphics3D
{
void installDialogSizePolicy();
void setDialogSizePolicyPercent(int percent);
void applyDialogMinimumSize(QDialog& dialog, int percent);
} // namespace smartGraphics3D
