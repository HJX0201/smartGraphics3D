#pragma once

#include <QColor>
#include <QDateTime>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QString>
#include <QUuid>
#include <QVector3D>

namespace smartGraphics3D
{
using SObjectId = QUuid;

enum class SObjectType
{
    Group,
    CadShape,
    Mesh,
    Measurement,
    CoordinateSystem
};

enum class SDataStage
{
    Original,
    Working,
    Published
};

enum class SSelectionMode
{
    Object,
    Solid,
    Face,
    Edge,
    Vertex
};

enum class SDisplayMode
{
    Shaded,
    Wireframe,
    ShadedWithEdges,
    HiddenLine,
    Transparent
};

enum class SCopyMode
{
    IndependentPresentation,
    SharedPresentation
};

struct SDisplayStyle
{
    QColor color = QColor(205, 228, 238);
    double transparency = 0.0;
    SDisplayMode mode = SDisplayMode::ShadedWithEdges;
};

struct SCoordinateSystem
{
    QUuid id = QUuid::createUuid();
    QString name = QStringLiteral("世界坐标系");
    QUuid parent_id;
    QMatrix4x4 transform_to_parent;
    QString source = QStringLiteral("内置");
    QDateTime calibrated_at;
    double error = 0.0;
    bool valid = true;
};

struct SSelection
{
    SObjectId object_id;
    SSelectionMode mode = SSelectionMode::Object;
    int sub_shape_index = -1;
};
} // namespace smartGraphics3D
