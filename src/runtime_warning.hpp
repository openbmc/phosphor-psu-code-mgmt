#pragma once

#include <exception>
#include <string>

namespace phosphor::software::updater
{

/**
 * @class RuntimeWarning
 *
 * Exception class to report a runtime warning condition.
 */
class RuntimeWarning : public std::exception
{
  public:
    // Specify which compiler-generated methods we want
    RuntimeWarning() = delete;
    RuntimeWarning(const RuntimeWarning&) = default;
    RuntimeWarning(RuntimeWarning&&) = default;
    RuntimeWarning& operator=(const RuntimeWarning&) = default;
    RuntimeWarning& operator=(RuntimeWarning&&) = default;
    ~RuntimeWarning() override = default;

    /** @brief Constructor.
     *
     * @param error error message
     */
    explicit RuntimeWarning(const std::string& error) : error{error} {}

    /** @brief Returns the description of this error.
     *
     * @return error description
     */
    const char* what() const noexcept override
    {
        return error.c_str();
    }

  private:
    /** @brief Error message */
    std::string error;
};

} // namespace phosphor::software::updater
