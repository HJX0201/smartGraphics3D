#pragma once

#include "s_i_command.h"

#include <functional>

namespace smartGraphics3D
{
class SCallbackCommand final : public SICommand
{
  public:
    using SBeginCallback = std::function<SResult<void>(SCommandContext&)>;

    SCallbackCommand(QString id, QString display_name, SBeginCallback begin_callback);

    QString id() const override;
    QString displayName() const override;
    SResult<void> begin(SCommandContext& context) override;
    SResult<void> confirm() override;
    void cancel() override;
    bool hasPreview() const override;

  private:
    QString m_id;
    QString m_display_name;
    SBeginCallback m_begin_callback;
    bool m_active = false;
};
} // namespace smartGraphics3D
