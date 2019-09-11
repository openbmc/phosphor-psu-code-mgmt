#include "config.h"

#include "activation.hpp"

#include "utils.hpp"

#include <cassert>
#include <filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{

constexpr auto SYSTEMD_BUSNAME = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_PATH = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";

namespace fs = std::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using SoftwareActivation = softwareServer::Activation;

namespace internal
{
/** Construct the systemd service name */
std::string getUpdateService(const std::string& psuInventoryPath,
                             const std::string& versionId)
{
    fs::path imagePath(IMG_DIR);
    imagePath /= versionId;

    // The systemd unit shall be escaped
    std::string args = psuInventoryPath;
    args += "\\x20";
    args += imagePath;
    std::replace(args.begin(), args.end(), '/', '-');

    std::string service = PSU_UPDATE_SERVICE;
    auto p = service.find('@');
    assert(p != std::string::npos);
    service.insert(p + 1, args);
    return service;
}

} // namespace internal
auto Activation::activation(Activations value) -> Activations
{
    if (value == Status::Activating)
    {
        startActivation();
    }
    else
    {
        // TODO
    }

    return SoftwareActivation::activation(value);
}

auto Activation::requestedActivation(RequestedActivations value)
    -> RequestedActivations
{
    if ((value == SoftwareActivation::RequestedActivations::Active) &&
        (SoftwareActivation::requestedActivation() !=
         SoftwareActivation::RequestedActivations::Active))
    {
        if ((activation() == Status::Ready) || (activation() == Status::Failed))
        {
            activation(Status::Activating);
        }
    }
    return SoftwareActivation::requestedActivation(value);
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    if (newStateUnit == psuUpdateUnit)
    {
        if (newStateResult == "done")
        {
            finishActivation();
        }
        if (newStateResult == "failed" || newStateResult == "dependency")
        {
            activation(Status::Failed);
        }
    }
}

void Activation::startActivation()
{
    // TODO: for now only update one psu, future commits shall handle update
    // multiple psus
    auto psuPaths = utils::getPSUInventoryPath(bus);
    if (psuPaths.empty())
    {
        return;
    }

    psuUpdateUnit = internal::getUpdateService(psuPaths[0], versionId);

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(psuUpdateUnit, "replace");
    bus.call_noreply(method);
}

void Activation::finishActivation()
{
    // TODO: delete the interfaces created by phosphor-software-manager
    // TODO: delete the old software object
    // TODO: create related associations
    activation(Status::Active);
}

} // namespace updater
} // namespace software
} // namespace phosphor
