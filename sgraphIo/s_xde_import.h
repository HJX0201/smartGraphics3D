#pragma once

#include "s_i_file_codec.h"

namespace smartGraphics3D
{
SResult<SImportedShape> importXdeFile(const QString& file_path, const QString& extension,
                                      SFileCompatibilityReport report);
} // namespace smartGraphics3D
