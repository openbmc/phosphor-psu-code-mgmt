#pragma once

#include "association_interface.hpp"

#include <gmock/gmock.h>

class MockedAssociationInterface : public AssociationInterface
{
  public:
    MockedAssociationInterface() = default;
    MockedAssociationInterface(const MockedAssociationInterface&) = delete;
    MockedAssociationInterface& operator=(const MockedAssociationInterface&) =
        delete;
    MockedAssociationInterface(MockedAssociationInterface&&) = delete;
    MockedAssociationInterface& operator=(MockedAssociationInterface&&) =
        delete;

    ~MockedAssociationInterface() override = default;

    MOCK_METHOD1(createActiveAssociation, void(const std::string& path));
    MOCK_METHOD1(addFunctionalAssociation, void(const std::string& path));
    MOCK_METHOD1(addUpdateableAssociation, void(const std::string& path));
    MOCK_METHOD1(removeAssociation, void(const std::string& path));
};
