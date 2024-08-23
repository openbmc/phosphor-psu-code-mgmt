#pragma once

#include "activation_listener.hpp"

#include <gmock/gmock.h>

class MockedActivationListener : public ActivationListener
{
  public:
    MockedActivationListener() = default;
    MockedActivationListener(const MockedActivationListener&) = delete;
    MockedActivationListener&
        operator=(const MockedActivationListener&) = delete;
    MockedActivationListener(MockedActivationListener&&) = delete;
    MockedActivationListener& operator=(MockedActivationListener&&) = delete;

    ~MockedActivationListener() override = default;

    MOCK_METHOD2(onUpdateDone, void(const std::string& versionId,
                                    const std::string& psuInventoryPath));
};
