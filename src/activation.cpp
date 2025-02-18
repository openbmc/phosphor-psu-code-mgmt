#include "config.h"

#include "activation.hpp"

#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <vector>

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

using namespace phosphor::logging;
using SoftwareActivation =
    sdbusplus::server::xyz::openbmc_project::software::Activation;
using ExtendedVersion =
    sdbusplus::server::xyz::openbmc_project::software::ExtendedVersion;

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
    if (value == RequestedActivations::Active)
    {
        if (SoftwareActivation::requestedActivation() !=
            RequestedActivations::Active)
        {
            // PSU image could be activated even when it's in active,
            // e.g. in case a PSU is replaced and has a older image, it will be
            // updated with the running PSU image that is stored in BMC.
            if ((activation() == Status::Ready) ||
                (activation() == Status::Failed) ||
                (activation() == Status::Active))
            {
                if (activation(Status::Activating) != Status::Activating)
                {
                    // Activation attempt failed
                    value = RequestedActivations::None;
                }
            }
        }
        else if (activation() == Status::Activating)
        {
            // Activation was requested when one was already in progress.
            // Activate again once the current activation is done. New PSU
            // information may have been found on D-Bus, or a new PSU may have
            // been plugged in.
            shouldActivateAgain = true;
        }
    }
    return SoftwareActivation::requestedActivation(value);
}

auto Activation::extendedVersion(std::string value) -> std::string
{
    auto info = Version::getExtVersionInfo(value);
    manufacturer = info["manufacturer"];
    model = info["model"];

    return ExtendedVersion::extendedVersion(value);
}

void Activation::unitStateChange(sdbusplus::message_t& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    try
    {
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
    catch (const std::exception& e)
    {
        lg2::error("Unable to handle unit state change event: {ERROR}", "ERROR",
                   e);
    }
}

bool Activation::doUpdate(const std::string& psuInventoryPath)
{
    currentUpdatingPsu = psuInventoryPath;
    try
    {
        psuUpdateUnit = getUpdateService(currentUpdatingPsu);
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(psuUpdateUnit, "replace");
        bus.call_noreply(method);
        return true;
    }
    catch (const std::exception& e)
    {
        lg2::error("Error starting update service for PSU {PSU}: {ERROR}",
                   "PSU", psuInventoryPath, "ERROR", e);
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
    assocs.emplace_back(ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
                        currentUpdatingPsu);
    associations(assocs);

    activationListener->onUpdateDone(versionId, currentUpdatingPsu);
    currentUpdatingPsu.clear();

    psuQueue.pop();
    doUpdate(); // Update the next psu
}

void Activation::onUpdateFailed()
{
    // TODO: report an event
    lg2::error("Failed to update PSU {PSU}", "PSU", psuQueue.front());
    std::queue<std::string>().swap(psuQueue); // Clear the queue
    activation(Status::Failed);
    requestedActivation(RequestedActivations::None);
    shouldActivateAgain = false;
}

Activation::Status Activation::startActivation()
{
    // Check if the activation has file path
    if (path().empty())
    {
        lg2::warning(
            "No image for the activation, skipped version {VERSION_ID}",
            "VERSION_ID", versionId);
        return activation(); // Return the previous activation status
    }

    auto psuPaths = utils::getPSUInventoryPaths(bus);
    if (psuPaths.empty())
    {
        lg2::warning("No PSU inventory found");
        return Status::Failed;
    }

    for (const auto& p : psuPaths)
    {
        if (!isPresent(p))
        {
            continue;
        }
        if (isCompatible(p))
        {
            if (utils::isAssociated(p, associations()))
            {
                lg2::notice("PSU {PSU} is already running the image, skipping",
                            "PSU", p);
                continue;
            }
            psuQueue.push(p);
        }
        else
        {
            lg2::notice("PSU {PSU} is not compatible", "PSU", p);
        }
    }

    if (psuQueue.empty())
    {
        lg2::warning("No PSU compatible with the software");
        return activation(); // Return the previous activation status
    }

    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(bus, objPath);
    }
    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
            std::make_unique<ActivationBlocksTransition>(bus, objPath);
    }

    // The progress to be increased for each successful update of PSU
    // E.g. in case we have 4 PSUs:
    //   1. Initial progress is 10
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
    storeImage();
    activationProgress->progress(100);

    deleteImageManagerObject();

    associationInterface->createActiveAssociation(objPath);
    associationInterface->addFunctionalAssociation(objPath);
    associationInterface->addUpdateableAssociation(objPath);

    // Reset RequestedActivations to none so that it could be activated in
    // future
    requestedActivation(RequestedActivations::None);
    activation(Status::Active);

    // Automatically restart activation if a request occurred while code update
    // was already in progress. New PSU information may have been found on
    // D-Bus, or a new PSU may have been plugged in.
    if (shouldActivateAgain)
    {
        shouldActivateAgain = false;
        requestedActivation(RequestedActivations::Active);
    }
}

