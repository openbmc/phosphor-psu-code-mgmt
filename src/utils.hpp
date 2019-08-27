#pragma once

#include <experimental/any>
#include <sdbusplus/bus.hpp>
#include <string>
#include <vector>

namespace utils
{

class UtilsInterface;

// Due to a libstdc++ bug, we got compile error using std::any with gmock.
// A temporary workaround is to use std::experimental::any.
// See details in https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90415
using std::experimental::any;
using std::experimental::any_cast;

/**
 * @brief Get the implementation of UtilsInterface
 */
const UtilsInterface& getUtils();

/**
 * @brief Get PSU inventory object path from DBus
 */
std::vector<std::string> getPSUInventoryPath(sdbusplus::bus::bus& bus);

/** @brief Get service name from object path and interface
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] path         - The Dbus object path
 * @param[in] interface    - The Dbus interface
 *
 * @return The name of the service
 */
std::string getService(sdbusplus::bus::bus& bus, const char* path,
                       const char* interface);

/** @brief The template function to get property from the requested dbus path
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] service      - The Dbus service name
 * @param[in] path         - The Dbus object path
 * @param[in] interface    - The Dbus interface
 * @param[in] propertyName - The property name to get
 *
 * @return The value of the property
 */
template <typename T>
T getProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,
              const char* interface, const char* propertyName);

/**
 * @brief Calculate the version id from the version string.
 *
 * @details The version id is a unique 8 hexadecimal digit id
 *          calculated from the version string.
 *
 * @param[in] version - The image version string (e.g. v1.99.10-19).
 *
 * @return The id.
 */
std::string getVersionId(const std::string& version);

/** @brief Get version of PSU specificied by the inventory path
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] inventoryPath - The PSU inventory object path
 *
 * @return The version string, or empry string if it fails to get the version
 */
std::string getVersion(const std::string& inventoryPath);

/**
 * @brief The interface for utils
 */
class UtilsInterface
{
  public:
    // For now the code needs to get property for Present and Version
    using PropertyType = sdbusplus::message::variant<std::string, bool>;

    virtual ~UtilsInterface() = default;

    virtual std::vector<std::string>
        getPSUInventoryPath(sdbusplus::bus::bus& bus) const = 0;

    virtual std::string getService(sdbusplus::bus::bus& bus, const char* path,
                                   const char* interface) const = 0;

    virtual std::string getVersionId(const std::string& version) const = 0;

    virtual std::string getVersion(const std::string& inventoryPath) const = 0;

    virtual any getPropertyImpl(sdbusplus::bus::bus& bus, const char* service,
                                const char* path, const char* interface,
                                const char* propertyName) const = 0;

    template <typename T>
    T getProperty(sdbusplus::bus::bus& bus, const char* service,
                  const char* path, const char* interface,
                  const char* propertyName) const
    {
        any result =
            getPropertyImpl(bus, service, path, interface, propertyName);
        auto value = any_cast<PropertyType>(result);
        return sdbusplus::message::variant_ns::get<T>(value);
    }
};

class Utils : public UtilsInterface
{
  public:
    std::vector<std::string>
        getPSUInventoryPath(sdbusplus::bus::bus& bus) const override;

    std::string getService(sdbusplus::bus::bus& bus, const char* path,
                           const char* interface) const override;

    std::string getVersionId(const std::string& version) const override;

    std::string getVersion(const std::string& inventoryPath) const override;

    any getPropertyImpl(sdbusplus::bus::bus& bus, const char* service,
                        const char* path, const char* interface,
                        const char* propertyName) const override;
};

inline std::string getService(sdbusplus::bus::bus& bus, const char* path,
                              const char* interface)
{
    return getUtils().getService(bus, path, interface);
}

inline std::vector<std::string> getPSUInventoryPath(sdbusplus::bus::bus& bus)
{
    return getUtils().getPSUInventoryPath(bus);
}

inline std::string getVersionId(const std::string& version)
{
    return getUtils().getVersionId(version);
}

inline std::string getVersion(const std::string& inventoryPath)
{
    return getUtils().getVersion(inventoryPath);
}

template <typename T>
T getProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,
              const char* interface, const char* propertyName)
{
    return getUtils().getProperty<T>(bus, service, path, interface,
                                     propertyName);
}

} // namespace utils
