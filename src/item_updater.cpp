#include "config.h"

#include "item_updater.hpp"

#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cassert>
#include <filesystem>

namespace
{
constexpr auto MANIFEST_VERSION = "version";
constexpr auto MANIFEST_EXTENDED_VERSION = "extended_version";
constexpr auto TIMEOUT = 10;
} // namespace

namespace phosphor
{
namespace software
{
namespace updater
{
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
using SVersion = server::Version;
using VersionPurpose = SVersion::VersionPurpose;

void ItemUpdater::createActivation(sdbusplus::message_t& m)
{
    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, std::variant<std::string>>>
        interfaces;
    m.read(objPath, interfaces);

    std::string path(std::move(objPath));
    std::string filePath;
    auto purpose = VersionPurpose::Unknown;
    std::string version;

    for (const auto& [interfaceName, propertyMap] : interfaces)
    {
        if (interfaceName == VERSION_IFACE)
        {
            for (const auto& [propertyName, propertyValue] : propertyMap)
            {
                if (propertyName == "Purpose")
                {
                    // Only process the PSU images
                    auto value = SVersion::convertVersionPurposeFromString(
                        std::get<std::string>(propertyValue));

                    if (value == VersionPurpose::PSU)
                    {
                        purpose = value;
                    }
                }
                else if (propertyName == VERSION)
                {
                    version = std::get<std::string>(propertyValue);
                }
            }
        }
        else if (interfaceName == FILEPATH_IFACE)
        {
            const auto& it = propertyMap.find("Path");
            if (it != propertyMap.end())
            {
                filePath = std::get<std::string>(it->second);
            }
        }
    }
    if ((filePath.empty()) || (purpose == VersionPurpose::Unknown))
    {
        return;
    }

    // If we are only installing PSU images from the built-in directory, ignore
    // PSU images from other directories
    if (ALWAYS_USE_BUILTIN_IMG_DIR && !filePath.starts_with(IMG_DIR_BUILTIN))
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind('/');
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path.c_str()));
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        // Determine the Activation state by processing the given image dir.
        AssociationList associations;
        auto activationState = Activation::Status::Ready;

        associations.emplace_back(std::make_tuple(
            ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
            PSU_INVENTORY_PATH_BASE));

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        std::string extendedVersion =
            Version::getValue(manifestPath, {MANIFEST_EXTENDED_VERSION});

        auto activation =
            createActivationObject(path, versionId, extendedVersion,
                                   activationState, associations, filePath);
        activations.emplace(versionId, std::move(activation));

        auto versionPtr =
            createVersionObject(path, versionId, version, purpose);
        versions.emplace(versionId, std::move(versionPtr));
    }
    return;
}

void ItemUpdater::erase(const std::string& versionId)
{
    auto it = versions.find(versionId);
    if (it == versions.end())
    {
        log<level::ERR>(("Error: Failed to find version " + versionId +
                         " in item updater versions map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        versionStrings.erase(it->second->getVersionString());
        versions.erase(it);
    }

    // Removing entry in activations map
    auto ita = activations.find(versionId);
    if (ita == activations.end())
    {
        log<level::ERR>(("Error: Failed to find version " + versionId +
                         " in item updater activations map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        activations.erase(versionId);
    }
}

void ItemUpdater::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::addFunctionalAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::addUpdateableAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(UPDATEABLE_FWD_ASSOCIATION,
                                        UPDATEABLE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::removeAssociation(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<2>(*iter)) == path)
        {
            iter = assocs.erase(iter);
            associations(assocs);
        }
        else
        {
            ++iter;
        }
    }
}

void ItemUpdater::onUpdateDone(const std::string& versionId,
                               const std::string& psuInventoryPath)
{
    // After update is done, remove old activation objects
    for (auto it = activations.begin(); it != activations.end(); ++it)
    {
        if (it->second->getVersionId() != versionId &&
            utils::isAssociated(psuInventoryPath, it->second->associations()))
        {
            removePsuObject(psuInventoryPath);
            break;
        }
    }

    auto it = activations.find(versionId);
    assert(it != activations.end());
    psuPathActivationMap.emplace(psuInventoryPath, it->second);
}

std::unique_ptr<Activation> ItemUpdater::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion, Activation::Status activationStatus,
    const AssociationList& assocs, const std::string& filePath)
{
    return std::make_unique<Activation>(bus, path, versionId, extVersion,
                                        activationStatus, assocs, filePath,
                                        this, this);
}

void ItemUpdater::createPsuObject(const std::string& psuInventoryPath,
                                  const std::string& psuVersion)
{
    auto versionId = utils::getVersionId(psuVersion);
    auto path = std::string(SOFTWARE_OBJPATH) + "/" + versionId;

    auto it = activations.find(versionId);
    if (it != activations.end())
    {
        // The versionId is already created, associate the path
        auto associations = it->second->associations();
        associations.emplace_back(
            std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                            ACTIVATION_REV_ASSOCIATION, psuInventoryPath));
        it->second->associations(associations);
        psuPathActivationMap.emplace(psuInventoryPath, it->second);
    }
    else
    {
        // Create a new object for running PSU inventory
        AssociationList associations;
        auto activationState = Activation::Status::Active;

        associations.emplace_back(
            std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                            ACTIVATION_REV_ASSOCIATION, psuInventoryPath));

        auto activation = createActivationObject(
            path, versionId, "", activationState, associations, "");
        activations.emplace(versionId, std::move(activation));
        psuPathActivationMap.emplace(psuInventoryPath, activations[versionId]);

        auto versionPtr = createVersionObject(path, versionId, psuVersion,
                                              VersionPurpose::PSU);
        versions.emplace(versionId, std::move(versionPtr));

        createActiveAssociation(path);
        addFunctionalAssociation(path);
        addUpdateableAssociation(path);
    }
}

