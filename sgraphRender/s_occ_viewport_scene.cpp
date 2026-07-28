#include "s_3d_document.h"
#include "s_kernel_shape_access.h"
#include "s_occ_viewport.h"
#include "s_occ_viewport_p.h"
#include "s_render_quality.h"

#include <AIS_ColoredShape.hxx>
#include <AIS_ConnectedInteractive.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <Prs3d_Drawer.hxx>
#include <QMap>
#include <Quantity_Color.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace smartGraphics3D
{
namespace
{
constexpr qint64 kEstimatedGpuBytesPerTriangle = 84;

Quantity_Color toOccColor(const QColor& color)
{
    return Quantity_Color(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_RGB);
}

int triangleCount(const TopoDS_Shape& shape)
{
    int triangle_count = 0;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next())
    {
        TopLoc_Location location;
        const Handle(Poly_Triangulation)& triangulation =
            BRep_Tool::Triangulation(TopoDS::Face(explorer.Current()), location);
        if (!triangulation.IsNull())
        {
            triangle_count += triangulation->NbTriangles();
        }
    }
    return triangle_count;
}

double measurementCoordinate(const QJsonObject& properties, const QString& annotation_key,
                             const QString& fallback_key)
{
    return properties.contains(annotation_key) ? properties.value(annotation_key).toDouble()
                                               : properties.value(fallback_key).toDouble();
}

gp_Trsf toOccTransform(const QMatrix4x4& matrix)
{
    gp_Trsf transform;
    transform.SetValues(matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(0, 3), matrix(1, 0),
                        matrix(1, 1), matrix(1, 2), matrix(1, 3), matrix(2, 0), matrix(2, 1),
                        matrix(2, 2), matrix(2, 3));
    return transform;
}

double effectiveTransparency(const SSceneObject& object, SDisplayMode mode)
{
    return mode == SDisplayMode::Transparent ? 0.65 : object.display.transparency;
}

double effectiveTransparency(double transparency, SDisplayMode mode)
{
    return mode == SDisplayMode::Transparent ? 0.65 : transparency;
}

quint64 mixAppearanceHash(quint64 hash, quint64 value)
{
    return hash ^ (value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U));
}

quint64 styleHash(const SAppearanceStyle& style)
{
    const quint64 hash = static_cast<quint64>(style.color.rgb());
    return mixAppearanceHash(hash,
                             static_cast<quint64>(qRound64(style.transparency * 1000000000.0)));
}

quint64 importedAppearanceHash(const SSceneObject& object)
{
    if (!object.use_imported_appearance || !object.imported_appearance.valid)
    {
        return 0;
    }
    quint64 hash = styleHash(object.imported_appearance.base_style);
    for (const SFaceAppearance& face : object.imported_appearance.face_overrides)
    {
        hash = mixAppearanceHash(hash, static_cast<quint64>(face.face_index));
        hash = mixAppearanceHash(hash, styleHash(face.style));
    }
    return hash;
}

QString presentationKey(const SSceneObject& object, SDisplayMode mode)
{
    const TopoDS_Shape& shape = SKernelShapeAccess::native(object.shape);
    const quintptr geometry_identity = reinterpret_cast<quintptr>(shape.TShape().get());
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(object.presentation_group_id.toString(QUuid::WithoutBraces))
        .arg(object.display.color.rgba())
        .arg(effectiveTransparency(object, mode), 0, 'g', 17)
        .arg(static_cast<int>(mode))
        .arg(static_cast<qulonglong>(geometry_identity), 0, 16)
        .arg(object.use_imported_appearance ? 1 : 0)
        .arg(static_cast<qulonglong>(importedAppearanceHash(object)), 0, 16);
}

