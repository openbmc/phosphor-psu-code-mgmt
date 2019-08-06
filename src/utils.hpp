#pragma once

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <string>
#include <vector>

namespace utils
{

using namespace phosphor::logging;

/**
 * @brief Get PSU inventory object path from config
 */
std::vector<std::string> getPSUInventoryPath();

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
              const char* interface, const char* propertyName)
{
    auto method = bus.new_method_call(service, path,
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(interface, propertyName);
    try
    {
        sdbusplus::message::variant<T> value{};
        auto reply = bus.call(method);
        reply.read(value);
        return sdbusplus::message::variant_ns::get<T>(value);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("GetProperty call failed", entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface),
                        entry("PROPERTY=%s", propertyName));
        throw std::runtime_error("GetProperty call failed");
    }
}

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

} // namespace utils
