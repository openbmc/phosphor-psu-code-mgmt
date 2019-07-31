#include "version.hpp"

#include "item_updater.hpp"

#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <xyz/openbmc_project/Common/error.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
using Argument = xyz::openbmc_project::Common::InvalidArgument;

std::map<std::string, std::string>
    Version::getValue(const std::string& filePath,
                      std::map<std::string, std::string> keys)
{
    if (filePath.empty())
    {
        log<level::ERR>("Error filePath is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FilePath"),
                              Argument::ARGUMENT_VALUE(filePath.c_str()));
    }

    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                     std::ifstream::eofbit);

    try
    {
        efile.open(filePath);
        while (getline(efile, line))
        {
            for (auto& key : keys)
            {
                auto value = key.first + "=";
                auto keySize = value.length();
                if (line.compare(0, keySize, value) == 0)
                {
                    key.second = line.substr(keySize);
                    break;
                }
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        if (!efile.eof())
        {
            log<level::ERR>("Error in reading file");
        }
        efile.close();
    }

    return keys;
}

void Delete::delete_()
{
    if (version.eraseCallback)
    {
        version.eraseCallback(version.getVersionId());
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
