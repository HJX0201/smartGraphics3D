#pragma once

#include "s_result.h"

#include <QString>

namespace smartGraphics3D
{
enum class SLengthUnit
{
    Millimeter,
    Centimeter,
    Meter,
    Inch
};

enum class SAngleUnit
{
    Degree,
    Radian
};

class SUnitSystem
{
  public:
    SLengthUnit lengthUnit() const;
    void setLengthUnit(SLengthUnit unit);
    SAngleUnit angleUnit() const;
    void setAngleUnit(SAngleUnit unit);

    double toMillimeters(double value) const;
    double fromMillimeters(double value) const;
    double toDegrees(double value) const;
    double fromDegrees(double value) const;
    QString lengthSuffix() const;
    QString angleSuffix() const;

  private:
    SLengthUnit m_length_unit = SLengthUnit::Millimeter;
    SAngleUnit m_angle_unit = SAngleUnit::Degree;
};
} // namespace smartGraphics3D
