#include "item_updater.hpp"
#include "mocked_utils.hpp"

#include <sdbusplus/test/sdbus_mock.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::software::updater;
using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

using std::experimental::any;
using PropertyType = utils::UtilsInterface::PropertyType;

class TestItemUpdater : public ::testing::Test
{
  public:
    TestItemUpdater() :
        mockedUtils(
            reinterpret_cast<const utils::MockedUtils&>(utils::getUtils()))
    {
    }

    ~TestItemUpdater()
    {
    }

    const auto& GetActivations()
    {
        return itemUpdater->activations;
    }

    static constexpr auto dBusPath = "/com/example/software/";
    sdbusplus::SdBusMock sdbusMock;
    const utils::MockedUtils& mockedUtils;
    std::unique_ptr<ItemUpdater> itemUpdater;
};

TEST_F(TestItemUpdater, ctordtor)
{
    auto mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);
}

TEST_F(TestItemUpdater, NotCreateObjectOnNotPresent)
{
    constexpr auto psuPath = "/com/exmaple/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    auto mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq("Version")))
        .WillOnce(Return(
            any(PropertyType(std::string("some version"))))); // dummy version
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq("Present")))
        .WillOnce(Return(any(PropertyType(false)))); // not present

    // only item_updater itself is created added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, _)).Times(1);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);
}

TEST_F(TestItemUpdater, CreateOnePSUOnPresent)
{
    constexpr auto psuPath = "/com/exmaple/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    auto mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq("Version")))
        .WillOnce(Return(
            any(PropertyType(std::string("some version"))))); // dummy version
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq("Present")))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // item_updater, new activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, _)).Times(3);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);
}

TEST_F(TestItemUpdater, CreateTwoPSUsWithSameVersion)
{
    constexpr auto psu0 = "/com/exmaple/inventory/psu0";
    constexpr auto psu1 = "/com/exmaple/inventory/psu1";
    constexpr auto service = "com.example.Software.Psu";
    auto version0 = std::string("some version");
    auto version1 = version0;

    auto mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    ON_CALL(mockedUtils, getVersionId(StrEq(version0)))
        .WillByDefault(Return(std::string(version0)));
    ON_CALL(mockedUtils, getVersionId(StrEq(version1)))
        .WillByDefault(Return(std::string(version1)));
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psu0, psu1})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu0), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu1), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq("Version")))
        .WillOnce(Return(any(PropertyType(version0))));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq("Present")))
        .WillOnce(Return(any(PropertyType(true)))); // present
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq("Version")))
        .WillOnce(Return(any(PropertyType(version1))));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq("Present")))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // item_updater, only one new activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, _)).Times(3);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);

    // Verify there is only one activation and it has two associations
    const auto& activations = GetActivations();
    EXPECT_EQ(1u, activations.size());
    const auto& activation = activations.find(version0)->second;
    const auto& assocs = activation->associations();
    EXPECT_EQ(2u, assocs.size());
    EXPECT_EQ(psu0, std::get<2>(assocs[0]));
    EXPECT_EQ(psu1, std::get<2>(assocs[1]));
}

TEST_F(TestItemUpdater, CreateTwoPSUsWithDifferentVersion)
{
    constexpr auto psu0 = "/com/exmaple/inventory/psu0";
    constexpr auto psu1 = "/com/exmaple/inventory/psu1";
    constexpr auto service = "com.example.Software.Psu";
    auto version0 = std::string("some version");
    auto version1 = std::string("different version");

    auto mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    ON_CALL(mockedUtils, getVersionId(StrEq(version0)))
        .WillByDefault(Return(std::string(version0)));
    ON_CALL(mockedUtils, getVersionId(StrEq(version1)))
        .WillByDefault(Return(std::string(version1)));
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psu0, psu1})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu0), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu1), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq("Version")))
        .WillOnce(Return(any(PropertyType(version0))));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq("Present")))
        .WillOnce(Return(any(PropertyType(true)))); // present
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq("Version")))
        .WillOnce(Return(any(PropertyType(version1))));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq("Present")))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // item_updater, two new activation and version objects will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, _)).Times(5);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);

    // Verify there are two activations and each with one association
    const auto& activations = GetActivations();
    EXPECT_EQ(2u, activations.size());
    const auto& activation0 = activations.find(version0)->second;
    const auto& assocs0 = activation0->associations();
    EXPECT_EQ(1u, assocs0.size());
    EXPECT_EQ(psu0, std::get<2>(assocs0[0]));

    const auto& activation1 = activations.find(version1)->second;
    const auto& assocs1 = activation1->associations();
    EXPECT_EQ(1u, assocs1.size());
    EXPECT_EQ(psu1, std::get<2>(assocs1[0]));
}
