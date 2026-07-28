#include "s_transform_utils.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace smartGraphics3D
{
namespace
{
constexpr float kTransformTolerance = 1.0e-4F;

bool nearlyEqual(float first, float second)
{
    return std::abs(first - second) <=
           kTransformTolerance * std::max({1.0F, std::abs(first), std::abs(second)});
}
} // namespace

bool isFiniteAffineTransform(const QMatrix4x4& transform)
{
    const float* values = transform.constData();
    for (int index = 0; index < 16; ++index)
    {
        if (!std::isfinite(values[index]))
        {
            return false;
        }
    }
    return nearlyEqual(transform(3, 0), 0.0F) && nearlyEqual(transform(3, 1), 0.0F) &&
           nearlyEqual(transform(3, 2), 0.0F) && nearlyEqual(transform(3, 3), 1.0F);
}

bool isSimilarityTransform(const QMatrix4x4& transform)
{
    if (!isFiniteAffineTransform(transform))
    {
        return false;
    }
    const QVector3D x(transform(0, 0), transform(1, 0), transform(2, 0));
    const QVector3D y(transform(0, 1), transform(1, 1), transform(2, 1));
    const QVector3D z(transform(0, 2), transform(1, 2), transform(2, 2));
    const float x_length = x.length();
    const float y_length = y.length();
    const float z_length = z.length();
    if (x_length <= kTransformTolerance || !nearlyEqual(x_length, y_length) ||
        !nearlyEqual(x_length, z_length))
    {
        return false;
    }
    const float squared_scale = x_length * x_length;
    return std::abs(QVector3D::dotProduct(x, y)) <= kTransformTolerance * squared_scale &&
           std::abs(QVector3D::dotProduct(x, z)) <= kTransformTolerance * squared_scale &&
           std::abs(QVector3D::dotProduct(y, z)) <= kTransformTolerance * squared_scale;
}

bool isRigidTransform(const QMatrix4x4& transform)
{
    if (!isSimilarityTransform(transform))
    {
        return false;
    }
    const QVector3D x(transform(0, 0), transform(1, 0), transform(2, 0));
    return nearlyEqual(x.length(), 1.0F);
}
} // namespace smartGraphics3D
