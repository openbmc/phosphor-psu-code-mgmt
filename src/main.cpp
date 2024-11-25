#include "config.h"

#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

#include <exception>

int main(int /* argc */, char* /* argv */[])
{
    int rc{0};
    try
    {
        auto bus = sdbusplus::bus::new_default();

        // Add sdbusplus ObjectManager.
        sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);

        phosphor::software::updater::ItemUpdater updater(bus, SOFTWARE_OBJPATH);

        bus.request_name(BUSNAME_UPDATER);

        while (true)
        {
            bus.process_discard();
            bus.wait();
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error: {ERROR}", "ERROR", e);
        rc = 1;
    }
    catch (...)
    {
        lg2::error("Caught unexpected exception type");
        rc = 1;
    }

    return rc;
}
