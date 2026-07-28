#include "s_icon_factory.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <initializer_list>

namespace
{
void ensureIconResources()
{
    static const bool initialized = []()
    {
        Q_INIT_RESOURCE(s_icons);
        return true;
    }();
    Q_UNUSED(initialized)
}
} // namespace

namespace smartGraphics3D
{
namespace
{
QString iconName(SIconId icon_id)
{
    switch (icon_id)
    {
    case SIconId::FileNew:
        return QStringLiteral("file-plus");
    case SIconId::FileOpen:
        return QStringLiteral("folder-open");
    case SIconId::FileSave:
        return QStringLiteral("save");
    case SIconId::FileSaveAs:
        return QStringLiteral("save-all");
    case SIconId::FileImport:
        return QStringLiteral("file-input");
    case SIconId::FileExport:
        return QStringLiteral("file-output");
    case SIconId::ProjectArchive:
        return QStringLiteral("archive");
    case SIconId::PrimitiveBox:
        return QStringLiteral("box");
    case SIconId::PrimitiveCylinder:
        return QStringLiteral("cylinder");
    case SIconId::PrimitiveCone:
        return QStringLiteral("cone");
    case SIconId::PrimitiveSphere:
        return QStringLiteral("globe");
    case SIconId::PrimitiveTorus:
        return QStringLiteral("circle-dot-dashed");
    case SIconId::BooleanUnion:
        return QStringLiteral("combine");
    case SIconId::BooleanDifference:
        return QStringLiteral("circle-minus");
    case SIconId::BooleanIntersection:
        return QStringLiteral("blend");
    case SIconId::Fillet:
        return QStringLiteral("radius");
    case SIconId::Chamfer:
        return QStringLiteral("triangle-right");
    case SIconId::Hole:
        return QStringLiteral("drill");
    case SIconId::Transform:
        return QStringLiteral("move-3d");
    case SIconId::Mirror:
        return QStringLiteral("flip-horizontal-2");
    case SIconId::Copy:
        return QStringLiteral("copy");
    case SIconId::LinearArray:
        return QStringLiteral("rows-3");
    case SIconId::PolarArray:
        return QStringLiteral("rotate-ccw");
    case SIconId::Delete:
        return QStringLiteral("trash-2");
    case SIconId::Undo:
        return QStringLiteral("undo-2");
    case SIconId::Redo:
        return QStringLiteral("redo-2");
    case SIconId::MeasureStatistics:
        return QStringLiteral("chart-no-axes-combined");
    case SIconId::MeasureSubElement:
        return QStringLiteral("ruler");
    case SIconId::MeasureDistance:
        return QStringLiteral("move-diagonal-2");
    case SIconId::MeasureAngle:
        return QStringLiteral("drafting-compass");
    case SIconId::MeasureSection:
        return QStringLiteral("scan-line");
    case SIconId::MeasureExport:
        return QStringLiteral("file-down");
    case SIconId::ViewportScreenshot:
        return QStringLiteral("camera");
    case SIconId::SnapshotCreate:
        return QStringLiteral("bookmark-plus");
    case SIconId::SnapshotRestore:
        return QStringLiteral("history");
    case SIconId::SnapshotBranch:
        return QStringLiteral("git-branch-plus");
    case SIconId::FitAll:
        return QStringLiteral("maximize");
    case SIconId::FitSelection:
        return QStringLiteral("focus");
    case SIconId::ViewIsometric:
        return QStringLiteral("rotate-3d");
    case SIconId::ViewFront:
        return QStringLiteral("arrow-down-to-line");
    case SIconId::ViewBack:
        return QStringLiteral("arrow-up-from-line");
    case SIconId::ViewLeft:
        return QStringLiteral("arrow-left-to-line");
    case SIconId::ViewRight:
        return QStringLiteral("arrow-right-to-line");
    case SIconId::ViewTop:
        return QStringLiteral("arrow-up-to-line");
    case SIconId::ViewBottom:
        return QStringLiteral("arrow-down-from-line");
    case SIconId::DisplayWireframe:
        return QStringLiteral("cuboid");
    case SIconId::DisplayShaded:
        return QStringLiteral("box");
    case SIconId::DisplayShadedEdges:
        return QStringLiteral("scan-eye");
    case SIconId::DisplayHiddenLine:
        return QStringLiteral("eye-off");
    case SIconId::DisplayTransparent:
        return QStringLiteral("layers-2");
    case SIconId::Projection:
        return QStringLiteral("panel-top");
    case SIconId::ViewSave:
        return QStringLiteral("bookmark");
    case SIconId::ViewRestore:
        return QStringLiteral("bookmark-check");
    case SIconId::RotationCenter:
        return QStringLiteral("crosshair");
    case SIconId::FreeRotation:
        return QStringLiteral("refresh-cw");
    case SIconId::ClipPlanes:
        return QStringLiteral("slice");
    case SIconId::ViewportSingle:
        return QStringLiteral("square");
    case SIconId::ViewportHorizontal:
        return QStringLiteral("columns-2");
    case SIconId::ViewportVertical:
        return QStringLiteral("rows-2");
    case SIconId::ViewportFour:
        return QStringLiteral("grid-2x2");
    case SIconId::SyncCamera:
        return QStringLiteral("radio-tower");
    case SIconId::SyncSelection:
        return QStringLiteral("mouse-pointer-click");
    case SIconId::SelectObject:
        return QStringLiteral("mouse-pointer");
    case SIconId::SelectSolid:
        return QStringLiteral("box");
    case SIconId::SelectFace:
        return QStringLiteral("square-dashed");
    case SIconId::SelectEdge:
        return QStringLiteral("spline");
    case SIconId::SelectVertex:
        return QStringLiteral("circle-dot");
    case SIconId::SelectAll:
        return QStringLiteral("list-checks");
    case SIconId::SelectInvert:
        return QStringLiteral("list-x");
    case SIconId::ProjectUnit:
        return QStringLiteral("badge-cent");
    case SIconId::UserCoordinateSystem:
        return QStringLiteral("axis-3d");
    case SIconId::ObjectCoordinateSystem:
        return QStringLiteral("map-pinned");
    case SIconId::CoordinateSystemList:
        return QStringLiteral("table-2");
    case SIconId::Diagnostics:
        return QStringLiteral("activity");
    case SIconId::InterfaceScale:
        return QStringLiteral("maximize-2");
    case SIconId::About:
        return QStringLiteral("circle-question-mark");
    case SIconId::SceneGroup:
        return QStringLiteral("folder-tree");
    case SIconId::SceneCadShape:
        return QStringLiteral("cuboid");
    case SIconId::SceneMesh:
        return QStringLiteral("network");
    case SIconId::SceneMeasurement:
        return QStringLiteral("ruler");
    case SIconId::SceneCoordinateSystem:
        return QStringLiteral("map-pinned");
    }
    return QStringLiteral("circle-question-mark");
}

QPixmap renderSvg(SIconId icon_id, int size, const QColor& color)
{
    ensureIconResources();
    QFile file(QStringLiteral(":/sgraph/icons/%1.svg").arg(iconName(icon_id)));
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
    {
        return {};
    }

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(1.0, 1.0, size - 2.0, size - 2.0));
    return pixmap;
}

QPolygonF makePolygon(std::initializer_list<QPointF> points)
{
    QPolygonF polygon;
    for (const QPointF& point : points)
    {
        polygon.push_back(point);
    }
    return polygon;
}

QPixmap renderApplicationPixmap(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(static_cast<double>(size) / 64.0, static_cast<double>(size) / 64.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#101d27")));
    painter.drawRoundedRect(QRectF(2.0, 2.0, 60.0, 60.0), 13.0, 13.0);

    painter.setBrush(QColor(20, 151, 207, 45));
    painter.setPen(
        QPen(QColor(QStringLiteral("#20a8df")), 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPolygonF top = makePolygon(
        {QPointF(12.0, 25.0), QPointF(31.0, 14.0), QPointF(51.0, 25.0), QPointF(31.0, 36.0)});
    painter.drawPolygon(top);
    painter.drawPolyline(makePolygon({QPointF(12.0, 25.0), QPointF(12.0, 43.0), QPointF(31.0, 53.0),
                                      QPointF(51.0, 43.0), QPointF(51.0, 25.0)}));
    painter.drawLine(QPointF(31.0, 36.0), QPointF(31.0, 53.0));

    painter.setBrush(QColor(QStringLiteral("#101d27")));
    painter.setPen(QPen(QColor(QStringLiteral("#d9f3ff")), 2.4));
    painter.drawEllipse(QRectF(23.0, 26.0, 16.0, 16.0));
    painter.setBrush(QColor(QStringLiteral("#20a8df")));
    painter.drawEllipse(QRectF(28.0, 31.0, 6.0, 6.0));
    return pixmap;
}
} // namespace

QIcon applicationIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
    {
        icon.addPixmap(renderApplicationPixmap(size));
    }
    return icon;
}

QIcon iconForAction(SIconId icon_id)
{
    QIcon icon;
    for (int size : {16, 20, 24, 32, 48, 64})
    {
        icon.addPixmap(renderSvg(icon_id, size, QColor(QStringLiteral("#d7e7f0"))), QIcon::Normal);
        icon.addPixmap(renderSvg(icon_id, size, QColor(QStringLiteral("#718694"))),
                       QIcon::Disabled);
    }
    return icon;
}
} // namespace smartGraphics3D
