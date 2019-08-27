#include "item_updater.hpp"
#include "mocked_utils.hpp"

#include <sdbusplus/test/sdbus_mock.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::software::updater;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::StrEq;

using std::experimental::any;

class TestItemUpdater : public ::testing::Test
{
  public:
    using Properties = ItemUpdater::Properties;
    using PropertyType = utils::UtilsInterface::PropertyType;

    TestItemUpdater() :
        mockedUtils(
            reinterpret_cast<const utils::MockedUtils&>(utils::getUtils()))
    {
        ON_CALL(mockedUtils, getVersionId(_)).WillByDefault(ReturnArg<0>());
    }

    ~TestItemUpdater()
    {
    }

    const auto& GetActivations()
    {
        return itemUpdater->activations;
    }

    std::string getObjPath(const std::string& versionId)
    {
        return std::string(dBusPath) + "/" + versionId;
    }

    void onPsuInventoryChanged(const std::string& psuPath,
                               const Properties& properties)
    {
        itemUpdater->onPsuInventoryChanged(psuPath, properties);
    }

    static constexpr auto dBusPath = SOFTWARE_OBJPATH;
    sdbusplus::SdBusMock sdbusMock;
    sdbusplus::bus::bus mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    const utils::MockedUtils& mockedUtils;
    std::unique_ptr<ItemUpdater> itemUpdater;
};

TEST_F(TestItemUpdater, ctordtor)
{
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);
}

TEST_F(TestItemUpdater, NotCreateObjectOnNotPresent)
{
    constexpr auto psuPath = "/com/example/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    constexpr auto version = "version0";
    std::string objPath = getObjPath(version);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psuPath)))
        .WillOnce(Return(std::string(version)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(false)))); // not present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // No activation/version objects are created
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(0);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);
}

TEST_F(TestItemUpdater, CreateOnePSUOnPresent)
{
    constexpr auto psuPath = "/com/example/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    constexpr auto version = "version0";
    std::string objPath = getObjPath(version);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psuPath)))
        .WillOnce(Return(std::string(version)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(2);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);
}

TEST_F(TestItemUpdater, CreateTwoPSUsWithSameVersion)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    constexpr auto psu1 = "/com/example/inventory/psu1";
    constexpr auto service = "com.example.Software.Psu";
    auto version0 = std::string("version0");
    auto version1 = std::string("version0");
    auto objPath0 = getObjPath(version0);
    auto objPath1 = getObjPath(version1);

    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psu0, psu1})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu0), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu1), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psu0)))
        .WillOnce(Return(std::string(version0)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psu1)))
        .WillOnce(Return(std::string(version1)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath0)))
        .Times(2);
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
    constexpr auto psu0 = "/com/example/inventory/psu0";
    constexpr auto psu1 = "/com/example/inventory/psu1";
    constexpr auto service = "com.example.Software.Psu";
    auto version0 = std::string("version0");
    auto version1 = std::string("version1");
    auto objPath0 = getObjPath(version0);
    auto objPath1 = getObjPath(version1);

    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psu0, psu1})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu0), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu1), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psu0)))
        .WillOnce(Return(std::string(version0)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psu1)))
        .WillOnce(Return(std::string(version1)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // two new activation and version objects will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath0)))
        .Times(2);
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath1)))
        .Times(2);
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

TEST_F(TestItemUpdater, OnOnePSURemoved)
{
    constexpr auto psuPath = "/com/example/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    constexpr auto version = "version0";
    std::string objPath = getObjPath(version);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psuPath)))
        .WillOnce(Return(std::string(version)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(2);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);

    // the activation and version object will be removed
    Properties p{{PRESENT, PropertyType(false)}};
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(objPath)))
        .Times(2);
    onPsuInventoryChanged(psuPath, p);

    // on exit, item updater is removed
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(dBusPath)))
        .Times(1);
}

TEST_F(TestItemUpdater, OnOnePSUAdded)
{
    constexpr auto psuPath = "/com/example/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    constexpr auto version = "version0";
    std::string objPath = getObjPath(version);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psuPath)))
        .WillOnce(Return(std::string(version)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(false)))); // not present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // No activation/version objects are created
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(0);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);

    // The PSU is present and version is added in a single call
    Properties propAdded{{PRESENT, PropertyType(true)},
                         {VERSION, PropertyType(std::string(version))}};
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(2);
    onPsuInventoryChanged(psuPath, propAdded);
}

