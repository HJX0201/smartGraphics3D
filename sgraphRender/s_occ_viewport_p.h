#pragma once

#include "s_occ_viewport.h"

#include <AIS_ConnectedInteractive.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <QElapsedTimer>
#include <QMap>
#include <QPoint>
#include <QRubberBand>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <vector>

namespace smartGraphics3D
{
struct SDisplayedObject
{
    SObjectId id;
    Handle(AIS_InteractiveObject) presentation;
    Handle(AIS_Shape) source_shape;
    bool connected = false;
    bool progressive = false;
    bool frozen = false;
    int triangle_count = 0;
};

struct SSharedPresentation
{
    QString key;
    Handle(AIS_Shape) prototype;
    bool progressive = false;
    int triangle_count = 0;
};

struct SOccViewport::SImpl
{
    S3dDocument* document = nullptr;
    Handle(Aspect_DisplayConnection) display_connection;
    Handle(OpenGl_GraphicDriver) graphic_driver;
    Handle(V3d_Viewer) viewer;
    Handle(V3d_View) view;
    Handle(AIS_InteractiveContext) context;
    Handle(AIS_Shape) preview;
    std::vector<Handle(AIS_TextLabel)> measurement_labels;
    std::vector<SSharedPresentation> shared_presentations;
    std::vector<Handle(Graphic3d_ClipPlane)> clip_planes;
    QList<SClipPlane> requested_clip_planes;
    QMap<QString, Handle(Graphic3d_Camera)> saved_views;
    std::vector<SDisplayedObject> displayed;
    QPoint last_mouse_position;
    bool rotating = false;
    bool panning = false;
    bool box_selecting = false;
    bool box_zooming = false;
    QPoint box_start;
    QRubberBand* rubber_band = nullptr;
    bool initialized = false;
    bool perspective = false;
    bool free_rotation = false;
    bool progressive_rendering_enabled = true;
    SStandardView standard_view = SStandardView::Isometric;
    SDisplayMode display_mode = SDisplayMode::ShadedWithEdges;
    SSelectionMode selection_mode = SSelectionMode::Object;
    QElapsedTimer frame_timer;
};
} // namespace smartGraphics3D
