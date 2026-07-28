#pragma once

#include <QMatrix4x4>

namespace smartGraphics3D
{
bool isFiniteAffineTransform(const QMatrix4x4& transform);
bool isSimilarityTransform(const QMatrix4x4& transform);
bool isRigidTransform(const QMatrix4x4& transform);
} // namespace smartGraphics3D
