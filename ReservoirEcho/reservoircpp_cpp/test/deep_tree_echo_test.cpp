#include <gtest/gtest.h>
#include <stdexcept>
#include "deep_tree_echo/core.hpp"

using namespace deep_tree_echo;

TEST(AvatarInterfaceTest, RegisteredToolRuns) {
    AvatarInterface avatar;
    avatar.register_tool("double", [](const Vector& v) { return Vector(v * 2.0); });
    const Vector out = avatar.use_tool("double", Vector::Ones(3));
    EXPECT_DOUBLE_EQ(out(0), 2.0);
    EXPECT_EQ(out.size(), 3);
}

TEST(AvatarInterfaceTest, UnregisteredToolThrowsInsteadOfEchoingInput) {
    AvatarInterface avatar;
    EXPECT_THROW(avatar.use_tool("missing", Vector::Ones(3)), std::out_of_range);
}
