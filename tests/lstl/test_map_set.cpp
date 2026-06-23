#include <gtest/gtest.h>
#include <lstl/container/map.h>
#include <lstl/container/set.h>
#include <lstl/container/bmap.h>

TEST(LstlMapSetTest, MapInsertFind) {
    lstl::map<std::string, int> m;
    m.insert(lstl::make_pair(std::string("foo"), 1));
    m["bar"] = 2;
    EXPECT_EQ(m["foo"], 1);
    EXPECT_EQ(m["bar"], 2);
    EXPECT_EQ(m.size(), 2u);
}

TEST(LstlMapSetTest, SetUniqueElements) {
    lstl::set<int> s;
    s.insert(3);
    s.insert(1);
    s.insert(3);
    EXPECT_EQ(s.size(), 2u);
    auto it = s.begin();
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it, 3);
}

TEST(LstlMapSetTest, BMapOrdered) {
    lstl::bmap<int, std::string> bm;
    bm[30] = "thirty";
    bm[10] = "ten";
    bm[20] = "twenty";
    auto it = bm.begin();
    EXPECT_EQ(it->first, 10);
    ++it;
    EXPECT_EQ(it->first, 20);
}

TEST(LstlMapSetTest, MapErase) {
    lstl::map<int, int> m;
    m[1] = 10;
    m[2] = 20;
    m.erase(1);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.find(2)->second, 20);
}
