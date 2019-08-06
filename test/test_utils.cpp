#include "utils.hpp"

#include <gtest/gtest.h>

TEST(Utils, GetPSUInventoryPath)
{
    auto ret = utils::getPSUInventoryPath();
    ASSERT_EQ(2u, ret.size());
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/chassis/motherboard/"
              "powersupply0",
              ret[0]);
    EXPECT_EQ("/xyz/openbmc_project/inventory/system/chassis/motherboard/"
              "powersupply1",
              ret[1]);
}

TEST(Utils, GetVersionID)
{

    auto ret = utils::getVersionId("");
    EXPECT_EQ("", ret);

    ret = utils::getVersionId("some version");
    EXPECT_EQ(8u, ret.size());
}
