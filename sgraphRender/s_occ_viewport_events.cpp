#include "s_occ_viewport.h"
#include "s_occ_viewport_p.h"
#include "s_render_quality.h"

#include <AIS_SelectionScheme.hxx>
#include <Aspect_Grid.hxx>
#include <Aspect_GridDrawMode.hxx>
#include <Aspect_GridType.hxx>
#include <Graphic3d_RenderingParams.hxx>
#include <Graphic3d_Vec2.hxx>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <WNT_Window.hxx>

namespace smartGraphics3D
{
QPaintEngine* SOccViewport::paintEngine() const
{
    return nullptr;
}

void SOccViewport::paintEvent(QPaintEvent*)
{
    if (!m_impl->initialized)
    {
        initializeViewer();
    }
    if (m_impl->view.IsNull())
    {
        return;
    }

    m_impl->view->Redraw();
    if (m_impl->frame_timer.isValid())
    {
        const qint64 elapsed = m_impl->frame_timer.restart();
        if (elapsed > 0)
        {
            emit frameRendered(1000.0 / static_cast<double>(elapsed));
        }
    }
    else
    {
        m_impl->frame_timer.start();
    }
}

void SOccViewport::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_impl->view.IsNull() && !m_impl->view->Window().IsNull())
    {
        m_impl->view->Window()->DoResize();
        m_impl->view->MustBeResized();
    }
}

void SOccViewport::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    initializeViewer();
}

void SOccViewport::mousePressEvent(QMouseEvent* event)
{
    m_impl->last_mouse_position = event->pos();
    if (event->button() == Qt::MiddleButton)
    {
        setInteractionQuality(true);
        if (event->modifiers().testFlag(Qt::ShiftModifier))
        {
            m_impl->panning = true;
        }
        else if (!m_impl->view.IsNull())
        {
            m_impl->rotating = true;
            const double roll_threshold = m_impl->free_rotation ? 0.4 : 0.0;
            m_impl->view->StartRotation(event->x(), event->y(), roll_threshold);
        }
    }
    else if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ShiftModifier))
    {
        m_impl->box_selecting = true;
        m_impl->box_zooming = event->modifiers().testFlag(Qt::AltModifier);
        m_impl->box_start = event->pos();
        m_impl->rubber_band->setGeometry(QRect(m_impl->box_start, QSize()));
        m_impl->rubber_band->show();
    }
    else if (event->button() == Qt::LeftButton && !m_impl->context.IsNull())
    {
        m_impl->context->MoveTo(event->x(), event->y(), m_impl->view, false);
        const AIS_SelectionScheme scheme = event->modifiers().testFlag(Qt::ControlModifier)
                                               ? AIS_SelectionScheme_XOR
                                               : AIS_SelectionScheme_Replace;
        m_impl->context->SelectDetected(scheme);
        emitCurrentSelection();
    }
}

void SOccViewport::mouseMoveEvent(QMouseEvent* event)
{
    if (m_impl->rotating && !m_impl->view.IsNull())
    {
        m_impl->view->Rotation(event->x(), event->y());
        emit cameraChanged();
    }
    else if (m_impl->box_selecting)
    {
        m_impl->rubber_band->setGeometry(QRect(m_impl->box_start, event->pos()).normalized());
    }
    else if (m_impl->panning && !m_impl->view.IsNull())
    {
        const QPoint delta = event->pos() - m_impl->last_mouse_position;
        m_impl->view->Pan(delta.x(), -delta.y(), 1.0, false);
        m_impl->last_mouse_position = event->pos();
        emit cameraChanged();
    }
    else if (!m_impl->context.IsNull())
    {
        m_impl->context->MoveTo(event->x(), event->y(), m_impl->view, true);
    }
    emit cursorPositionChanged(event->x(), event->y(), 0.0);
}

void SOccViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_impl->box_selecting && event->button() == Qt::LeftButton && !m_impl->context.IsNull())
    {
        const QRect rectangle = QRect(m_impl->box_start, event->pos()).normalized();
        m_impl->rubber_band->hide();
        m_impl->box_selecting = false;
        if (rectangle.width() > 3 && rectangle.height() > 3 && m_impl->box_zooming)
        {
            m_impl->view->WindowFitAll(rectangle.left(), rectangle.top(), rectangle.right(),
                                       rectangle.bottom());
            emit cameraChanged();
        }
        else if (rectangle.width() > 3 && rectangle.height() > 3)
        {
            m_impl->context->SelectRectangle(Graphic3d_Vec2i(rectangle.left(), rectangle.top()),
                                             Graphic3d_Vec2i(rectangle.right(), rectangle.bottom()),
                                             m_impl->view, AIS_SelectionScheme_Replace);
            m_impl->context->UpdateCurrentViewer();
            emitCurrentSelection();
        }
        m_impl->box_zooming = false;
    }
    m_impl->rotating = false;
    m_impl->panning = false;
    setInteractionQuality(false);
    QWidget::mouseReleaseEvent(event);
}

