#include "activation.hpp"

#include <sdbusplus/test/sdbus_mock.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::software::updater;

class TestActivation : public ::testing::Test
{
  public:
    using ActivationStatus = sdbusplus::xyz::openbmc_project::Software::server::
        Activation::Activations;
    TestActivation()
    {
    }
    ~TestActivation()
    {
    }
    static constexpr auto dBusPath = SOFTWARE_OBJPATH;
    sdbusplus::SdBusMock sdbusMock;
    sdbusplus::bus::bus mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    std::unique_ptr<Activation> activation;
    std::string versionId = "abcdefgh";
    std::string extVersion = "Some Ext Version";
    ActivationStatus status = ActivationStatus::Active;
    AssociationList associations;
};

TEST_F(TestActivation, ctordtor)
{
    activation = std::make_unique<Activation>(mockedBus, dBusPath, versionId,
                                              extVersion, status, associations);
}

namespace phosphor::software::updater::internal
{
extern std::string getUpdateService(const std::string& psuInventoryPath,
                                    const std::string& versionId);
}

TEST_F(TestActivation, getUpdateService)
{
    std::string psuInventoryPath = "/com/example/inventory/powersupply1";
    std::string versionId = "12345678";
    std::string toCompare = "psu-update@-com-example-inventory-"
                            "powersupply1\\x20-tmp-images-12345678.service";

    auto service = phosphor::software::updater::internal::getUpdateService(
        psuInventoryPath, versionId);
    EXPECT_EQ(toCompare, service);
}