void ItemUpdater::removePsuObject(const std::string& psuInventoryPath)
{
    auto it = psuPathActivationMap.find(psuInventoryPath);
    if (it == psuPathActivationMap.end())
    {
        log<level::ERR>("No Activation found for PSU",
                        entry("PSUPATH=%s", psuInventoryPath.c_str()));
        return;
    }
    const auto& activationPtr = it->second;
    psuPathActivationMap.erase(psuInventoryPath);

    auto associations = activationPtr->associations();
    for (auto iter = associations.begin(); iter != associations.end();)
    {
        if ((std::get<2>(*iter)) == psuInventoryPath)
        {
            iter = associations.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    if (associations.empty())
    {
        // Remove the activation
        erase(activationPtr->getVersionId());
    }
    else
    {
        // Update association
        activationPtr->associations(associations);
    }
}

void ItemUpdater::addPsuToStatusMap(const std::string& psuPath)
{
    if (!psuStatusMap.contains(psuPath))
    {
        psuStatusMap[psuPath] = {false, ""};

        // Add matches for PSU Inventory's property changes
        psuMatches.emplace_back(
            bus, MatchRules::propertiesChanged(psuPath, ITEM_IFACE),
            std::bind(&ItemUpdater::onPsuInventoryChangedMsg, this,
                      std::placeholders::_1)); // For present
        psuMatches.emplace_back(
            bus, MatchRules::propertiesChanged(psuPath, ASSET_IFACE),
            std::bind(&ItemUpdater::onPsuInventoryChangedMsg, this,
                      std::placeholders::_1)); // For model
    }
}

std::unique_ptr<Version> ItemUpdater::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose)
{
    versionStrings.insert(versionString);
    auto version = std::make_unique<Version>(
        bus, objPath, versionId, versionString, versionPurpose,
        std::bind(&ItemUpdater::erase, this, std::placeholders::_1));
    return version;
}

void ItemUpdater::onPsuInventoryChangedMsg(sdbusplus::message_t& msg)
{
    using Interface = std::string;
    Interface interface;
    Properties properties;
    std::string psuPath = msg.get_path();

    msg.read(interface, properties);
    onPsuInventoryChanged(psuPath, properties);
}

void ItemUpdater::onPsuInventoryChanged(const std::string& psuPath,
                                        const Properties& properties)
{
    std::optional<bool> present;
    std::optional<std::string> model;

    // The code was expecting to get callback on multiple properties changed.
    // But in practice, the callback is received one-by-one for each property.
    // So it has to handle Present and Version property separately.
    auto p = properties.find(PRESENT);
    if (p != properties.end())
    {
        present = std::get<bool>(p->second);
        psuStatusMap[psuPath].present = *present;
    }
    p = properties.find(MODEL);
    if (p != properties.end())
    {
        model = std::get<std::string>(p->second);
        psuStatusMap[psuPath].model = *model;
    }

    // If present or model is not changed, ignore
    if (!present.has_value() && !model.has_value())
    {
        return;
    }

    if (psuStatusMap[psuPath].present)
    {
        // If model is not updated, let's wait for it
        if (psuStatusMap[psuPath].model.empty())
        {
            log<level::DEBUG>("Waiting for model to be updated");
            return;
        }

        auto version = utils::getVersion(psuPath);
        if (!version.empty())
        {
            if (psuPathActivationMap.find(psuPath) ==
                psuPathActivationMap.end())
            {
                createPsuObject(psuPath, version);
            }
        }
        else
        {
            // TODO: log an event
            log<level::ERR>("Failed to get PSU version",
                            entry("PSU=%s", psuPath.c_str()));
        }
        // Check if there are new PSU images to update
        processStoredImage();
        syncToLatestImage();
    }
    else
    {
        if (!present.has_value())
        {
            // If a PSU is plugged out, model property is update to empty as
            // well, and we get callback here, but ignore that because it is
            // handled by "Present" callback.
            return;
        }
        psuStatusMap[psuPath].model = "";

        // Remove object or association
        removePsuObject(psuPath);
    }
}

void ItemUpdater::processPSUImage()
{
    auto paths = utils::getPSUInventoryPath(bus);
    for (const auto& p : paths)
    {
        addPsuToStatusMap(p);

        auto service = utils::getService(bus, p.c_str(), ITEM_IFACE);
        psuStatusMap[p].present = utils::getProperty<bool>(
            bus, service.c_str(), p.c_str(), ITEM_IFACE, PRESENT);
        psuStatusMap[p].model = utils::getProperty<std::string>(
            bus, service.c_str(), p.c_str(), ASSET_IFACE, MODEL);
        auto version = utils::getVersion(p);
        if ((psuPathActivationMap.find(p) == psuPathActivationMap.end()) &&
            psuStatusMap[p].present && !version.empty())
        {
            createPsuObject(p, version);
        }
    }
}

void ItemUpdater::processStoredImage()
{
    scanDirectory(IMG_DIR_BUILTIN);

    if (!ALWAYS_USE_BUILTIN_IMG_DIR)
    {
        scanDirectory(IMG_DIR_PERSIST);
    }
}

void ItemUpdater::scanDirectory(const fs::path& dir)
{
    auto manifest = dir;
    auto path = dir;
    // The directory shall put PSU images in directories named with model
    if (!fs::exists(dir))
    {
        // Skip
        return;
    }
    if (!fs::is_directory(dir))
    {
        log<level::ERR>("The path is not a directory",
                        entry("PATH=%s", dir.c_str()));
        return;
    }

    for (const auto& [key, item] : psuStatusMap)
    {
        if (!item.model.empty())
        {
            path = path / item.model;
            manifest = dir / item.model / MANIFEST_FILE;
            break;
        }
    }
    if (path == dir)
    {
        log<level::ERR>("Model directory not found");
        return;
    }

    if (!fs::is_directory(path))
    {
        log<level::ERR>("The path is not a directory",
                        entry("PATH=%s", path.c_str()));
        return;
    }

    if (!fs::exists(manifest))
    {
        log<level::ERR>("No MANIFEST found",
                        entry("PATH=%s", manifest.c_str()));
        return;
    }
    // If the model in manifest does not match the dir name
    // Log a warning
    if (fs::is_regular_file(manifest))
    {
        auto ret = Version::getValues(
            manifest.string(), {MANIFEST_VERSION, MANIFEST_EXTENDED_VERSION});
        auto version = ret[MANIFEST_VERSION];
        auto extVersion = ret[MANIFEST_EXTENDED_VERSION];
        auto info = Version::getExtVersionInfo(extVersion);
        auto model = info["model"];
        if (path.stem() != model)
        {
            log<level::ERR>("Unmatched model", entry("PATH=%s", path.c_str()),
                            entry("MODEL=%s", model.c_str()));
        }
        else
        {
            auto versionId = utils::getVersionId(version);
            auto it = activations.find(versionId);
            if (it == activations.end())
            {
                // This is a version that is different than the running PSUs
                auto activationState = Activation::Status::Ready;
                auto purpose = VersionPurpose::PSU;
                auto objPath = std::string(SOFTWARE_OBJPATH) + "/" + versionId;

                auto activation = createActivationObject(
                    objPath, versionId, extVersion, activationState, {}, path);
                activations.emplace(versionId, std::move(activation));

                auto versionPtr =
                    createVersionObject(objPath, versionId, version, purpose);
                versions.emplace(versionId, std::move(versionPtr));
            }
            else
            {
                // This is a version that a running PSU is using, set the path
                // on the version object
                it->second->path(path);
            }
        }
    }
    else
    {
        log<level::ERR>("MANIFEST is not a file",
                        entry("PATH=%s", manifest.c_str()));
    }
}

std::optional<std::string> ItemUpdater::getLatestVersionId()
{
    std::string latestVersion;
    if (ALWAYS_USE_BUILTIN_IMG_DIR)
    {
        latestVersion = getFWVersionFromBuiltinDir();
    }
    else
    {
        latestVersion = utils::getLatestVersion(versionStrings);
    }
    if (latestVersion.empty())
    {
        return {};
    }

    std::optional<std::string> versionId;
    for (const auto& v : versions)
    {
        if (v.second->version() == latestVersion)
        {
            versionId = v.first;
            break;
        }
    }
    assert(versionId.has_value());
    return versionId;
}

void ItemUpdater::syncToLatestImage()
{
    auto latestVersionId = getLatestVersionId();
    if (!latestVersionId)
    {
        return;
    }
    const auto& it = activations.find(*latestVersionId);
    assert(it != activations.end());
    const auto& activation = it->second;
    const auto& assocs = activation->associations();

    auto paths = utils::getPSUInventoryPath(bus);
    for (const auto& p : paths)
    {
        // As long as there is a PSU is not associated with the latest
        // image, run the activation so that all PSUs are running the same
        // latest image.
        if (!utils::isAssociated(p, assocs))
        {
            log<level::INFO>("Automatically update PSU",
                             entry("VERSION_ID=%s", latestVersionId->c_str()));
            invokeActivation(activation);
            break;
        }
    }
}

void ItemUpdater::invokeActivation(
    const std::unique_ptr<Activation>& activation)
{
    activation->requestedActivation(Activation::RequestedActivations::Active);
}

void ItemUpdater::onPSUInterfaceAdded(sdbusplus::message_t& msg)
{
    sdbusplus::message::object_path objPath;
    std::map<std::string,
             std::map<std::string, std::variant<bool, std::string>>>
        interfaces;
    msg.read(objPath, interfaces);
    std::string path = objPath.str;

    if (interfaces.find(PSU_INVENTORY_IFACE) == interfaces.end())
    {
        return;
    }

    addPsuToStatusMap(path);

    if (psuStatusMap[path].present && !psuStatusMap[path].model.empty())
    {
        return;
    }

    auto timeout = std::chrono::steady_clock::now() +
                   std::chrono::seconds(TIMEOUT);

    // Poll the inventory item until it gets the present property
    // or the timeout is reached
    while (std::chrono::steady_clock::now() < timeout)
    {
        try
        {
            psuStatusMap[path].present = utils::getProperty<bool>(
                bus, msg.get_sender(), path.c_str(), ITEM_IFACE, PRESENT);
            break;
        }
        catch (const std::exception& e)
        {
            auto err = errno;
            log<level::INFO>(
                std::format("Failed to get Inventory Item Present. errno={}",
                            err)
                    .c_str());
            sleep(1);
        }
    }

    // Poll the inventory item until it retrieves model or the timeout is
    // reached. The model is the path trail of the firmware's and manifest
    // subdirectory. If the model not found the firmware and manifest
    // cannot be located.
    timeout = std::chrono::steady_clock::now() + std::chrono::seconds(TIMEOUT);
    while (std::chrono::steady_clock::now() < timeout &&
           psuStatusMap[path].present)
    {
        try
        {
            psuStatusMap[path].model = utils::getProperty<std::string>(
                bus, msg.get_sender(), path.c_str(), ASSET_IFACE, MODEL);
            processPSUImageAndSyncToLatest();
            break;
        }
        catch (const std::exception& e)
        {
            auto err = errno;
            log<level::INFO>(
                std::format(
                    "Failed to get Inventory Decorator Asset model. errno={}",
                    err)
                    .c_str());
            sleep(1);
        }
    }
}

void ItemUpdater::processPSUImageAndSyncToLatest()
{
    processPSUImage();
    processStoredImage();
    syncToLatestImage();
}

std::string ItemUpdater::getFWVersionFromBuiltinDir()
{
    std::string version;
    for (const auto& activation : activations)
    {
        if (activation.second->path().starts_with(IMG_DIR_BUILTIN))
        {
            std::string versionId = activation.second->getVersionId();
            auto it = versions.find(versionId);
            if (it != versions.end())
            {
                const auto& versionPtr = it->second;
                version = versionPtr->version();
                break;
            }
        }
    }
    return version;
}

} // namespace updater
} // namespace software
} // namespace phosphor