void SOccViewport::wheelEvent(QWheelEvent* event)
{
    if (!m_impl->view.IsNull())
    {
        setInteractionQuality(true);
        const int direction = event->angleDelta().y() > 0 ? 20 : -20;
        m_impl->view->StartZoomAtPoint(event->x(), event->y());
        m_impl->view->ZoomAtPoint(event->x(), event->y(), event->x(), event->y() + direction);
        emit cameraChanged();
        QTimer::singleShot(160, this,
                           [this]()
                           {
                               setInteractionQuality(false);
                           });
    }
    event->accept();
}

void SOccViewport::initializeViewer()
{
    if (m_impl->initialized || !isVisible())
    {
        return;
    }
    m_impl->display_connection = new Aspect_DisplayConnection();
    m_impl->graphic_driver = new OpenGl_GraphicDriver(m_impl->display_connection);
    m_impl->viewer = new V3d_Viewer(m_impl->graphic_driver);
    m_impl->viewer->SetDefaultLights();
    m_impl->viewer->SetLightOn();
    m_impl->viewer->SetRectangularGridValues(0.0, 0.0, 10.0, 10.0, 0.0);
    m_impl->viewer->SetRectangularGridGraphicValues(2000.0, 2000.0, 0.0);
    m_impl->viewer->Grid(Aspect_GT_Rectangular)
        ->SetColors(Quantity_Color(0.035, 0.055, 0.070, Quantity_TOC_RGB),
                    Quantity_Color(0.075, 0.115, 0.145, Quantity_TOC_RGB));
    if (m_impl->grid_visible)
    {
        m_impl->viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
    }
    m_impl->context = new AIS_InteractiveContext(m_impl->viewer);
    m_impl->view = m_impl->viewer->CreateView();
    m_impl->view->ChangeRenderingParams().CollectedStats =
        Graphic3d_RenderingParams::PerfCounters_All;
    m_impl->view->ChangeRenderingParams().StatsUpdateInterval = 0.0F;
    m_impl->view->SetProj(V3d_XposYnegZpos);

    Handle(WNT_Window) window =
        new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()), Quantity_NOC_BLACK);
    m_impl->view->SetWindow(window);
    if (!window->IsMapped())
    {
        window->Map();
    }
    m_impl->view->SetBgGradientColors(Quantity_Color(0.003, 0.006, 0.009, Quantity_TOC_RGB),
                                      Quantity_Color(0.010, 0.018, 0.025, Quantity_TOC_RGB),
                                      Aspect_GFM_VER, false);
    m_impl->view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
    m_impl->initialized = true;
    const QList<SClipPlane> clip_planes = m_impl->requested_clip_planes;
    synchronizeScene();
    setStandardView(m_impl->standard_view);
    setPerspective(m_impl->perspective);
    setDisplayMode(m_impl->display_mode);
    setClipPlanes(clip_planes);
    fitAll();
}

void SOccViewport::emitCurrentSelection()
{
    emit selectionChanged(selectedObjectIds());
    emit subSelectionChanged(selectedSelections());
}

void SOccViewport::setInteractionQuality(bool preview)
{
    if (m_impl->context.IsNull())
    {
        return;
    }
    bool changed = false;
    for (SDisplayedObject& displayed : m_impl->displayed)
    {
        if (!displayed.progressive || displayed.connected)
        {
            continue;
        }
        displayed.source_shape->SetOwnDeviationCoefficient(
            SRenderQualityPolicy::deviationCoefficient(preview));
        m_impl->context->Redisplay(displayed.source_shape, false);
        changed = true;
    }
    for (SSharedPresentation& shared : m_impl->shared_presentations)
    {
        if (!shared.progressive)
        {
            continue;
        }
        shared.prototype->SetOwnDeviationCoefficient(
            SRenderQualityPolicy::deviationCoefficient(preview));
        m_impl->context->Redisplay(shared.prototype, false);
        changed = true;
    }
    if (changed)
    {
        m_impl->context->UpdateCurrentViewer();
    }
}
} // namespace smartGraphics3D
