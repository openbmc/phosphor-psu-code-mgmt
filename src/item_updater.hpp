#pragma once

#include "activation.hpp"
#include "types.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

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
                         MatchRules::path("/xyz/openbmc_project/software"),
                     std::bind(std::mem_fn(&ItemUpdater::createActivation),
                               this, std::placeholders::_1))
    {
    }

    virtual ~ItemUpdater() = default;

    /** @brief Deletes version
     *
     *  @param[in] entryId - Id of the version to delete
     *
     *  @return - Returns true if the version is deleted.
     */
    bool erase(std::string entryId);

    /**
     * @brief Erases any non-active versions.
     */
    void deleteAll();

    /** @brief Creates an active association to the
     *  newly active software image
     *
     * @param[in]  path - The path to create the association to.
     */
    virtual void createActiveAssociation(const std::string& path);

    /** @brief Updates the functional association to the
     *  new "running" PNOR image
     *
     * @param[in]  versionId - The id of the image to update the association to.
     */
    virtual void updateFunctionalAssociation(const std::string& versionId);

    /** @brief Removes the associations from the provided software image path
     *
     * @param[in]  path - The path to remove the association from.
     */
    virtual void removeAssociation(const std::string& path);

  protected:
    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void createActivation(sdbusplus::message::message& msg);

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

    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Activation>> activations;

    /** @brief Persistent map of Version D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Version>> versions;

    /** @brief sdbusplus signal match for Software.Version */
    sdbusplus::bus::match_t versionMatch;

    /** @brief This entry's associations */
    AssociationList assocs = {};
};

} // namespace updater
} // namespace software
} // namespace phosphor
