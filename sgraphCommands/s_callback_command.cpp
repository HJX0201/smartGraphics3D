#include "s_callback_command.h"

namespace smartGraphics3D
{
SCallbackCommand::SCallbackCommand(QString id, QString display_name, SBeginCallback begin_callback)
    : m_id(std::move(id)), m_display_name(std::move(display_name)),
      m_begin_callback(std::move(begin_callback))
{
}

QString SCallbackCommand::id() const
{
    return m_id;
}

QString SCallbackCommand::displayName() const
{
    return m_display_name;
}

SResult<void> SCallbackCommand::begin(SCommandContext& context)
{
    if (!m_begin_callback)
    {
        return SResult<void>::failure(SErrorCode::InternalFailure,
                                      QStringLiteral("命令没有可执行入口"));
    }
    m_active = true;
    const SResult<void> result = m_begin_callback(context);
    if (!result)
    {
        m_active = false;
    }
    return result;
}

SResult<void> SCallbackCommand::confirm()
{
    m_active = false;
    return SResult<void>::success();
}

void SCallbackCommand::cancel()
{
    m_active = false;
}

bool SCallbackCommand::hasPreview() const
{
    return false;
}
} // namespace smartGraphics3D
