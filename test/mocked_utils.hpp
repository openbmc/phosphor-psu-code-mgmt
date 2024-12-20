#include "utils.hpp"

#include <gmock/gmock.h>

namespace utils
{

class MockedUtils : public UtilsInterface
{
  public:
    MockedUtils() = default;
    MockedUtils(const MockedUtils&) = delete;
    MockedUtils& operator=(const MockedUtils&) = delete;
    MockedUtils(MockedUtils&&) = delete;
    MockedUtils& operator=(MockedUtils&&) = delete;

    ~MockedUtils() override = default;

    MOCK_CONST_METHOD1(getPSUInventoryPaths,
                       std::vector<std::string>(sdbusplus::bus_t& bus));

    MOCK_CONST_METHOD3(getService,
                       std::string(sdbusplus::bus_t& bus, const char* path,
                                   const char* interface));

    MOCK_CONST_METHOD3(getServices,
                       std::vector<std::string>(sdbusplus::bus_t& bus,
                                                const char* path,
                                                const char* interface));

    MOCK_CONST_METHOD1(getVersionId, std::string(const std::string& version));

    MOCK_CONST_METHOD1(getVersion,
                       std::string(const std::string& psuInventoryPath));

    MOCK_CONST_METHOD1(getModel,
                       std::string(const std::string& psuInventoryPath));

    MOCK_CONST_METHOD1(getLatestVersion,
                       std::string(const std::set<std::string>& versions));

    MOCK_CONST_METHOD2(isAssociated, bool(const std::string& psuInventoryPath,
                                          const AssociationList& assocs));

    MOCK_CONST_METHOD5(getPropertyImpl,
                       any(sdbusplus::bus_t& bus, const char* service,
                           const char* path, const char* interface,
                           const char* propertyName));
};

static std::unique_ptr<MockedUtils> utils;
inline const UtilsInterface& getUtils()
{
    if (!utils)
    {
        utils = std::make_unique<MockedUtils>();
    }
    return *utils;
}

inline void freeUtils()
{
    utils.reset();
}

} // namespace utils
