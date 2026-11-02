/**
 * @file DomainError.cpp
 * @brief Реализация исключения предметной области.
 */
#include "DomainError.h"

namespace sedm {

DomainError::DomainError(const QString& message)
    : std::runtime_error(message.toUtf8().toStdString())
{
}

QString DomainError::message() const
{
    return QString::fromUtf8(what());
}

}  // namespace sedm
