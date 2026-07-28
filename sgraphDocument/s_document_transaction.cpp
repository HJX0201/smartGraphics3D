#include "s_document_transaction.h"

#include "s_3d_document.h"

namespace smartGraphics3D
{
SDocumentTransaction::SDocumentTransaction(S3dDocument& document, QString operation_name,
                                           QList<SObjectId> affected_ids, QString parameter_summary)
    : m_document(&document), m_operation_name(std::move(operation_name)),
      m_affected_ids(std::move(affected_ids)), m_parameter_summary(std::move(parameter_summary))
{
}

SResult<void> SDocumentTransaction::commit(const std::function<SResult<void>()>& mutation)
{
    if (m_finished)
    {
        return SResult<void>::failure(SErrorCode::Conflict,
                                      QObject::tr("文档事务已经结束，不能重复提交"));
    }
    m_finished = true;
    if (!m_document)
    {
        return SResult<void>::failure(SErrorCode::NotFound, QObject::tr("目标文档已关闭"));
    }
    return m_document->commit(std::move(m_operation_name), mutation, m_affected_ids,
                              std::move(m_parameter_summary));
}

void SDocumentTransaction::cancel()
{
    m_finished = true;
}

bool SDocumentTransaction::isFinished() const
{
    return m_finished;
}
} // namespace smartGraphics3D
