#pragma once

#include <QString>
#include <optional>
#include <utility>

namespace smartGraphics3D
{
enum class SErrorCode
{
    None,
    InvalidArgument,
    NotFound,
    Locked,
    Conflict,
    Cancelled,
    Unsupported,
    GeometryFailure,
    FileFailure,
    CorruptData,
    VersionMismatch,
    InternalFailure
};

template <typename T> class SResult
{
  public:
    SResult() = default;

    static SResult success(T value)
    {
        return SResult(std::move(value), SErrorCode::None, {}, {});
    }

    static SResult failure(SErrorCode code, QString message, QString details = {})
    {
        return SResult(std::nullopt, code, std::move(message), std::move(details));
    }

    bool isSuccess() const
    {
        return m_value.has_value();
    }

    explicit operator bool() const
    {
        return isSuccess();
    }

    const T& value() const
    {
        return m_value.value();
    }

    T& value()
    {
        return m_value.value();
    }

    T takeValue()
    {
        return std::move(m_value.value());
    }

    SErrorCode errorCode() const
    {
        return m_error_code;
    }

    const QString& message() const
    {
        return m_message;
    }

    const QString& details() const
    {
        return m_details;
    }

  private:
    SResult(std::optional<T> value, SErrorCode code, QString message, QString details)
        : m_value(std::move(value)), m_error_code(code), m_message(std::move(message)),
          m_details(std::move(details))
    {
    }

    std::optional<T> m_value;
    SErrorCode m_error_code = SErrorCode::None;
    QString m_message;
    QString m_details;
};

template <> class SResult<void>
{
  public:
    SResult() = default;

    static SResult success()
    {
        return SResult(true, SErrorCode::None, {}, {});
    }

    static SResult failure(SErrorCode code, QString message, QString details = {})
    {
        return SResult(false, code, std::move(message), std::move(details));
    }

    bool isSuccess() const
    {
        return m_success;
    }

    explicit operator bool() const
    {
        return isSuccess();
    }

    SErrorCode errorCode() const
    {
        return m_error_code;
    }

    const QString& message() const
    {
        return m_message;
    }

    const QString& details() const
    {
        return m_details;
    }

  private:
    SResult(bool success, SErrorCode code, QString message, QString details)
        : m_success(success), m_error_code(code), m_message(std::move(message)),
          m_details(std::move(details))
    {
    }

    bool m_success = false;
    SErrorCode m_error_code = SErrorCode::None;
    QString m_message;
    QString m_details;
};
} // namespace smartGraphics3D
