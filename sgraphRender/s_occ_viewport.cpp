#include "s_occ_viewport.h"

#include "s_3d_document.h"
#include "s_kernel_shape_access.h"
#include "s_occ_viewport_p.h"
#include "s_render_quality.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_SelectionScheme.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <Aspect_Grid.hxx>
#include <Aspect_GridDrawMode.hxx>
#include <Aspect_GridType.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <Poly_Triangulation.hxx>
#include <Prs3d_Drawer.hxx>
#include <QJsonObject>
#include <QRubberBand>
#include <Quantity_Color.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <algorithm>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

namespace smartGraphics3D
{
namespace
{
int occSelectionMode(SSelectionMode mode)
{
    switch (mode)
    {
    case SSelectionMode::Object:
        return 0;
    case SSelectionMode::Solid:
        return AIS_Shape::SelectionMode(TopAbs_SOLID);
    case SSelectionMode::Face:
        return AIS_Shape::SelectionMode(TopAbs_FACE);
    case SSelectionMode::Edge:
        return AIS_Shape::SelectionMode(TopAbs_EDGE);
    case SSelectionMode::Vertex:
        return AIS_Shape::SelectionMode(TopAbs_VERTEX);
    }
    return 0;
}

} // namespace

SOccViewport::SOccViewport(QWidget* parent) : QWidget(parent), m_impl(std::make_unique<SImpl>())
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
    m_impl->rubber_band = new QRubberBand(QRubberBand::Rectangle, this);
    qRegisterMetaType<QList<SObjectId>>();
    qRegisterMetaType<QList<SSelection>>();
}

SOccViewport::~SOccViewport()
{
    clearScenePresentations();
}

void SOccViewport::setDocument(S3dDocument* document)
{
    if (m_impl->document == document)
    {
        return;
    }
    if (m_impl->document)
    {
        disconnect(m_impl->document, nullptr, this, nullptr);
    }
    m_impl->document = document;
    if (document)
    {
        connect(document, &S3dDocument::documentChanged, this, &SOccViewport::synchronizeScene);
    }
    synchronizeScene();
}

void SOccViewport::showPreview(const SKernelShape& shape)
{
    if (m_impl->context.IsNull() || shape.isNull())
    {
        return;
    }
    clearPreview();
    m_impl->preview = new AIS_Shape(SKernelShapeAccess::native(shape));
    m_impl->preview->SetColor(Quantity_Color(0.1, 0.65, 0.95, Quantity_TOC_RGB));
    m_impl->preview->SetTransparency(0.45);
    m_impl->preview->Attributes()->SetFaceBoundaryDraw(true);
    m_impl->context->Display(m_impl->preview, false);
    m_impl->context->Deactivate(m_impl->preview);
    m_impl->context->UpdateCurrentViewer();
}

void SOccViewport::clearPreview()
{
    if (!m_impl->context.IsNull() && !m_impl->preview.IsNull())
    {
        m_impl->context->Remove(m_impl->preview, false);
        m_impl->preview.Nullify();
        m_impl->context->UpdateCurrentViewer();
    }
}

void SOccViewport::fitAll()
{
    if (!m_impl->view.IsNull())
    {
        m_impl->view->FitAll(0.01, false);
        m_impl->view->ZFitAll();
        m_impl->view->Redraw();
        emit cameraChanged();
    }
}

void SOccViewport::fitSelection()
{
    if (m_impl->view.IsNull() || m_impl->context.IsNull())
    {
        return;
    }
    Bnd_Box bounds;
    for (m_impl->context->InitSelected(); m_impl->context->MoreSelected();
         m_impl->context->NextSelected())
    {
        BRepBndLib::Add(m_impl->context->SelectedShape(), bounds);
    }
    if (bounds.IsVoid())
    {
        fitAll();
        return;
    }
    m_impl->view->FitAll(bounds, 0.05, false);
    m_impl->view->ZFitAll();
    m_impl->view->Redraw();
    emit cameraChanged();
}

void SOccViewport::setStandardView(SStandardView view)
{
    m_impl->standard_view = view;
    if (m_impl->view.IsNull())
    {
        return;
    }
    switch (view)
    {
    case SStandardView::Front:
        m_impl->view->SetProj(V3d_Yneg);
        break;
    case SStandardView::Back:
        m_impl->view->SetProj(V3d_Ypos);
        break;
    case SStandardView::Left:
        m_impl->view->SetProj(V3d_Xpos);
        break;
    case SStandardView::Right:
        m_impl->view->SetProj(V3d_Xneg);
        break;
    case SStandardView::Top:
        m_impl->view->SetProj(V3d_Zpos);
        break;
    case SStandardView::Bottom:
        m_impl->view->SetProj(V3d_Zneg);
        break;
    case SStandardView::Isometric:
        m_impl->view->SetProj(V3d_XposYnegZpos);
        break;
    }
    fitAll();
}

