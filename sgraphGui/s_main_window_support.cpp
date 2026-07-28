#include "s_callback_command.h"
#include "s_icon_factory.h"
#include "s_main_window.h"

#include <QAction>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QTextEdit>

namespace smartGraphics3D
{
QAction* SMainWindow::makeAction(const QString& text, SIconId icon_id, const QString& shortcut)
{
    QAction* action = new QAction(iconForAction(icon_id), text, this);
    if (!shortcut.isEmpty())
    {
        action->setShortcut(QKeySequence(shortcut));
        action->setShortcutContext(Qt::ApplicationShortcut);
    }
    addAction(action);
    return action;
}

void SMainWindow::bindCommand(QAction* action, const QString& command_id,
                              const std::function<void()>& callback)
{
    auto command =
        std::make_unique<SCallbackCommand>(command_id, action ? action->text() : command_id,
                                           [callback](SCommandContext&)
                                           {
                                               callback();
                                               return SResult<void>::success();
                                           });
    const SResult<void> registered = m_command_registry.registerCommand(std::move(command));
    if (!registered)
    {
        appendLog(QStringLiteral("ERROR"), tr("命令注册失败：%1").arg(command_id),
                  registered.message());
        return;
    }
    if (!action)
    {
        return;
    }

    action->setProperty("commandId", command_id);
    connect(action, &QAction::triggered, this,
            [this, command_id]()
            {
                SCommandContext context;
                context.document = &m_document;
                if (SICommand* command = m_command_registry.command(command_id))
                {
                    const SResult<void> result = command->begin(context);
                    if (result)
                    {
                        command->confirm();
                    }
                    else
                    {
                        showFailure(tr("执行命令失败"), result);
                    }
                }
            });
}

void SMainWindow::refreshWindowTitle()
{
    setWindowTitle(
        QStringLiteral("%1%2 — smartGraphics3D")
            .arg(m_document.projectName(), m_document.isDirty() ? QStringLiteral("*") : QString()));
}

void SMainWindow::appendLog(const QString& level, const QString& message, const QString& details,
                            SErrorCode error_code)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString line = QStringLiteral("[%1] [%2] %3").arg(timestamp, level, message);
    m_log_view->append(line);
    m_console->append(message);
    if (!details.isEmpty())
    {
        m_log_view->append(QStringLiteral("    %1").arg(details));
    }
    QJsonObject record;
    record.insert(QStringLiteral("timestamp"), timestamp);
    record.insert(QStringLiteral("level"), level);
    record.insert(QStringLiteral("module"), QStringLiteral("sgraphGui"));
    record.insert(QStringLiteral("task"), QString());
    record.insert(QStringLiteral("project"), m_document.projectName());
    record.insert(QStringLiteral("message"), message);
    record.insert(QStringLiteral("details"), details);
    record.insert(QStringLiteral("errorCode"), static_cast<int>(error_code));
    m_structured_logs.push_back(
        QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact)));
}
} // namespace smartGraphics3D
