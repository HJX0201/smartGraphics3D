#pragma once

#include "s_3d_document.h"
#include "s_i_file_codec.h"
#include "s_result.h"

#include <QString>

namespace smartGraphics3D
{
class SProjectCodec final : public SIFileCodec
{
  public:
    QStringList extensions() const override;
    SResult<void> loadProject(S3dDocument& document, const QString& file_path) const override;
    SResult<void> saveProject(const S3dDocument& document, const QString& file_path) const override;
    SResult<void> save(const S3dDocument& document, const QString& file_path) const;
    SResult<void> load(S3dDocument& document, const QString& file_path) const;
    SResult<void> createArchive(const S3dDocument& document, const QString& directory_path) const;
};
} // namespace smartGraphics3D
