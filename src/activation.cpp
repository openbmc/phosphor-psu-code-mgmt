#include "config.h"

#include "activation.hpp"

#include "utils.hpp"

#include <cassert>
#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

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

using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;
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
        value = startActivation();
    }
    else
    {
        activationBlocksTransition.reset();
        activationProgress.reset();
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
            onUpdateDone();
        }
        if (newStateResult == "failed" || newStateResult == "dependency")
        {
            onUpdateFailed();
        }
    }
}

bool Activation::doUpdate(const std::string& psuInventoryPath)
{
    currentUpdatingPsu = psuInventoryPath;
    psuUpdateUnit = internal::getUpdateService(currentUpdatingPsu, versionId);
    try
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(psuUpdateUnit, "replace");
        bus.call_noreply(method);
        return true;
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error staring service", entry("ERROR=%s", e.what()));
        onUpdateFailed();
        return false;
    }
}

bool Activation::doUpdate()
{
    // When the queue is empty, all updates are done
    if (psuQueue.empty())
    {
        finishActivation();
        return true;
    }

    // Do the update on a PSU
    const auto& psu = psuQueue.front();
    return doUpdate(psu);
}

void Activation::onUpdateDone()
{
    auto progress = activationProgress->progress() + progressStep;
    activationProgress->progress(progress);

    // Update the activation association
    auto assocs = associations();
    assocs.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                        ACTIVATION_REV_ASSOCIATION,
                                        currentUpdatingPsu));
    currentUpdatingPsu.clear();
    associations(assocs);

    psuQueue.pop();
    doUpdate(); // Update the next psu
}

void Activation::onUpdateFailed()
{
    log<level::ERR>("Failed to udpate PSU",
                    entry("PSU=%s", psuQueue.front().c_str()));
    std::queue<std::string>().swap(psuQueue); // Clear the queue
    activation(Status::Failed);
}

Activation::Status Activation::startActivation()
{
    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(bus, path);
    }
    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
            std::make_unique<ActivationBlocksTransition>(bus, path);
    }

    auto psuPaths = utils::getPSUInventoryPath(bus);
    if (psuPaths.empty())
    {
        log<level::WARNING>("No PSU inventory found");
        return Status::Failed;
    }

    for (const auto& p : psuPaths)
    {
        if (isCompatible(p))
        {
            psuQueue.push(p);
        }
        else
        {
            log<level::NOTICE>("PSU not compatible",
                               entry("PSU=%s", p.c_str()));
        }
    }

    if (psuQueue.empty())
    {
        log<level::ERR>("No PSU compatible with the software");
        return Status::Failed;
    }

    // The progress to be increased for each successful update of PSU
    // E.g. in case we have 4 PSUs:
    //   1. Initial progrss is 10
    //   2. Add 20 after each update is done, so we will see progress to be 30,
    //      50, 70, 90
    //   3. When all PSUs are updated, it will be 100 and the interface is
    //   removed.
    progressStep = 80 / psuQueue.size();
    if (doUpdate())
    {
        activationProgress->progress(10);
        return Status::Activating;
    }
    else
    {
        return Status::Failed;
    }
}

void Activation::finishActivation()
{
    activationProgress->progress(100);

    // TODO: delete the old software object
    deleteImageManagerObject();

    associationInterface->createActiveAssociation(path);
    associationInterface->addFunctionalAssociation(path);

    activation(Status::Active);
}

void Activation::deleteImageManagerObject()
{
    // Get the Delete object for <versionID> inside image_manager
    constexpr auto versionServiceStr = "xyz.openbmc_project.Software.Version";
    constexpr auto deleteInterface = "xyz.openbmc_project.Object.Delete";
    std::string versionService;
    auto services = utils::getServices(bus, path.c_str(), deleteInterface);

    // We need to find the phosphor-version-software-manager's version service
    // to invoke the delete interface
    for (const auto& service : services)
    {
        if (service.find(versionServiceStr) != std::string::npos)
        {
            versionService = service;
            break;
        }
    }
    if (versionService.empty())
    {
        log<level::ERR>("Error finding version service");
        return;
    }

    // Call the Delete object for <versionID> inside image_manager
    auto method = bus.new_method_call(versionService.c_str(), path.c_str(),
                                      deleteInterface, "Delete");
    try
    {
        bus.call(method);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error performing call to Delete object path",
                        entry("ERROR=%s", e.what()),
                        entry("PATH=%s", path.c_str()));
    }
}

bool Activation::isCompatible(const std::string& psuInventoryPath)
{
    auto service =
        utils::getService(bus, psuInventoryPath.c_str(), ASSET_IFACE);
    auto psuManufacturer = utils::getProperty<std::string>(
        bus, service.c_str(), psuInventoryPath.c_str(), ASSET_IFACE,
        MANUFACTURER);
    auto psuModel = utils::getProperty<std::string>(
        bus, service.c_str(), psuInventoryPath.c_str(), ASSET_IFACE, MODEL);
    if (psuModel != model)
    {
        // The model shall match
        return false;
    }
    if (!psuManufacturer.empty())
    {
        // If PSU inventory has manufacturer property, it shall match
        return psuManufacturer == manufacturer;
    }
    return true;
}

std::map<std::string, std::string>
    Activation::getExtVersionInfo(const std::string& extVersion)
{
    // The extVersion shall be key/value pairs separated by comma,
    // e.g. key1=value1,key2=value2
    std::map<std::string, std::string> result;
    std::stringstream ss(extVersion);

    while (ss.good())
    {
        std::string substr;
        getline(ss, substr, ',');
        auto pos = substr.find('=');
        if (pos != std::string::npos)
        {
            std::string key = substr.substr(0, pos);
            std::string value = substr.substr(pos + 1);
            result.emplace(key, value);
        }
    }
    return result;
}

} // namespace updater
} // namespace software
} // namespace phosphor
