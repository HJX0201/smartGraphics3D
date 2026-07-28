#include "s_command_registry.h"

namespace smartGraphics3D
{
SResult<void> SCommandRegistry::registerCommand(std::unique_ptr<SICommand> command)
{
    if (!command || command->id().trimmed().isEmpty())
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument, QStringLiteral("命令无效"));
    }
    if (m_commands.contains(command->id()))
    {
        return SResult<void>::failure(SErrorCode::InvalidArgument,
                                      QStringLiteral("命令 ID 已存在：%1").arg(command->id()));
    }
    const QString id = command->id();
    m_commands.insert(id, std::shared_ptr<SICommand>(std::move(command)));
    return SResult<void>::success();
}

SICommand* SCommandRegistry::command(const QString& id) const
{
    const auto iterator = m_commands.constFind(id);
    return iterator == m_commands.constEnd() ? nullptr : iterator.value().get();
}

QStringList SCommandRegistry::commandIds() const
{
    return m_commands.keys();
}
} // namespace smartGraphics3D
