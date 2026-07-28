#pragma once

#include <memory>

namespace smartGraphics3D
{
class SKernelShapeAccess;

class SKernelShape
{
  public:
    SKernelShape();
    SKernelShape(const SKernelShape&);
    SKernelShape(SKernelShape&&) noexcept;
    SKernelShape& operator=(const SKernelShape&);
    SKernelShape& operator=(SKernelShape&&) noexcept;
    ~SKernelShape();

    bool isNull() const;
    bool isValid() const;

  private:
    struct SImpl;
    explicit SKernelShape(std::shared_ptr<SImpl> impl);

    std::shared_ptr<SImpl> m_impl;

    friend class SKernelShapeAccess;
};
} // namespace smartGraphics3D
