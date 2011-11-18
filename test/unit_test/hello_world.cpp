int HelloWorld(int a)
{
    return a;
}

#include <gtest/gtest.h>

TEST(HelloWorld, Hello)
{
    EXPECT_EQ(0, HelloWorld(0));
    EXPECT_EQ(2, HelloWorld(1));
}

int main(int argc, char ** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