TEST_F(TestItemUpdater, OnOnePSURemovedAndAdded)
{
    constexpr auto psuPath = "/com/example/inventory/psu0";
    constexpr auto service = "com.example.Software.Psu";
    constexpr auto version = "version0";
    std::string objPath = getObjPath(version);
    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psuPath})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psuPath), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psuPath)))
        .WillOnce(Return(std::string(version)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psuPath),
                                             _, StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(2);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);

    // the activation and version object will be removed
    Properties propRemoved{{PRESENT, PropertyType(false)}};
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(objPath)))
        .Times(2);
    onPsuInventoryChanged(psuPath, propRemoved);

    Properties propAdded{{PRESENT, PropertyType(true)}};
    Properties propVersion{{VERSION, PropertyType(std::string(version))}};
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath)))
        .Times(2);
    onPsuInventoryChanged(psuPath, propAdded);
    onPsuInventoryChanged(psuPath, propVersion);

    // on exit, objects are removed
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(objPath)))
        .Times(2);
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(dBusPath)))
        .Times(1);
}

TEST_F(TestItemUpdater,
       TwoPSUsWithSameVersionRemovedAndAddedWithDifferntVersion)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    constexpr auto psu1 = "/com/example/inventory/psu1";
    constexpr auto service = "com.example.Software.Psu";
    auto version0 = std::string("version0");
    auto version1 = std::string("version0");
    auto objPath0 = getObjPath(version0);
    auto objPath1 = getObjPath(version1);

    EXPECT_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillOnce(Return(std::vector<std::string>({psu0, psu1})));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu0), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getService(_, StrEq(psu1), _))
        .WillOnce(Return(service));
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psu0)))
        .WillOnce(Return(std::string(version0)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu0), _,
                                             StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present
    EXPECT_CALL(mockedUtils, getVersion(StrEq(psu1)))
        .WillOnce(Return(std::string(version1)));
    EXPECT_CALL(mockedUtils, getPropertyImpl(_, StrEq(service), StrEq(psu1), _,
                                             StrEq(PRESENT)))
        .WillOnce(Return(any(PropertyType(true)))); // present

    // The item updater itself
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(dBusPath)))
        .Times(1);

    // activation and version object will be added
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath0)))
        .Times(2);
    itemUpdater = std::make_unique<ItemUpdater>(mockedBus, dBusPath);

    // Verify there is only one activation and it has two associations
    const auto& activations = GetActivations();
    EXPECT_EQ(1u, activations.size());
    const auto& activation = activations.find(version0)->second;
    auto assocs = activation->associations();
    EXPECT_EQ(2u, assocs.size());
    EXPECT_EQ(psu0, std::get<2>(assocs[0]));
    EXPECT_EQ(psu1, std::get<2>(assocs[1]));

    // PSU0 is removed, only associations shall be updated
    Properties propRemoved{{PRESENT, PropertyType(false)}};
    onPsuInventoryChanged(psu0, propRemoved);
    assocs = activation->associations();
    EXPECT_EQ(1u, assocs.size());
    EXPECT_EQ(psu1, std::get<2>(assocs[0]));

    // PSU1 is removed, the activation and version object shall be removed
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(objPath0)))
        .Times(2);
    onPsuInventoryChanged(psu1, propRemoved);

    // Add PSU0 and PSU1 back, but PSU1 with a different version
    version1 = "version1";
    objPath1 = getObjPath(version1);
    Properties propAdded0{{PRESENT, PropertyType(true)},
                          {VERSION, PropertyType(std::string(version0))}};
    Properties propAdded1{{PRESENT, PropertyType(true)},
                          {VERSION, PropertyType(std::string(version1))}};
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath0)))
        .Times(2);
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_added(_, StrEq(objPath1)))
        .Times(2);
    onPsuInventoryChanged(psu0, propAdded0);
    onPsuInventoryChanged(psu1, propAdded1);

    // on exit, objects are removed
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(objPath0)))
        .Times(2);
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(objPath1)))
        .Times(2);
    EXPECT_CALL(sdbusMock, sd_bus_emit_object_removed(_, StrEq(dBusPath)))
        .Times(1);
}
