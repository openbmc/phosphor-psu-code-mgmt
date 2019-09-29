#include "activation.hpp"
#include "mocked_association_interface.hpp"
#include "mocked_utils.hpp"

#include <sdbusplus/test/sdbus_mock.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace phosphor::software::updater;

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

using std::experimental::any;

class TestActivation : public ::testing::Test
{
  public:
    using PropertyType = utils::UtilsInterface::PropertyType;
    using Status = Activation::Status;
    using RequestedStatus = Activation::RequestedActivations;
    TestActivation() :
        mockedUtils(
            reinterpret_cast<const utils::MockedUtils&>(utils::getUtils()))
    {
        // By default make it compatible with the test software
        ON_CALL(mockedUtils, getPropertyImpl(_, _, _, _, StrEq(MANUFACTURER)))
            .WillByDefault(Return(any(PropertyType(std::string("TestManu")))));
        ON_CALL(mockedUtils, getPropertyImpl(_, _, _, _, StrEq(MODEL)))
            .WillByDefault(Return(any(PropertyType(std::string("TestModel")))));
    }
    ~TestActivation()
    {
    }

    void onUpdateDone()
    {
        activation->onUpdateDone();
    }
    void onUpdateFailed()
    {
        activation->onUpdateFailed();
    }
    int getProgress()
    {
        return activation->activationProgress->progress();
    }
    const auto& getPsuQueue()
    {
        return activation->psuQueue;
    }

    sdbusplus::SdBusMock sdbusMock;
    sdbusplus::bus::bus mockedBus = sdbusplus::get_mocked_new(&sdbusMock);
    const utils::MockedUtils& mockedUtils;
    MockedAssociationInterface mockedAssociationInterface;
    std::unique_ptr<Activation> activation;
    std::string versionId = "abcdefgh";
    std::string extVersion = "manufacturer=TestManu,model=TestModel";
    std::string filePath = "";
    std::string dBusPath = std::string(SOFTWARE_OBJPATH) + "/" + versionId;
    Status status = Status::Ready;
    AssociationList associations;
};

TEST_F(TestActivation, ctordtor)
{
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
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

TEST_F(TestActivation, doUpdateWhenNoPSU)
{
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(
            Return(std::vector<std::string>({}))); // No PSU inventory
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_CALL(mockedAssociationInterface, createActiveAssociation(dBusPath))
        .Times(0);
    EXPECT_CALL(mockedAssociationInterface, addFunctionalAssociation(dBusPath))
        .Times(0);
    EXPECT_EQ(Status::Failed, activation->activation());
}

TEST_F(TestActivation, doUpdateOnePSUOK)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(
            Return(std::vector<std::string>({psu0}))); // One PSU inventory
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Activating, activation->activation());

    EXPECT_CALL(mockedAssociationInterface, createActiveAssociation(dBusPath))
        .Times(1);
    EXPECT_CALL(mockedAssociationInterface, addFunctionalAssociation(dBusPath))
        .Times(1);
    onUpdateDone();
    EXPECT_EQ(Status::Active, activation->activation());
}

TEST_F(TestActivation, doUpdateFourPSUsOK)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    constexpr auto psu1 = "/com/example/inventory/psu1";
    constexpr auto psu2 = "/com/example/inventory/psu2";
    constexpr auto psu3 = "/com/example/inventory/psu3";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(Return(
            std::vector<std::string>({psu0, psu1, psu2, psu3}))); // 4 PSUs
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(10, getProgress());

    onUpdateDone();
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(30, getProgress());

    onUpdateDone();
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(50, getProgress());

    onUpdateDone();
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(70, getProgress());

    EXPECT_CALL(mockedAssociationInterface, createActiveAssociation(dBusPath))
        .Times(1);
    EXPECT_CALL(mockedAssociationInterface, addFunctionalAssociation(dBusPath))
        .Times(1);

    onUpdateDone();
    EXPECT_EQ(Status::Active, activation->activation());
}

