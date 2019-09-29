#include "config.h"

#include "item_updater.hpp"

#include "utils.hpp"

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace
{
constexpr auto EXTENDED_VERSION = "extended_version";
}

namespace phosphor
{
namespace software
{
namespace updater
{
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
using SVersion = server::Version;
using VersionPurpose = SVersion::VersionPurpose;

void ItemUpdater::createActivation(sdbusplus::message::message& m)
{
    namespace msg = sdbusplus::message;
    namespace variant_ns = msg::variant_ns;

    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, msg::variant<std::string>>>
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
                        variant_ns::get<std::string>(propertyValue));

                    if (value == VersionPurpose::PSU)
                    {
                        purpose = value;
                    }
                }
                else if (propertyName == VERSION)
                {
                    version = variant_ns::get<std::string>(propertyValue);
                }
            }
        }
        else if (interfaceName == FILEPATH_IFACE)
        {
            const auto& it = propertyMap.find("Path");
            if (it != propertyMap.end())
            {
                filePath = variant_ns::get<std::string>(it->second);
            }
        }
    }
    if ((filePath.empty()) || (purpose == VersionPurpose::Unknown))
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
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
        auto activationState = server::Activation::Activations::Ready;

        associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  PSU_INVENTORY_PATH_BASE));

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        std::string extendedVersion;
        auto values =
            Version::getValues(manifestPath.string(), {EXTENDED_VERSION});
        const auto it = values.find(EXTENDED_VERSION);
        if (it != values.end())
        {
            extendedVersion = it->second;
        }

        auto activation = createActivationObject(
            path, versionId, extendedVersion, activationState, associations);
        activations.emplace(versionId, std::move(activation));

        auto versionPtr =
            createVersionObject(path, versionId, version, purpose, filePath);
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
        versions.erase(versionId);
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

void ItemUpdater::removeAssociation(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<2>(*iter)).compare(path) == 0)
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

std::unique_ptr<Activation> ItemUpdater::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations
        activationStatus,
    const AssociationList& assocs)
{
    return std::make_unique<Activation>(bus, path, versionId, extVersion,
                                        activationStatus, assocs, this);
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
        associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  psuInventoryPath));
        it->second->associations(associations);
        psuPathActivationMap.emplace(psuInventoryPath, it->second);
    }
    else
    {
        // Create a new object for running PSU inventory
        AssociationList associations;
        auto activationState = server::Activation::Activations::Active;

        associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  psuInventoryPath));

        auto activation = createActivationObject(path, versionId, "",
                                                 activationState, associations);
        activations.emplace(versionId, std::move(activation));
        psuPathActivationMap.emplace(psuInventoryPath, activations[versionId]);

        auto versionPtr = createVersionObject(path, versionId, psuVersion,
                                              VersionPurpose::PSU, "");
        versions.emplace(versionId, std::move(versionPtr));

        createActiveAssociation(path);
        addFunctionalAssociation(path);
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
        if ((std::get<2>(*iter)).compare(psuInventoryPath) == 0)
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

std::unique_ptr<Version> ItemUpdater::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose,
    const std::string& filePath)
{
    auto version = std::make_unique<Version>(
        bus, objPath, versionId, versionString, versionPurpose, filePath,
        std::bind(&ItemUpdater::erase, this, std::placeholders::_1));
    return version;
}

void ItemUpdater::onPsuInventoryChangedMsg(sdbusplus::message::message& msg)
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
    bool present;
    std::string version;

    // Only present property is interested
    auto p = properties.find(PRESENT);
    if (p == properties.end())
    {
        return;
    }
    present = sdbusplus::message::variant_ns::get<bool>(p->second);

    if (present)
    {
        version = utils::getVersion(psuPath);
        if (!version.empty())
        {
            createPsuObject(psuPath, version);
        }
    }
    else
    {
        // Remove object or association
        removePsuObject(psuPath);
    }
}

void ItemUpdater::processPSUImage()
{
    auto paths = utils::getPSUInventoryPath(bus);
    for (const auto& p : paths)
    {
        auto service = utils::getService(bus, p.c_str(), ITEM_IFACE);
        auto present = utils::getProperty<bool>(bus, service.c_str(), p.c_str(),
                                                ITEM_IFACE, PRESENT);
        auto version = utils::getVersion(p);
        if (present && !version.empty())
        {
            createPsuObject(p, version);
        }
        // Add matches for PSU Inventory's property changes
        psuMatches.emplace_back(
            bus, MatchRules::propertiesChanged(p, ITEM_IFACE),
            std::bind(&ItemUpdater::onPsuInventoryChangedMsg, this,
                      std::placeholders::_1));
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
