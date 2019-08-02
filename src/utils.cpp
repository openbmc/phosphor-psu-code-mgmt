#include "config.h"

#include "utils.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>

using json = nlohmann::json;
using namespace phosphor::logging;

namespace utils
{

namespace details
{

json loadFromFile(const char* path)
{
    std::ifstream ifs(path);
    if (!ifs.good())
    {
        log<level::ERR>("Unable to open file", entry("PATH=%s", path));
        return nullptr;
    }
    auto data = json::parse(ifs, nullptr, false);
    if (data.is_discarded())
    {
        log<level::ERR>("Failed to parse json", entry("PATH=%s", path));
        return nullptr;
    }
    return data;
}
} // namespace details

std::vector<std::string> getPSUInventoryPath()
{
    auto data = details::loadFromFile(PSU_JSON_CONFIG);

    if (data == nullptr)
    {
        return {};
    }

    if (data.find("PSU_INVENTORY_PATH") == data.end())
    {
        log<level::WARNING>("Unable to find PSU_INVENTORY_PATH");
    }

    return data["PSU_INVENTORY_PATH"];
}

} // namespace utils
