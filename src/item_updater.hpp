#pragma once

#include "config.h"

#include "activation.hpp"
#include "association_interface.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "version.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Collection/DeleteAll/server.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <variant>
#include <vector>

class TestItemUpdater;

namespace phosphor
{
namespace software
{
namespace updater
{

class Version;

using ItemUpdaterInherit = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

namespace MatchRules = sdbusplus::bus::match::rules;

namespace fs = std::filesystem;

/** @class ItemUpdater
 *  @brief Manages the activation of the PSU version items.
 */
class ItemUpdater :
    public ItemUpdaterInherit,
    public AssociationInterface,
    public ActivationListener
{
    friend class ::TestItemUpdater;

  public:
    /** @brief Constructs ItemUpdater
     *
     * @param[in] bus    - The D-Bus bus object
     * @param[in] path   - The D-Bus path
     */
    ItemUpdater(sdbusplus::bus_t& bus, const std::string& path) :
        ItemUpdaterInherit(bus, path.c_str()), bus(bus),
        versionMatch(
            bus,
            MatchRules::interfacesAdded() + MatchRules::path(SOFTWARE_OBJPATH),
            std::bind(std::mem_fn(&ItemUpdater::onVersionInterfacesAddedMsg),
                      this, std::placeholders::_1)),
        psuInterfaceMatch(
            bus,
            MatchRules::interfacesAdded() +
                MatchRules::path("/xyz/openbmc_project/inventory") +
                MatchRules::sender("xyz.openbmc_project.Inventory.Manager"),
            std::bind(std::mem_fn(&ItemUpdater::onPSUInterfacesAdded), this,
                      std::placeholders::_1))
    {
        processPSUImageAndSyncToLatest();
    }

    /** @brief Deletes version
     *
     *  @param[in] versionId - Id of the version to delete
     */
    void erase(const std::string& versionId);

    /** @brief Creates an active association to the
     *  newly active software image
     *
     * @param[in]  path - The path to create the association to.
     */
    void createActiveAssociation(const std::string& path) override;

    /** @brief Add the functional association to the
     *  new "running" PSU images
     *
     * @param[in]  path - The path to add the association to.
     */
    void addFunctionalAssociation(const std::string& path) override;

    /** @brief Add the updateable association to the
     *  "running" PSU software image
     *
     * @param[in]  path - The path to create the association.
     */
    void addUpdateableAssociation(const std::string& path) override;

    /** @brief Removes the associations from the provided software image path
     *
     * @param[in]  path - The path to remove the association from.
     */
    void removeAssociation(const std::string& path) override;

    /** @brief Notify a PSU is updated
     *
     * @param[in]  versionId - The versionId of the activation
     * @param[in]  psuInventoryPath - The PSU inventory path that is updated
     */
    void onUpdateDone(const std::string& versionId,
                      const std::string& psuInventoryPath) override;

  private:
    using Properties =
        std::map<std::string, utils::UtilsInterface::PropertyType>;
    using InterfacesAddedMap =
        std::map<std::string,
                 std::map<std::string, std::variant<bool, std::string>>>;

    /** @brief Callback function for Software.Version match.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void onVersionInterfacesAddedMsg(sdbusplus::message_t& msg);

    /** @brief Called when new Software.Version interfaces are found
     *  @details Creates an Activation D-Bus object if appropriate
     *           Throws an exception if an error occurs.
     *
     * @param[in]  path       - D-Bus object path
     * @param[in]  interfaces - D-Bus interfaces that were added
     */
    void onVersionInterfacesAdded(const std::string& path,
                                  const InterfacesAddedMap& interfaces);

    /** @brief Callback function for PSU inventory match.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void onPsuInventoryChangedMsg(sdbusplus::message_t& msg);

    /** @brief Called when a PSU inventory object has changed
     *  @details Update an Activation D-Bus object for PSU inventory.
     *           Throws an exception if an error occurs.
     *
     * @param[in]  psuPath - The PSU inventory path
     * @param[in]  properties - The updated properties
     */
    void onPsuInventoryChanged(const std::string& psuPath,
                               const Properties& properties);

    /** @brief Create Activation object */
    std::unique_ptr<Activation> createActivationObject(
        const std::string& path, const std::string& versionId,
        const std::string& extVersion, Activation::Status activationStatus,
        const AssociationList& assocs, const std::string& filePath);

    /** @brief Create Version object */
    std::unique_ptr<Version> createVersionObject(
        const std::string& objPath, const std::string& versionId,
        const std::string& versionString,
        sdbusplus::xyz::openbmc_project::Software::server::Version::
            VersionPurpose versionPurpose);

    /** @brief Create Activation and Version object for PSU inventory
     *  @details If the same version exists for multiple PSUs, just add
     *           related association, instead of creating new objects.
     * */
    void createPsuObject(const std::string& psuInventoryPath,
                         const std::string& psuVersion);

    /** @brief Remove Activation and Version object for PSU inventory
     *  @details If the same version exists for multiple PSUs, just remove
     *           related association.
     *           If the version has no association, the Activation and
     *           Version object will be removed
     */
    void removePsuObject(const std::string& psuInventoryPath);

    /** @brief Add PSU inventory path to the PSU status map
     *  @details Also adds a PropertiesChanged listener for the inventory path
     *           so we are notified when the Present property changes.
     *           Does nothing if the inventory path already exists in the map.
     *
     * @param[in]  psuPath - The PSU inventory path
     */
    void addPsuToStatusMap(const std::string& psuPath);

    /** @brief Handle a change in presence for a PSU.
     *
     * @param[in]  psuPath - The PSU inventory path
     */
    void handlePSUPresenceChanged(const std::string& psuPath);

    /**
     * @brief Create and populate the active PSU Version.
     */
    void processPSUImage();

    /** @brief Create PSU Version from stored images */
    void processStoredImage();

    /** @brief Scan a directory and create PSU Version from stored images
     *  @details Throws an exception if an error occurs
     *
     * @param[in] dir Directory path to scan
     */
    void scanDirectory(const fs::path& dir);

    /** @brief Find the PSU model subdirectory within the specified directory
     *  @details Throws an exception if an error occurs
     *
     * @param[in] dir Directory path to search
     *
     * @return Subdirectory path, or an empty path if none found
     */
    fs::path findModelDirectory(const fs::path& dir);

    /** @brief Get the versionId of the latest PSU version */
    std::optional<std::string> getLatestVersionId();

    /** @brief Update PSUs to the latest version */
    void syncToLatestImage();

    /** @brief Invoke the activation via DBus */
    static void invokeActivation(const std::unique_ptr<Activation>& activation);

    /** @brief Callback function for interfaces added signal.
     *
     * This method is called when new interfaces are added. It updates the
     * internal status map and processes the new PSU if it's present.
     *
     *  @param[in] msg - Data associated with subscribed signal
     */
    void onPSUInterfacesAdded(sdbusplus::message_t& msg);

    /**
     * @brief Handles the processing of PSU images.
     *
     * This function responsible for invoking the sequence of processing PSU
     * images, processing stored images, and syncing to the latest firmware
     * image.
     */
    void processPSUImageAndSyncToLatest();

    /** @brief Retrieve FW version from IMG_DIR_BUILTIN
     *
     * This function retrieves the firmware version from the PSU model directory
     * that is in the IMG_DIR_BUILTIN. It loops through the activations map to
     * find matching path starts with IMG_DIR_BUILTIN, then gets the
     * corresponding version ID, and then looks it up in the versions map to
     * retrieve the associated version string.
     */
    std::string getFWVersionFromBuiltinDir();

    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus_t& bus;

    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Activation>> activations;

    /** @brief Persistent map of Version D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Version>> versions;

    /** @brief The reference map of PSU Inventory objects and the
     * Activation*/
    std::map<std::string, const std::unique_ptr<Activation>&>
        psuPathActivationMap;

    /** @brief sdbusplus signal match for PSU Software*/
    sdbusplus::bus::match_t versionMatch;

    /** @brief sdbusplus signal matches for PSU Inventory */
    std::vector<sdbusplus::bus::match_t> psuMatches;

    /** @brief This entry's associations */
    AssociationList assocs;

    /** @brief A collection of the version strings */
    std::set<std::string> versionStrings;

    /** @brief A struct to hold the PSU present status and model */
    struct psuStatus
    {
        bool present;
        std::string model;
    };

    /** @brief The map of PSU inventory path and the psuStatus
     *
     * It is used to handle psu inventory changed event, that only create psu
     * software object when a PSU is present and the model is retrieved */
    std::map<std::string, psuStatus> psuStatusMap;

    /** @brief Signal match for PSU interfaces added.
     *
     * This match listens for D-Bus signals indicating new interface has been
     * added. When such a signal received, it triggers the
     * `onInterfacesAdded` method to handle the new PSU.
     */
    sdbusplus::bus::match_t psuInterfaceMatch;
};

} // namespace updater
} // namespace software
} // namespace phosphor