void configurePresentation(const Handle(AIS_Shape) & presentation, const SSceneObject& object,
                           SDisplayMode mode)
{
    const bool use_imported = object.use_imported_appearance && object.imported_appearance.valid;
    const SAppearanceStyle base_style =
        use_imported ? object.imported_appearance.base_style
                     : SAppearanceStyle{object.display.color, object.display.transparency};
    presentation->SetColor(toOccColor(base_style.color));
    presentation->SetTransparency(effectiveTransparency(base_style.transparency, mode));
    if (use_imported)
    {
        const Handle(AIS_ColoredShape) colored = Handle(AIS_ColoredShape)::DownCast(presentation);
        TopTools_IndexedMapOfShape faces;
        TopExp::MapShapes(presentation->Shape(), TopAbs_FACE, faces);
        if (!colored.IsNull())
        {
            for (const SFaceAppearance& face : object.imported_appearance.face_overrides)
            {
                if (face.face_index <= 0 || face.face_index > faces.Extent())
                {
                    continue;
                }
                const TopoDS_Shape& sub_shape = faces.FindKey(face.face_index);
                colored->SetCustomColor(sub_shape, toOccColor(face.style.color));
                colored->SetCustomTransparency(
                    sub_shape, effectiveTransparency(face.style.transparency, mode));
            }
        }
    }
    const bool wireframe = mode == SDisplayMode::Wireframe || mode == SDisplayMode::HiddenLine;
    presentation->SetDisplayMode(wireframe ? AIS_WireFrame : AIS_Shaded);
    presentation->Attributes()->SetFaceBoundaryDraw(mode == SDisplayMode::ShadedWithEdges);
}
} // namespace

void SOccViewport::clearScenePresentations()
{
    if (m_impl->context.IsNull())
    {
        return;
    }
    for (const SDisplayedObject& displayed : m_impl->displayed)
    {
        if (displayed.connected)
        {
            const Handle(AIS_ConnectedInteractive) connected =
                Handle(AIS_ConnectedInteractive)::DownCast(displayed.presentation);
            if (!connected.IsNull())
            {
                connected->Disconnect();
            }
        }
        m_impl->context->Remove(displayed.presentation, false);
    }
    for (const SSharedPresentation& shared : m_impl->shared_presentations)
    {
        m_impl->context->Remove(shared.prototype, false);
    }
    for (const Handle(AIS_TextLabel) & label : m_impl->measurement_labels)
    {
        m_impl->context->Remove(label, false);
    }
    if (!m_impl->preview.IsNull())
    {
        m_impl->context->Remove(m_impl->preview, false);
        m_impl->preview.Nullify();
    }
    m_impl->displayed.clear();
    m_impl->shared_presentations.clear();
    m_impl->measurement_labels.clear();
}