void SOccViewport::copyCameraFrom(const SOccViewport& other)
{
    if (m_impl->view.IsNull() || other.m_impl->view.IsNull())
    {
        return;
    }
    m_impl->view->SetCamera(new Graphic3d_Camera(other.m_impl->view->Camera()));
    m_impl->perspective = other.m_impl->perspective;
    m_impl->view->Redraw();
}

void SOccViewport::setPerspective(bool enabled)
{
    m_impl->perspective = enabled;
    if (m_impl->view.IsNull())
    {
        return;
    }
    m_impl->view->Camera()->SetProjectionType(enabled ? Graphic3d_Camera::Projection_Perspective
                                                      : Graphic3d_Camera::Projection_Orthographic);
    fitAll();
}

void SOccViewport::setFreeRotation(bool enabled)
{
    m_impl->free_rotation = enabled;
}

void SOccViewport::setDisplayMode(SDisplayMode mode)
{
    m_impl->display_mode = mode;
    if (m_impl->context.IsNull())
    {
        return;
    }
    const bool wireframe = mode == SDisplayMode::Wireframe || mode == SDisplayMode::HiddenLine;
    m_impl->view->SetComputedMode(mode == SDisplayMode::HiddenLine);
    for (SDisplayedObject& displayed : m_impl->displayed)
    {
        if (displayed.connected)
        {
            continue;
        }
        displayed.source_shape->SetDisplayMode(wireframe ? AIS_WireFrame : AIS_Shaded);
        displayed.source_shape->Attributes()->SetFaceBoundaryDraw(mode ==
                                                                  SDisplayMode::ShadedWithEdges);
        displayed.source_shape->SetTransparency(mode == SDisplayMode::Transparent ? 0.65 : 0.0);
        m_impl->context->Redisplay(displayed.source_shape, false);
    }
    for (SSharedPresentation& shared : m_impl->shared_presentations)
    {
        shared.prototype->SetDisplayMode(wireframe ? AIS_WireFrame : AIS_Shaded);
        shared.prototype->Attributes()->SetFaceBoundaryDraw(mode == SDisplayMode::ShadedWithEdges);
        shared.prototype->SetTransparency(mode == SDisplayMode::Transparent ? 0.65 : 0.0);
        m_impl->context->Redisplay(shared.prototype, false);
    }
    m_impl->context->UpdateCurrentViewer();
}

void SOccViewport::setSelectionMode(SSelectionMode mode)
{
    m_impl->selection_mode = mode;
    if (m_impl->context.IsNull())
    {
        return;
    }
    m_impl->context->Deactivate();
    m_impl->context->Activate(occSelectionMode(mode));
}

void SOccViewport::setClipPlane(bool enabled, double z_offset, bool flipped)
{
    setClipPlanes(enabled ? QList<SClipPlane>{{QVector3D(0.0F, 0.0F, 1.0F), z_offset, flipped}}
                          : QList<SClipPlane>{});
}

void SOccViewport::setClipPlanes(const QList<SClipPlane>& planes)
{
    m_impl->requested_clip_planes = planes;
    if (m_impl->view.IsNull())
    {
        return;
    }
    for (const Handle(Graphic3d_ClipPlane) & plane : m_impl->clip_planes)
    {
        m_impl->view->RemoveClipPlane(plane);
    }
    m_impl->clip_planes.clear();
    for (const SClipPlane& definition : planes)
    {
        if (definition.normal.lengthSquared() < 1.0e-12F)
        {
            continue;
        }
        QVector3D vector = definition.normal.normalized();
        if (definition.flipped)
        {
            vector = -vector;
        }
        const gp_Dir normal(vector.x(), vector.y(), vector.z());
        Handle(Graphic3d_ClipPlane) plane = new Graphic3d_ClipPlane(
            gp_Pln(gp_Pnt(normal.X() * definition.offset, normal.Y() * definition.offset,
                          normal.Z() * definition.offset),
                   normal));
        plane->SetCapping(true);
        m_impl->view->AddClipPlane(plane);
        m_impl->clip_planes.push_back(plane);
    }
    m_impl->view->Redraw();
}

void SOccViewport::saveView(const QString& name)
{
    if (!name.trimmed().isEmpty() && !m_impl->view.IsNull())
    {
        m_impl->saved_views.insert(name.trimmed(), new Graphic3d_Camera(m_impl->view->Camera()));
    }
}

bool SOccViewport::restoreView(const QString& name)
{
    const auto iterator = m_impl->saved_views.constFind(name);
    if (iterator == m_impl->saved_views.cend() || m_impl->view.IsNull())
    {
        return false;
    }
    m_impl->view->SetCamera(new Graphic3d_Camera(iterator.value()));
    m_impl->view->Redraw();
    emit cameraChanged();
    return true;
}

QStringList SOccViewport::savedViewNames() const
{
    return m_impl->saved_views.keys();
}

