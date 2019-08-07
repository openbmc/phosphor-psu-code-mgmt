#include "config.h"

#include "utils.hpp"

#include <openssl/sha.h>

#include <fstream>
#include <phosphor-logging/log.hpp>

using namespace phosphor::logging;

namespace utils
{

namespace // anonymous
{
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
} // namespace

const UtilsInterface& getUtils()
{
    static Utils utils;
    return utils;
}

std::vector<std::string>
    Utils::getPSUInventoryPath(sdbusplus::bus::bus& bus) const
{
    std::vector<std::string> paths;
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetSubTreePaths");
    method.append(PSU_INVENTORY_PATH_BASE);
    method.append(0); // Depth 0 to search all
    method.append(std::vector<std::string>({PSU_INVENTORY_IFACE}));
    auto reply = bus.call(method);

    reply.read(paths);
    return paths;
}

std::string Utils::getService(sdbusplus::bus::bus& bus, const char* path,
                              const char* interface) const
{
    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");

    mapper.append(path, std::vector<std::string>({interface}));
    try
    {
        auto mapperResponseMsg = bus.call(mapper);

        std::vector<std::pair<std::string, std::vector<std::string>>>
            mapperResponse;
        mapperResponseMsg.read(mapperResponse);
        if (mapperResponse.empty())
        {
            log<level::ERR>("Error reading mapper response");
            throw std::runtime_error("Error reading mapper response");
        }
        if (mapperResponse.size() < 1)
        {
            return "";
        }
        return mapperResponse[0].first;
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Mapper call failed", entry("METHOD=%d", "GetObject"),
                        entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface));
        throw std::runtime_error("Mapper call failed");
    }
}

std::string Utils::getVersionId(const std::string& version) const
{
    if (version.empty())
    {
        log<level::ERR>("Error version is empty");
        return {};
    }

    unsigned char digest[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, version.c_str(), strlen(version.c_str()));
    SHA512_Final(digest, &ctx);
    char mdString[SHA512_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i * 2], 3, "%02x", (unsigned int)digest[i]);
    }

    // Only need 8 hex digits.
    std::string hexId = std::string(mdString);
    return (hexId.substr(0, 8));
}

any Utils::getPropertyImpl(sdbusplus::bus::bus& bus, const char* service,
                           const char* path, const char* interface,
                           const char* propertyName) const
{
    auto method = bus.new_method_call(service, path,
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(interface, propertyName);
    try
    {
        PropertyType value{};
        auto reply = bus.call(method);
        reply.read(value);
        return any(value);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("GetProperty call failed", entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface),
                        entry("PROPERTY=%s", propertyName));
        throw std::runtime_error("GetProperty call failed");
    }
}

} // namespace utils
