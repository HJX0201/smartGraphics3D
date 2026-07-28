#pragma once

#include "s_kernel_shape.h"
#include "s_result.h"

#include <QString>
#include <QStringList>

namespace smartGraphics3D
{
class S3dDocument;

struct SFileCompatibilityReport
{
    QString format;
    QStringList preserved_properties;
    QStringList lost_properties;
    QStringList warnings;
};

struct SImportedShape
{
    SKernelShape shape;
    QString suggested_name;
    SFileCompatibilityReport report;
};

class SIFileCodec
{
  public:
    virtual ~SIFileCodec() = default;

    virtual QStringList extensions() const = 0;
    virtual SResult<SImportedShape> read(const QString& file_path) const
    {
        Q_UNUSED(file_path);
        return SResult<SImportedShape>::failure(SErrorCode::Unsupported,
                                                QStringLiteral("该编解码器不支持几何导入"));
    }
    virtual SResult<SFileCompatibilityReport> write(const SKernelShape& shape,
                                                    const QString& file_path) const
    {
        Q_UNUSED(shape);
        Q_UNUSED(file_path);
        return SResult<SFileCompatibilityReport>::failure(
            SErrorCode::Unsupported, QStringLiteral("该编解码器不支持几何导出"));
    }
    virtual SResult<void> loadProject(S3dDocument& document, const QString& file_path) const
    {
        Q_UNUSED(document);
        Q_UNUSED(file_path);
        return SResult<void>::failure(SErrorCode::Unsupported,
                                      QStringLiteral("该编解码器不支持项目读取"));
    }
    virtual SResult<void> saveProject(const S3dDocument& document, const QString& file_path) const
    {
        Q_UNUSED(document);
        Q_UNUSED(file_path);
        return SResult<void>::failure(SErrorCode::Unsupported,
                                      QStringLiteral("该编解码器不支持项目保存"));
    }
};
} // namespace smartGraphics3D
