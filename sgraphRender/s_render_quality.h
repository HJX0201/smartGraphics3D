#pragma once

namespace smartGraphics3D
{
class SRenderQualityPolicy
{
  public:
    static constexpr int kLargeMeshTriangleCount = 200000;

    static bool shouldUseProgressiveRendering(int triangle_count);
    static double deviationCoefficient(bool interaction_preview);
};
} // namespace smartGraphics3D
