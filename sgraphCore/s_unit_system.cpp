#include "s_unit_system.h"

#include <QtMath>

namespace smartGraphics3D
{
namespace
{
double millimeterFactor(SLengthUnit unit)
{
    switch (unit)
    {
    case SLengthUnit::Millimeter:
        return 1.0;
    case SLengthUnit::Centimeter:
        return 10.0;
    case SLengthUnit::Meter:
        return 1000.0;
    case SLengthUnit::Inch:
        return 25.4;
    }
    return 1.0;
}
} // namespace

SLengthUnit SUnitSystem::lengthUnit() const
{
    return m_length_unit;
}

void SUnitSystem::setLengthUnit(SLengthUnit unit)
{
    m_length_unit = unit;
}

SAngleUnit SUnitSystem::angleUnit() const
{
    return m_angle_unit;
}

void SUnitSystem::setAngleUnit(SAngleUnit unit)
{
    m_angle_unit = unit;
}

double SUnitSystem::toMillimeters(double value) const
{
    return value * millimeterFactor(m_length_unit);
}

double SUnitSystem::fromMillimeters(double value) const
{
    return value / millimeterFactor(m_length_unit);
}

double SUnitSystem::toDegrees(double value) const
{
    return m_angle_unit == SAngleUnit::Degree ? value : qRadiansToDegrees(value);
}

double SUnitSystem::fromDegrees(double value) const
{
    return m_angle_unit == SAngleUnit::Degree ? value : qDegreesToRadians(value);
}

QString SUnitSystem::lengthSuffix() const
{
    switch (m_length_unit)
    {
    case SLengthUnit::Millimeter:
        return QStringLiteral("mm");
    case SLengthUnit::Centimeter:
        return QStringLiteral("cm");
    case SLengthUnit::Meter:
        return QStringLiteral("m");
    case SLengthUnit::Inch:
        return QStringLiteral("in");
    }
    return QStringLiteral("mm");
}

QString SUnitSystem::angleSuffix() const
{
    return m_angle_unit == SAngleUnit::Degree ? QStringLiteral("°") : QStringLiteral("rad");
}
} // namespace smartGraphics3D