void Activation::deleteImageManagerObject()
{
    // Get the Delete object for <versionID> inside image_manager
    std::vector<std::string> services;
    constexpr auto deleteInterface = "xyz.openbmc_project.Object.Delete";
    try
    {
        services = utils::getServices(bus, objPath.c_str(), deleteInterface);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Unable to find services to Delete object path {PATH}: {ERROR}",
            "PATH", objPath, "ERROR", e);
    }

    // We need to find the phosphor-version-software-manager's version service
    // to invoke the delete interface
    constexpr auto versionServiceStr = "xyz.openbmc_project.Software.Version";
    std::string versionService;
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
        // When updating a stored image, there is no version object created by
        // "xyz.openbmc_project.Software.Version" service, so skip it.
        return;
    }

    // Call the Delete object for <versionID> inside image_manager
    try
    {
        auto method = bus.new_method_call(
            versionService.c_str(), objPath.c_str(), deleteInterface, "Delete");
        bus.call(method);
    }
    catch (const std::exception& e)
    {
        lg2::error("Unable to Delete object path {PATH}: {ERROR}", "PATH",
                   objPath, "ERROR", e);
    }
}

bool Activation::isPresent(const std::string& psuInventoryPath)
{
    bool isPres{false};
    try
    {
        auto service =
            utils::getService(bus, psuInventoryPath.c_str(), ITEM_IFACE);
        isPres = utils::getProperty<bool>(bus, service.c_str(),
                                          psuInventoryPath.c_str(), ITEM_IFACE,
                                          PRESENT);
    }
    catch (const std::exception& e)
    {
        // Treat as a warning condition and assume the PSU is missing.  The
        // D-Bus information might not be available if the PSU is missing.
        lg2::warning("Unable to determine if PSU {PSU} is present: {ERROR}",
                     "PSU", psuInventoryPath, "ERROR", e);
    }
    return isPres;
}

bool Activation::isCompatible(const std::string& psuInventoryPath)
{
    bool isCompat{false};
    try
    {
        auto service =
            utils::getService(bus, psuInventoryPath.c_str(), ASSET_IFACE);
        auto psuManufacturer = utils::getProperty<std::string>(
            bus, service.c_str(), psuInventoryPath.c_str(), ASSET_IFACE,
            MANUFACTURER);
        auto psuModel = utils::getModel(psuInventoryPath);
        // The model shall match
        if (psuModel == model)
        {
            // If PSU inventory has manufacturer property, it shall match
            if (psuManufacturer.empty() || (psuManufacturer == manufacturer))
            {
                isCompat = true;
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Unable to determine if PSU {PSU} is compatible with firmware "
            "versionId {VERSION_ID}: {ERROR}",
            "PSU", psuInventoryPath, "VERSION_ID", versionId, "ERROR", e);
    }
    return isCompat;
}

void Activation::storeImage()
{
    // If image is not in IMG_DIR (temporary storage) then exit.  We don't want
    // to copy from IMG_DIR_PERSIST or IMG_DIR_BUILTIN.
    auto src = path();
    if (!src.starts_with(IMG_DIR))
    {
        return;
    }

    // Store image in persistent dir separated by model
    // and only store the latest one by removing old ones
    auto dst = fs::path(IMG_DIR_PERSIST) / model;
    try
    {
        fs::remove_all(dst);
        fs::create_directories(dst);
        fs::copy(src, dst);
        path(dst.string()); // Update the FilePath interface
    }
    catch (const fs::filesystem_error& e)
    {
        lg2::error("Error storing PSU image: src={SRC}, dst={DST}: {ERROR}",
                   "SRC", src, "DST", dst, "ERROR", e);
    }
}

std::string Activation::getUpdateService(const std::string& psuInventoryPath)
{
    fs::path imagePath(path());

    // The systemd unit shall be escaped
    std::string args = psuInventoryPath;
    args += "\\x20";
    args += imagePath;
    std::replace(args.begin(), args.end(), '/', '-');

    std::string service = PSU_UPDATE_SERVICE;
    auto p = service.find('@');
    if (p == std::string::npos)
    {
        throw std::runtime_error{std::format(
            "Invalid PSU update service name: {}", PSU_UPDATE_SERVICE)};
    }
    service.insert(p + 1, args);
    return service;
}

void ActivationBlocksTransition::enableRebootGuard()
{
    lg2::info("PSU image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply_noerror(method);
}

void ActivationBlocksTransition::disableRebootGuard()
{
    lg2::info("PSU activation has ended - BMC reboots are re-enabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-disable.service", "replace");
    bus.call_noreply_noerror(method);
}

} // namespace updater
} // namespace software
} // namespace phosphor
