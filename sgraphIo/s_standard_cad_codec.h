#pragma once

#include "s_i_file_codec.h"

namespace smartGraphics3D
{
class SStandardCadCodec final : public SIFileCodec
{
  public:
    QStringList extensions() const override;
    SResult<SFileCompatibilityReport> compatibilityReport(const QString& file_path) const;
    SResult<SImportedShape> read(const QString& file_path) const override;
    SResult<SFileCompatibilityReport> write(const SKernelShape& shape,
                                            const QString& file_path) const override;
};
} // namespace smartGraphics3D
