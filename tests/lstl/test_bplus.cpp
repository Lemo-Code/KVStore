#include <gtest/gtest.h>
#include <lstl/container/bmap.h>

TEST(LstlBplusTest, BMapInsertFind) {
    lstl::bmap<int, int> tree;
    for (int i = 0; i < 100; ++i) tree[i] = i * 10;
    EXPECT_EQ(tree[42], 420);
    EXPECT_EQ(tree.size(), 100u);
}

TEST(LstlBplusTest, BMapErase) {
    lstl::bmap<int, std::string> tree;
    tree[1] = "one";
    tree[2] = "two";
    tree.erase(1);
    EXPECT_EQ(tree.size(), 1u);
    EXPECT_EQ(tree.find(2)->second, "two");
}
