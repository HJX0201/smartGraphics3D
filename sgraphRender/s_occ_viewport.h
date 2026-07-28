#pragma once

#include "s_kernel_shape.h"
#include "s_types.h"

#include <QPoint>
#include <QWidget>
#include <memory>

namespace smartGraphics3D
{
class S3dDocument;

enum class SStandardView
{
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom,
    Isometric
};

struct SClipPlane
{
    QVector3D normal = QVector3D(0.0F, 0.0F, 1.0F);
    double offset = 0.0;
    bool flipped = false;
};

struct SRenderResourceStatistics
{
    int independent_presentations = 0;
    int shared_prototypes = 0;
    int colored_prototypes = 0;
    int connected_instances = 0;
    int graphic_structures = 0;
    qint64 rendered_triangles = 0;
    qint64 estimated_gpu_geometry_bytes = 0;
    QString occt_statistics;
};

class SOccViewport final : public QWidget
{
    Q_OBJECT

  public:
    explicit SOccViewport(QWidget* parent = nullptr);
    ~SOccViewport() override;

    void setDocument(S3dDocument* document);
    void synchronizeScene();
    void fitAll();
    void fitSelection();
    void setStandardView(SStandardView view);
    void setPerspective(bool enabled);
    void setFreeRotation(bool enabled);
    void setDisplayMode(SDisplayMode mode);
    void setSelectionMode(SSelectionMode mode);
    void setProgressiveRenderingEnabled(bool enabled);
    void setClipPlane(bool enabled, double z_offset = 0.0, bool flipped = false);
    void setClipPlanes(const QList<SClipPlane>& planes);
    void saveView(const QString& name);
    bool restoreView(const QString& name);
    QStringList savedViewNames() const;
    void setRotationCenterFromSelection();
    void copyCameraFrom(const SOccViewport& other);
    void showPreview(const SKernelShape& shape);
    void clearPreview();
    void selectObject(const SObjectId& id, bool add = false);
    void selectAllObjects();
    void invertObjectSelection();
    void setSelectedObjects(const QList<SObjectId>& ids);
    QList<SObjectId> selectedObjectIds() const;
    QList<SSelection> selectedSelections() const;
    bool isInitialized() const;
    bool isPerspective() const;
    bool isFreeRotation() const;
    bool isGridActive() const;
    SStandardView standardView() const;
    SDisplayMode displayMode() const;
    SSelectionMode selectionMode() const;
    int clipPlaneCount() const;
    int progressiveObjectCount() const;
    SRenderResourceStatistics renderResourceStatistics() const;

  signals:
    void selectionChanged(const QList<smartGraphics3D::SObjectId>& ids);
    void subSelectionChanged(const QList<smartGraphics3D::SSelection>& selections);
    void cursorPositionChanged(double x, double y, double z);
    void frameRendered(double frames_per_second);
    void cameraChanged();

  protected:
    QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

  private:
    void initializeViewer();
    void clearScenePresentations();
    void emitCurrentSelection();
    void setInteractionQuality(bool preview);

    struct SImpl;
    std::unique_ptr<SImpl> m_impl;
};
} // namespace smartGraphics3D

Q_DECLARE_METATYPE(QList<smartGraphics3D::SObjectId>)
Q_DECLARE_METATYPE(smartGraphics3D::SSelection)
Q_DECLARE_METATYPE(QList<smartGraphics3D::SSelection>)
