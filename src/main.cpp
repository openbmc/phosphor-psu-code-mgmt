#include "config.h"

#include "item_updater.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server/manager.hpp>
#include <xyz/openbmc_project/State/Chassis/server.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

constexpr auto TIMEOUT = 30; // Timeout in seconds
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";

using namespace phosphor::logging;
using sdbusplus::bus::bus;

void interfacesAddedHandler(const std::string& path,
                            std::vector<std::string>& previousPaths,
                            std::chrono::steady_clock::time_point& timeout)
{
    if (std::find(previousPaths.begin(), previousPaths.end(), path) ==
        previousPaths.end())
    {
        // New PSU path found
        previousPaths.push_back(path);
        timeout = std::chrono::steady_clock::now() +
                  std::chrono::seconds(TIMEOUT); // Reset timer
    }
}

void waitForPSUsPath(sdbusplus::bus_t& bus)
{
    std::vector<std::string> paths;
    std::vector<std::string> previousPaths;
    auto timeout = std::chrono::steady_clock::now() +
                   std::chrono::seconds(TIMEOUT);

    // Connect to InterfacesAdded signal
    auto match = sdbusplus::bus::match::match(
        bus,
        "type='signal',interface='org.freedesktop.DBus.ObjectManager',member='InterfacesAdded',path_namespace='" PSU_INVENTORY_PATH_BASE
        "'",
        [&previousPaths, &timeout](sdbusplus::message::message& msg) {
        std::string path;
        std::map<std::string, std::map<std::string, std::variant<std::string>>>
            interfaces;

        msg.read(path, interfaces);

        if (interfaces.find(PSU_INVENTORY_IFACE) != interfaces.end())
        {
            interfacesAddedHandler(path, previousPaths, timeout);
        }
        });

    while (std::chrono::steady_clock::now() < timeout)
    {
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(PSU_INVENTORY_PATH_BASE);
        method.append(0); // Depth 0 to search all
        method.append(std::vector<std::string>({PSU_INVENTORY_IFACE}));

        try
        {
            auto reply = bus.call(method);
            reply.read(paths);
        }
        catch (const std::exception& ex)
        {
            log<level::ERR>(
                std::format("bus call exception ex={}", ex.what()).c_str());
        }

        for (const auto& newPath : paths)
        {
            if (std::find(previousPaths.begin(), previousPaths.end(),
                          newPath) == previousPaths.end())
            {
                previousPaths.push_back(newPath);        // New PSU Path
                timeout = std::chrono::steady_clock::now() +
                          std::chrono::seconds(TIMEOUT); // Reset timer
            }
        }

        paths.clear();
        // Sleep for 3 seconds
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

int main(int /* argc */, char* /* argv */[])
{
    auto bus = sdbusplus::bus::new_default();
    waitForPSUsPath(bus);
    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::updater::ItemUpdater updater(bus, SOFTWARE_OBJPATH);

    bus.request_name(BUSNAME_UPDATER);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }

    return 0;
}
