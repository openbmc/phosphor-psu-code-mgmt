#include "config.h"

#include "item_updater.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

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

void ItemUpdater::createActivation(sdbusplus::message::message& m)
{
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
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
                else if (propertyName == "Version")
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
        auto activationState = server::Activation::Activations::Invalid;
        activationState = server::Activation::Activations::Ready;

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        std::string extendedVersion =
            (Version::getValue(
                 manifestPath.string(),
                 std::map<std::string, std::string>{{"extended_version", ""}}))
                .begin()
                ->second;

        auto activation = createActivationObject(
            path, versionId, extendedVersion, activationState);
        activations.emplace(versionId, std::move(activation));

        auto versionPtr =
            createVersionObject(path, versionId, version, purpose, filePath);
        versions.emplace(versionId, std::move(versionPtr));
    }
    return;
}

void ItemUpdater::erase(std::string entryId)
{
    auto it = versions.find(entryId);
    if (it == versions.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId +
                         " in item updater versions map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        versions.erase(entryId);
    }

    // Removing entry in activations map
    auto ita = activations.find(entryId);
    if (ita == activations.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId +
                         " in item updater activations map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        activations.erase(entryId);
    }
}

void ItemUpdater::deleteAll()
{
    // TODO: when PSU's running firmware is implemented, delete all versions
    // that are not the running firmware.
}

std::unique_ptr<Activation> ItemUpdater::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations
        activationStatus)
{
    return std::make_unique<Activation>(bus, path, versionId, extVersion,
                                        activationStatus);
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

} // namespace updater
} // namespace software
} // namespace phosphor
