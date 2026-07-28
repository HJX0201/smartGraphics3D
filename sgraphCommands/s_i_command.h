#pragma once

#include "s_result.h"

#include <QString>

namespace smartGraphics3D
{
class S3dDocument;

struct SCommandContext
{
    S3dDocument* document = nullptr;
};

class SICommand
{
  public:
    virtual ~SICommand() = default;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual SResult<void> begin(SCommandContext& context) = 0;
    virtual SResult<void> confirm() = 0;
    virtual void cancel() = 0;
    virtual bool hasPreview() const = 0;
};
} // namespace smartGraphics3D
