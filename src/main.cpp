#include "config.h"

#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

#include <chrono>
#include <system_error>
#include <thread>
#include <vector>

constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
constexpr auto TIMEOUT = 30;

void waitForPSUsPath(sdbusplus::bus_t& bus);
void waitForPSUsPath(sdbusplus::bus_t& bus)
{
    std::vector<std::string> paths;
    std::vector<std::string> previousPaths;
    auto timeout = std::chrono::steady_clock::now() +
                   std::chrono::seconds(TIMEOUT);

    while (std::chrono::steady_clock::now() < timeout)
    {
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(PSU_INVENTORY_PATH_BASE);
        method.append(0); // Depth 0 to search all

        method.append(std::vector<std::string>({PSU_INVENTORY_IFACE}));
        //
        // Since we don't know how many PSUs in the system will wait 30 seconds
        // max to get new PSU path. The timer gets reset everytime new PSU path
        // discovered.
        //
        try
        {
            auto reply = bus.call(method);
            reply.read(paths);
        }
        catch (const std::exception& ex)
        {
            lg2::error("bus call execption ex={ERROR}", "ERROR", ex);
        }
        for (const auto& newPath : paths)
        {
            if (std::find(previousPaths.begin(), previousPaths.end(),
                          newPath) != previousPaths.end())
            {
                // Old PSU path found
                continue;
            }
            else
            {
                previousPaths.push_back(std::move(newPath)); // New PSU Path
                timeout = std::chrono::steady_clock::now() +
                          std::chrono::seconds(TIMEOUT);     // Reset timer
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

    //   WaitUtil waitUtil;

    //  waitUtil.waitForPSUsPath(bus);
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