void SOccViewport::synchronizeScene()
{
    if (!m_impl->initialized || m_impl->context.IsNull())
    {
        return;
    }

    clearScenePresentations();

    QMap<QString, QList<const SSceneObject*>> groups;
    if (m_impl->document)
    {
        for (const SSceneObject& object : m_impl->document->objects())
        {
            if (object.type == SObjectType::Measurement)
            {
                if (object.quality_warning)
                {
                    continue;
                }
                const QJsonObject& properties = object.custom_properties;
                Handle(AIS_TextLabel) label = new AIS_TextLabel();
                const std::wstring text = object.name.toStdWString();
                label->SetText(TCollection_ExtendedString(text.c_str()));
                label->SetPosition(
                    gp_Pnt(measurementCoordinate(properties, QStringLiteral("annotationX"),
                                                 QStringLiteral("centerX")),
                           measurementCoordinate(properties, QStringLiteral("annotationY"),
                                                 QStringLiteral("centerY")),
                           measurementCoordinate(properties, QStringLiteral("annotationZ"),
                                                 QStringLiteral("centerZ"))));
                label->SetColor(Quantity_Color(0.25, 0.82, 1.0, Quantity_TOC_RGB));
                label->SetHeight(14.0);
                m_impl->context->Display(label, false);
                m_impl->context->Deactivate(label);
                m_impl->measurement_labels.push_back(label);
                continue;
            }
            if (object.visible && !object.shape.isNull())
            {
                groups[presentationKey(object, m_impl->display_mode)].push_back(&object);
            }
        }
    }

    for (auto iterator = groups.cbegin(); iterator != groups.cend(); ++iterator)
    {
        const QList<const SSceneObject*>& objects = iterator.value();
        const SSceneObject& first = *objects.front();
        Handle(AIS_Shape) prototype =
            first.use_imported_appearance && first.imported_appearance.valid
                ? Handle(AIS_Shape)(new AIS_ColoredShape(SKernelShapeAccess::native(first.shape)))
                : Handle(AIS_Shape)(new AIS_Shape(SKernelShapeAccess::native(first.shape)));
        configurePresentation(prototype, first, m_impl->display_mode);
        const int triangles = triangleCount(prototype->Shape());
        const bool progressive = m_impl->progressive_rendering_enabled &&
                                 SRenderQualityPolicy::shouldUseProgressiveRendering(triangles);
        if (progressive)
        {
            prototype->SetOwnDeviationCoefficient(
                SRenderQualityPolicy::deviationCoefficient(false));
        }

        if (objects.size() == 1)
        {
            try
            {
                prototype->SetLocalTransformation(toOccTransform(first.transform));
            }
            catch (const Standard_Failure&)
            {
                prototype->ResetTransformation();
            }
            m_impl->context->Display(prototype, false);
            m_impl->displayed.push_back(
                {first.id, prototype, prototype, false, progressive, first.frozen, triangles});
            continue;
        }

        m_impl->shared_presentations.push_back({iterator.key(), prototype, progressive, triangles});
        for (const SSceneObject* object : objects)
        {
            Handle(AIS_ConnectedInteractive) instance = new AIS_ConnectedInteractive();
            try
            {
                instance->Connect(prototype, toOccTransform(object->transform));
            }
            catch (const Standard_Failure&)
            {
                instance->Connect(prototype);
            }
            m_impl->context->Display(instance, false);
            m_impl->displayed.push_back(
                {object->id, instance, prototype, true, progressive, object->frozen, triangles});
        }
    }

    setSelectionMode(m_impl->selection_mode);
    for (const SDisplayedObject& displayed : m_impl->displayed)
    {
        if (displayed.frozen)
        {
            m_impl->context->Deactivate(displayed.presentation);
        }
    }
    m_impl->context->UpdateCurrentViewer();
}

SRenderResourceStatistics SOccViewport::renderResourceStatistics() const
{
    SRenderResourceStatistics statistics;
    statistics.shared_prototypes = static_cast<int>(m_impl->shared_presentations.size());
    for (const SDisplayedObject& displayed : m_impl->displayed)
    {
        const int triangles = triangleCount(displayed.source_shape->Shape());
        statistics.rendered_triangles += triangles;
        if (displayed.connected)
        {
            ++statistics.connected_instances;
        }
        else
        {
            ++statistics.independent_presentations;
            if (!Handle(AIS_ColoredShape)::DownCast(displayed.source_shape).IsNull())
            {
                ++statistics.colored_prototypes;
            }
            statistics.estimated_gpu_geometry_bytes +=
                static_cast<qint64>(triangles) * kEstimatedGpuBytesPerTriangle;
        }
    }
    for (const SSharedPresentation& shared : m_impl->shared_presentations)
    {
        if (!Handle(AIS_ColoredShape)::DownCast(shared.prototype).IsNull())
        {
            ++statistics.colored_prototypes;
        }
        statistics.estimated_gpu_geometry_bytes +=
            static_cast<qint64>(triangleCount(shared.prototype->Shape())) *
            kEstimatedGpuBytesPerTriangle;
    }
    statistics.graphic_structures = statistics.independent_presentations +
                                    statistics.shared_prototypes + statistics.connected_instances;
    if (!m_impl->view.IsNull())
    {
        statistics.occt_statistics =
            QString::fromLatin1(m_impl->view->StatisticInformation().ToCString());
    }
    return statistics;
}
} // namespace smartGraphics3D
