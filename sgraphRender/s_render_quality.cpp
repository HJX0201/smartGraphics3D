#include "s_render_quality.h"

namespace smartGraphics3D
{
bool SRenderQualityPolicy::shouldUseProgressiveRendering(int triangle_count)
{
    return triangle_count >= kLargeMeshTriangleCount;
}

double SRenderQualityPolicy::deviationCoefficient(bool interaction_preview)
{
    return interaction_preview ? 0.08 : 0.005;
}
} // namespace smartGraphics3D
