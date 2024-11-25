#include "config.h"

#include "utils.hpp"

#include <openssl/evp.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace utils
{

namespace // anonymous
{
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
} // namespace

namespace internal
{

/**
 * @brief Concatenate the specified values, separated by spaces, and return
 *        the resulting string.
 *
 * @param[in] ts - Parameter pack of values to concatenate
 *
 * @return Parameter values separated by spaces
 */
template <typename... Ts>
std::string concat_string(const Ts&... ts)
{
    std::stringstream s;
    ((s << ts << " "), ...);
    return s.str();
}

/**
 * @brief Execute the specified command.
 *
 * @details Returns a pair containing the exit status and command output.
 *          Throws an exception if an error occurs. Note that a command that
 *          returns a non-zero exit status is not considered an error.
 *
 * @param[in] ts - Parameter pack of command and parameters
 *
 * @return Exit status and standard output from the command
 */
template <typename... Ts>
std::pair<int, std::string> exec(const Ts&... ts)
{
    std::array<char, 512> buffer;
    std::string cmd = concat_string(ts...);
    std::stringstream result;
    int rc;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error{
            std::format("Unable to execute command '{}': popen() failed: {}",
                        cmd, std::strerror(errno))};
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result << buffer.data();
    }
    rc = pclose(pipe);
    return {rc, result.str()};
}

} // namespace internal

const UtilsInterface& getUtils()
{
    static Utils utils;
    return utils;
}

std::vector<std::string>
    Utils::getPSUInventoryPaths(sdbusplus::bus_t& bus) const
{
    std::vector<std::string> paths;
    try
    {
        auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");
        method.append(PSU_INVENTORY_PATH_BASE);
        method.append(0); // Depth 0 to search all
        method.append(std::vector<std::string>({PSU_INVENTORY_IFACE}));
        auto reply = bus.call(method);

        reply.read(paths);
    }
    catch (const std::exception& e)
    {
        // Inventory base path not there yet.
    }
    return paths;
}

std::string Utils::getService(sdbusplus::bus_t& bus, const char* path,
                              const char* interface) const
{
    auto services = getServices(bus, path, interface);
    if (services.empty())
    {
        throw std::runtime_error{std::format(
            "No service found for path {}, interface {}", path, interface)};
    }
    return services[0];
}

std::vector<std::string> Utils::getServices(
    sdbusplus::bus_t& bus, const char* path, const char* interface) const
{
    std::vector<std::string> services;
    try
    {
        auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetObject");

        mapper.append(path, std::vector<std::string>({interface}));

        auto mapperResponseMsg = bus.call(mapper);

        std::vector<std::pair<std::string, std::vector<std::string>>>
            mapperResponse;
        mapperResponseMsg.read(mapperResponse);
        services.reserve(mapperResponse.size());
        for (const auto& i : mapperResponse)
        {
            services.emplace_back(i.first);
        }
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error{
            std::format("Unable to find services for path {}, interface {}: {}",
                        path, interface, e.what())};
    }
    return services;
}

std::string Utils::getVersionId(const std::string& version) const
{
    if (version.empty())
    {
        lg2::error("Error version is empty");
        return {};
    }

    using EVP_MD_CTX_Ptr =
        std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    EVP_MD_CTX_Ptr ctx(EVP_MD_CTX_new(), &::EVP_MD_CTX_free);

    EVP_DigestInit(ctx.get(), EVP_sha512());
    EVP_DigestUpdate(ctx.get(), version.c_str(), strlen(version.c_str()));
    EVP_DigestFinal(ctx.get(), digest.data(), nullptr);

    // Only need 8 hex digits.
    char mdString[9];
    snprintf(mdString, sizeof(mdString), "%02x%02x%02x%02x",
             (unsigned int)digest[0], (unsigned int)digest[1],
             (unsigned int)digest[2], (unsigned int)digest[3]);

    return mdString;
}

std::string Utils::getVersion(const std::string& inventoryPath) const
{
    std::string version;
    try
    {
        // Invoke vendor-specific tool to get the version string, e.g.
        //   psutils --get-version
        //   /xyz/openbmc_project/inventory/system/chassis/motherboard/powersupply0
        auto [rc, output] = internal::exec(PSU_VERSION_UTIL, inventoryPath);
        if (rc == 0)
        {
            version = output;
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Unable to get firmware version for PSU {PSU}: {ERROR}",
                   "PSU", inventoryPath, "ERROR", e);
    }
    return version;
}

std::string Utils::getModel(const std::string& inventoryPath) const
{
    std::string model;
    try
    {
        // Invoke vendor-specific tool to get the model string, e.g.
        //   psutils --get-model
        //   /xyz/openbmc_project/inventory/system/chassis/motherboard/powersupply0
        auto [rc, output] = internal::exec(PSU_MODEL_UTIL, inventoryPath);
        if (rc == 0)
        {
            model = output;
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Unable to get model for PSU {PSU}: {ERROR}", "PSU",
                   inventoryPath, "ERROR", e);
    }
    return model;
}

std::string Utils::getLatestVersion(const std::set<std::string>& versions) const
{
    std::string latestVersion;
    try
    {
        if (!versions.empty())
        {
            std::stringstream args;
            for (const auto& s : versions)
            {
                args << s << " ";
            }
            auto [rc, output] =
                internal::exec(PSU_VERSION_COMPARE_UTIL, args.str());
            if (rc == 0)
            {
                latestVersion = output;
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Unable to get latest PSU firmware version: {ERROR}",
                   "ERROR", e);
    }
    return latestVersion;
}

bool Utils::isAssociated(const std::string& psuInventoryPath,
                         const AssociationList& assocs) const
{
    return std::find_if(assocs.begin(), assocs.end(),
                        [&psuInventoryPath](const auto& assoc) {
                            return psuInventoryPath == std::get<2>(assoc);
                        }) != assocs.end();
}

any Utils::getPropertyImpl(sdbusplus::bus_t& bus, const char* service,
                           const char* path, const char* interface,
                           const char* propertyName) const
{
    any anyValue{};
    try
    {
        auto method = bus.new_method_call(
            service, path, "org.freedesktop.DBus.Properties", "Get");
        method.append(interface, propertyName);
        auto reply = bus.call(method);
        PropertyType value{};
        reply.read(value);
        anyValue = value;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error{std::format(
            "Unable to get property {} for path {} and interface {}: {}",
            propertyName, path, interface, e.what())};
    }
    return anyValue;
}

} // namespace utils
