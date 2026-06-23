#include <gtest/gtest.h>
#include <lstl/container/skip_map.h>
#include <lstl/container/skip_set.h>

TEST(LstlSkipTest, SkipMapOrdered) {
    lstl::skip_map<int, std::string> sm;
    sm[30] = "c";
    sm[10] = "a";
    sm[20] = "b";
    auto it = sm.begin();
    EXPECT_EQ(it->first, 10);
    ++it;
    EXPECT_EQ(it->first, 20);
}

TEST(LstlSkipTest, SkipSetUnique) {
    lstl::skip_set<int> ss;
    ss.insert(5);
    ss.insert(3);
    ss.insert(5);
    EXPECT_EQ(ss.size(), 2u);
}
