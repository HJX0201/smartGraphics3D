#pragma once

#include "s_result.h"
#include "s_types.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <functional>

namespace smartGraphics3D
{
class S3dDocument;

class SDocumentTransaction final
{
  public:
    SDocumentTransaction(S3dDocument& document, QString operation_name,
                         QList<SObjectId> affected_ids = {}, QString parameter_summary = {});

    SResult<void> commit(const std::function<SResult<void>()>& mutation);
    void cancel();
    bool isFinished() const;

  private:
    QPointer<S3dDocument> m_document;
    QString m_operation_name;
    QList<SObjectId> m_affected_ids;
    QString m_parameter_summary;
    bool m_finished = false;
};
} // namespace smartGraphics3D