TEST_F(TestActivation, doUpdateFourPSUsFailonSecond)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    constexpr auto psu1 = "/com/example/inventory/psu1";
    constexpr auto psu2 = "/com/example/inventory/psu2";
    constexpr auto psu3 = "/com/example/inventory/psu3";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(Return(
            std::vector<std::string>({psu0, psu1, psu2, psu3}))); // 4 PSUs
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(10, getProgress());

    onUpdateDone();
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(30, getProgress());

    EXPECT_CALL(mockedAssociationInterface, createActiveAssociation(dBusPath))
        .Times(0);
    EXPECT_CALL(mockedAssociationInterface, addFunctionalAssociation(dBusPath))
        .Times(0);
    onUpdateFailed();
    EXPECT_EQ(Status::Failed, activation->activation());
}

TEST_F(TestActivation, doUpdateOnExceptionFromDbus)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(
            Return(std::vector<std::string>({psu0}))); // One PSU inventory
    ON_CALL(sdbusMock, sd_bus_call(_, _, _, _, nullptr))
        .WillByDefault(Return(-1)); // Make sdbus call failure
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Failed, activation->activation());
}

TEST_F(TestActivation, doUpdateOnePSUModelNotCompatible)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    extVersion = "manufacturer=TestManu,model=DifferentModel";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(Return(std::vector<std::string>({psu0})));
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Failed, activation->activation());
}

TEST_F(TestActivation, doUpdateOnePSUManufactureNotCompatible)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    extVersion = "manufacturer=DifferentManu,model=TestModel";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(Return(std::vector<std::string>({psu0})));
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Failed, activation->activation());
}

TEST_F(TestActivation, doUpdateOnePSUSelfManufactureIsEmpty)
{
    ON_CALL(mockedUtils, getPropertyImpl(_, _, _, _, StrEq(MANUFACTURER)))
        .WillByDefault(Return(any(PropertyType(std::string("")))));
    extVersion = "manufacturer=AnyManu,model=TestModel";
    // Below is the same as doUpdateOnePSUOK case
    constexpr auto psu0 = "/com/example/inventory/psu0";
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(
            Return(std::vector<std::string>({psu0}))); // One PSU inventory
    activation->requestedActivation(RequestedStatus::Active);

    EXPECT_EQ(Status::Activating, activation->activation());

    EXPECT_CALL(mockedAssociationInterface, createActiveAssociation(dBusPath))
        .Times(1);
    EXPECT_CALL(mockedAssociationInterface, addFunctionalAssociation(dBusPath))
        .Times(1);
    onUpdateDone();
    EXPECT_EQ(Status::Active, activation->activation());
}

TEST_F(TestActivation, doUpdateFourPSUsSecondPSUNotCompatible)
{
    constexpr auto psu0 = "/com/example/inventory/psu0";
    constexpr auto psu1 = "/com/example/inventory/psu1";
    constexpr auto psu2 = "/com/example/inventory/psu2";
    constexpr auto psu3 = "/com/example/inventory/psu3";
    ON_CALL(mockedUtils, getPropertyImpl(_, _, StrEq(psu1), _, StrEq(MODEL)))
        .WillByDefault(
            Return(any(PropertyType(std::string("DifferentModel")))));
    activation = std::make_unique<Activation>(
        mockedBus, dBusPath, versionId, extVersion, status, associations,
        &mockedAssociationInterface, filePath);
    ON_CALL(mockedUtils, getPSUInventoryPath(_))
        .WillByDefault(Return(
            std::vector<std::string>({psu0, psu1, psu2, psu3}))); // 4 PSUs
    activation->requestedActivation(RequestedStatus::Active);

    const auto& psuQueue = getPsuQueue();
    EXPECT_EQ(3u, psuQueue.size());

    // Only 3 PSUs shall be updated, and psu1 shall be skipped
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(10, getProgress());

    onUpdateDone();
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(36, getProgress());

    onUpdateDone();
    EXPECT_EQ(Status::Activating, activation->activation());
    EXPECT_EQ(62, getProgress());

    EXPECT_CALL(mockedAssociationInterface, createActiveAssociation(dBusPath))
        .Times(1);
    EXPECT_CALL(mockedAssociationInterface, addFunctionalAssociation(dBusPath))
        .Times(1);

    onUpdateDone();
    EXPECT_EQ(Status::Active, activation->activation());
}
