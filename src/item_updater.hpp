#pragma once

#include "config.h"

#include "activation.hpp"
#include "types.hpp"
#include "version.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

class Version;

using ItemUpdaterInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions,
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll>;
namespace MatchRules = sdbusplus::bus::match::rules;

/** @class ItemUpdater
 *  @brief Manages the activation of the PSU version items.
 */
class ItemUpdater : public ItemUpdaterInherit
{
  public:
    /** @brief Constructs ItemUpdater
     *
     * @param[in] bus    - The D-Bus bus object
     * @param[in] path   - The D-Bus path
     */
    ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
        ItemUpdaterInherit(bus, path.c_str()), bus(bus),
        versionMatch(bus,
                     MatchRules::interfacesAdded() +
                         MatchRules::path(SOFTWARE_OBJPATH),
                     std::bind(std::mem_fn(&ItemUpdater::createActivation),
                               this, std::placeholders::_1))
    {
        processPSUImage();
    }

    /** @brief Deletes version
     *
     *  @param[in] entryId - Id of the version to delete
     */
    void erase(std::string entryId);

    /**
     * @brief Erases any non-active versions.
     */
    void deleteAll();

  private:
    /** @brief Creates an active association to the
     *  newly active software image
     *
     * @param[in]  path - The path to create the association to.
     */
    void createActiveAssociation(const std::string& path);

    /** @brief Add the functional association to the
     *  new "running" PSU images
     *
     * @param[in]  versionId - The id of the image to update the association to.
     */
    void addFunctionalAssociation(const std::string& path);

    /** @brief Removes the associations from the provided software image path
     *
     * @param[in]  path - The path to remove the association from.
     */
    void removeAssociation(const std::string& path);

    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void createActivation(sdbusplus::message::message& msg);

    /** @brief Callback function for PSU inventory match.
     *  @details Update an Activation D-Bus object for PSU inventory.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void onPsuInventoryChanged(sdbusplus::message::message& msg);

    /** @brief Create Activation object */
    std::unique_ptr<Activation> createActivationObject(
        const std::string& path, const std::string& versionId,
        const std::string& extVersion,
        sdbusplus::xyz::openbmc_project::Software::server::Activation::
            Activations activationStatus,
        const AssociationList& assocs);

    /** @brief Create Version object */
    std::unique_ptr<Version>
        createVersionObject(const std::string& objPath,
                            const std::string& versionId,
                            const std::string& versionString,
                            sdbusplus::xyz::openbmc_project::Software::server::
                                Version::VersionPurpose versionPurpose,
                            const std::string& filePath);

    /** @brief Create Activation and Version object for PSU inventory */
    void createPsuObject(const std::string& psuInventoryPath,
                         const std::string& psuVersion);

    /**
     * @brief Create and populate the active PSU Version.
     */
    void processPSUImage();

    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Activation>> activations;

    /** @brief Persistent map of Version D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Version>> versions;

    /** @brief sdbusplus signal match for PSU Software*/
    sdbusplus::bus::match_t versionMatch;

    /** @brief sdbusplus signal matches for PSU Inventory */
    std::vector<sdbusplus::bus::match_t> psuMatches;

    /** @brief This entry's associations */
    AssociationList assocs;
};

} // namespace updater
} // namespace software
} // namespace phosphor
