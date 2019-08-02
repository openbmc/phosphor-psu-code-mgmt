#pragma once

#include <sdbusplus/bus.hpp>
#include <string>
#include <vector>

namespace utils
{

/**
 * @brief Get PSU inventory object path from DBus
 */
std::vector<std::string> getPSUInventoryPath(sdbusplus::bus::bus& bus);

} // namespace utils
