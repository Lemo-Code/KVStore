#include <gtest/gtest.h>
#include <lstl/container/vector.h>
#include <lstl/container/list.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/multimap.h>
#include <lstl/container/bset.h>
#include <lstl/memory/uninitialized.h>
#include <lstl/memory/utility.h>

TEST(LstlExtendedTest, VectorDeep) {
    lstl::vector<int> v;
    v.reserve(100);
    for (int i = 0; i < 500; ++i) v.push_back(i);
    v.insert(v.begin() + 10, 999);
    v.erase(v.begin() + 10);
    v.shrink_to_fit();
    EXPECT_EQ(v.size(), 500u);
    v.clear();
    EXPECT_TRUE(v.empty());
}

TEST(LstlExtendedTest, ListDeep) {
    lstl::list<std::string> lst;
    for (int i = 0; i < 100; ++i) lst.push_back("s" + std::to_string(i));
    lst.remove("s50");
    EXPECT_EQ(lst.size(), 99u);
}

TEST(LstlExtendedTest, UnorderedMapDeep) {
    lstl::unordered_map<int, std::string> m;
    for (int i = 0; i < 500; ++i) m[i] = "v";
    m.erase(10);
    EXPECT_EQ(m.size(), 499u);
}

TEST(LstlExtendedTest, MultimapAndBset) {
    lstl::multimap<int, int> mm;
    mm.insert(lstl::make_pair(1, 10));
    mm.insert(lstl::make_pair(1, 20));
    EXPECT_EQ(mm.count(1), 2u);
    lstl::bset<int> bs;
    bs.insert(3);
    bs.insert(1);
    EXPECT_EQ(*bs.begin(), 1);
}

TEST(LstlExtendedTest, UninitializedCopy) {
    lstl::allocator<int> a;
    int* buf = a.allocate(10);
    lstl::uninitialized_fill(buf, buf + 10, 42);
    for (int i = 0; i < 10; ++i) EXPECT_EQ(buf[i], 42);
    lstl::destroy(buf, buf + 10);
    a.deallocate(buf, 10);
}

TEST(LstlExtendedTest, PairAndSwap) {
    auto p = lstl::make_pair(1, std::string("a"));
    EXPECT_EQ(p.first, 1);
    int x = 1, y = 2;
    lstl::swap(x, y);
    EXPECT_EQ(x, 2);
}
