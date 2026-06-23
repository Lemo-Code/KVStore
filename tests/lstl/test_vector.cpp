#include <gtest/gtest.h>
#include <lstl/container/vector.h>

TEST(LstlVectorTest, PushPopBack) {
    lstl::vector<int> v;
    for (int i = 0; i < 100; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 100u);
    for (int i = 99; i >= 0; --i) {
        EXPECT_EQ(v.back(), i);
        v.pop_back();
    }
    EXPECT_TRUE(v.empty());
}

TEST(LstlVectorTest, ReserveAndCapacity) {
    lstl::vector<int> v;
    v.reserve(50);
    EXPECT_GE(v.capacity(), 50u);
    v.push_back(1);
    EXPECT_EQ(v[0], 1);
}

TEST(LstlVectorTest, InsertErase) {
    lstl::vector<int> v{1, 2, 3};
    v.insert(v.begin() + 1, 99);
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[1], 99);
    v.erase(v.begin() + 1);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(LstlVectorTest, InitializerList) {
    lstl::vector<std::string> v{"a", "b", "c"};
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[2], "c");
}

TEST(LstlVectorTest, MoveSemantics) {
    lstl::vector<std::string> a;
    a.push_back("hello");
    lstl::vector<std::string> b = std::move(a);
    EXPECT_EQ(b[0], "hello");
    EXPECT_TRUE(a.empty());
}