void SOccViewport::setRotationCenterFromSelection()
{
    if (m_impl->view.IsNull() || m_impl->context.IsNull())
    {
        return;
    }
    Bnd_Box bounds;
    for (m_impl->context->InitSelected(); m_impl->context->MoreSelected();
         m_impl->context->NextSelected())
    {
        BRepBndLib::Add(m_impl->context->SelectedShape(), bounds);
    }
    if (bounds.IsVoid())
    {
        return;
    }
    double x_min = 0.0;
    double y_min = 0.0;
    double z_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;
    bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
    m_impl->view->Camera()->SetCenter(
        gp_Pnt((x_min + x_max) * 0.5, (y_min + y_max) * 0.5, (z_min + z_max) * 0.5));
    m_impl->view->Redraw();
    emit cameraChanged();
}

void SOccViewport::selectObject(const SObjectId& id, bool add)
{
    QList<SObjectId> ids = add ? selectedObjectIds() : QList<SObjectId>{};
    if (!ids.contains(id))
    {
        ids.push_back(id);
    }
    setSelectedObjects(ids);
}

void SOccViewport::selectAllObjects()
{
    QList<SObjectId> ids;
    for (const SDisplayedObject& displayed : m_impl->displayed)
    {
        if (!displayed.frozen)
        {
            ids.push_back(displayed.id);
        }
    }
    setSelectedObjects(ids);
}

void SOccViewport::invertObjectSelection()
{
    const QList<SObjectId> selected = selectedObjectIds();
    QList<SObjectId> inverted;
    for (const SDisplayedObject& displayed : m_impl->displayed)
    {
        if (!displayed.frozen && !selected.contains(displayed.id))
        {
            inverted.push_back(displayed.id);
        }
    }
    setSelectedObjects(inverted);
}

void SOccViewport::setSelectedObjects(const QList<SObjectId>& ids)
{
    if (m_impl->context.IsNull())
    {
        return;
    }
    m_impl->context->ClearSelected(false);
    for (const SDisplayedObject& displayed : m_impl->displayed)
    {
        if (!displayed.frozen && ids.contains(displayed.id))
        {
            m_impl->context->AddOrRemoveSelected(displayed.presentation, false);
        }
    }
    m_impl->context->UpdateCurrentViewer();
    emitCurrentSelection();
}

QList<SObjectId> SOccViewport::selectedObjectIds() const
{
    QList<SObjectId> ids;
    for (const SSelection& selection : selectedSelections())
    {
        if (!ids.contains(selection.object_id))
        {
            ids.push_back(selection.object_id);
        }
    }
    return ids;
}

QList<SSelection> SOccViewport::selectedSelections() const
{
    QList<SSelection> selections;
    if (m_impl->context.IsNull())
    {
        return selections;
    }
    for (m_impl->context->InitSelected(); m_impl->context->MoreSelected();
         m_impl->context->NextSelected())
    {
        const Handle(AIS_InteractiveObject) selected = m_impl->context->SelectedInteractive();
        for (const SDisplayedObject& displayed : m_impl->displayed)
        {
            if (displayed.presentation != selected)
            {
                continue;
            }
            SSelection selection;
            selection.object_id = displayed.id;
            selection.mode = m_impl->selection_mode;
            if (selection.mode != SSelectionMode::Object)
            {
                const TopoDS_Shape selected_shape = m_impl->context->SelectedShape();
                TopTools_IndexedMapOfShape map;
                TopExp::MapShapes(displayed.source_shape->Shape(), selected_shape.ShapeType(), map);
                for (int index = 1; index <= map.Extent(); ++index)
                {
                    if (map(index).IsPartner(selected_shape))
                    {
                        selection.sub_shape_index = index;
                        break;
                    }
                }
            }
            selections.push_back(selection);
            break;
        }
    }
    return selections;
}

bool SOccViewport::isInitialized() const
{
    return m_impl->initialized;
}

bool SOccViewport::isPerspective() const
{
    return m_impl->perspective;
}

bool SOccViewport::isFreeRotation() const
{
    return m_impl->free_rotation;
}

bool SOccViewport::isGridActive() const
{
    return !m_impl->viewer.IsNull() && m_impl->viewer->IsGridActive();
}

SStandardView SOccViewport::standardView() const
{
    return m_impl->standard_view;
}

SDisplayMode SOccViewport::displayMode() const
{
    return m_impl->display_mode;
}

SSelectionMode SOccViewport::selectionMode() const
{
    return m_impl->selection_mode;
}

void SOccViewport::setProgressiveRenderingEnabled(bool enabled)
{
    m_impl->progressive_rendering_enabled = enabled;
}

int SOccViewport::clipPlaneCount() const
{
    return static_cast<int>(m_impl->clip_planes.size());
}

int SOccViewport::progressiveObjectCount() const
{
    return static_cast<int>(std::count_if(m_impl->displayed.cbegin(), m_impl->displayed.cend(),
                                          [](const SDisplayedObject& object)
                                          {
                                              return object.progressive;
                                          }));
}
} // namespace smartGraphics3D
