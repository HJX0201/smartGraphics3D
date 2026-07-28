#pragma once

#include "s_i_command.h"

#include <QHash>
#include <memory>

namespace smartGraphics3D
{
class SCommandRegistry
{
  public:
    SResult<void> registerCommand(std::unique_ptr<SICommand> command);
    SICommand* command(const QString& id) const;
    QStringList commandIds() const;

  private:
    QHash<QString, std::shared_ptr<SICommand>> m_commands;
};
} // namespace smartGraphics3D
