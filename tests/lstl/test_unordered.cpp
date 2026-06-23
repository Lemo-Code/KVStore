#include <gtest/gtest.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>

TEST(LstlUnorderedTest, MapLookup) {
    lstl::unordered_map<std::string, int> m;
    m["alpha"] = 1;
    m["beta"] = 2;
    EXPECT_EQ(m["alpha"], 1);
    EXPECT_EQ(m.count("gamma"), 0u);
}

TEST(LstlUnorderedTest, SetMembership) {
    lstl::unordered_set<std::string> s;
    s.insert("x");
    s.insert("y");
    EXPECT_TRUE(s.count("x") > 0);
    EXPECT_EQ(s.count("z"), 0u);
}

TEST(LstlUnorderedTest, RehashGrowth) {
    lstl::unordered_map<int, int> m;
    for (int i = 0; i < 1000; ++i)
        m[i] = i * 2;
    EXPECT_EQ(m.size(), 1000u);
    for (int i = 0; i < 1000; ++i)
        EXPECT_EQ(m[i], i * 2);
}
