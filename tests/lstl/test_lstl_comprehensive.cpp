#include <gtest/gtest.h>
#include <lstl/container/vector.h>
#include <lstl/container/deque.h>
#include <lstl/container/list.h>
#include <lstl/container/map.h>
#include <lstl/container/set.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>
#include <lstl/container/unordered_multimap.h>
#include <lstl/container/unordered_multiset.h>
#include <lstl/memory/construct.h>
#include <lstl/memory/uninitialized.h>
#include <lstl/memory/allocator.h>
#include <lstl/memory/utility.h>
#include <stdexcept>
#include <string>

struct Counted {
    int v = 0;
    explicit Counted(int x = 0) : v(x) {}
    Counted(const Counted& o) : v(o.v) {}
    Counted(Counted&& o) noexcept : v(o.v) { o.v = -1; }
    Counted& operator=(const Counted& o) { v = o.v; return *this; }
    bool operator==(const Counted& o) const { return v == o.v; }
};

TEST(LstlComprehensiveTest, VectorFullApi) {
    lstl::vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);

    lstl::vector<int> sized(5, 42);
    EXPECT_EQ(sized.size(), 5u);
    for (size_t i = 0; i < sized.size(); ++i) EXPECT_EQ(sized[i], 42);

    lstl::vector<int> init = {1, 2, 3, 4, 5};
    lstl::vector<int> copied(init);
    EXPECT_EQ(copied[0], 1);
    init[0] = 99;
    EXPECT_EQ(copied[0], 1);

    lstl::vector<int> moved(lstl::move(copied));
    EXPECT_EQ(moved.size(), 5u);
    EXPECT_TRUE(copied.empty());

    for (int i = 0; i < 1000; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 1000u);
    EXPECT_EQ(v.front(), 0);
    EXPECT_EQ(v.back(), 999);
    v.pop_back();
    EXPECT_EQ(v.size(), 999u);

    size_t cap = v.capacity();
    v.reserve(cap + 50);
    EXPECT_GE(v.capacity(), cap + 50);
    v.reserve(10);
    EXPECT_GE(v.capacity(), cap + 50);

    v.resize(1005, 7);
    EXPECT_EQ(v.size(), 1005u);
    EXPECT_EQ(v[1000], 7);
    v.resize(500);
    EXPECT_EQ(v.size(), 500u);

    v.insert(v.begin(), 0);
    v.insert(v.begin() + 1, 1);
    v.insert(v.end(), 999);
    v.erase(v.begin());
    v.erase(v.end() - 1);

    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
    for (auto x : v) (void)x;
    sum = 0;
    for (auto it = v.rbegin(); it != v.rend(); ++it) sum += *it;

    EXPECT_EQ(v.at(0), v[0]);
    EXPECT_THROW(v.at(v.size() + 1), std::out_of_range);

    v.shrink_to_fit();
    lstl::vector<int> assign_src = {10, 20};
    v = assign_src;
    EXPECT_EQ(v.size(), 2u);
    v = lstl::move(assign_src);
    EXPECT_EQ(v.size(), 2u);

    lstl::vector<std::string> sv;
    sv.push_back("hello");
    sv.push_back("world");
    lstl::vector<std::string> sv2(sv);
    EXPECT_EQ(sv2[1], "world");

    const lstl::vector<int> cv = {1, 2, 3};
    EXPECT_EQ(cv.front(), 1);
    EXPECT_EQ(cv.back(), 3);
    EXPECT_EQ(cv.data()[0], 1);

    v.clear();
    EXPECT_TRUE(v.empty());
}

TEST(LstlComprehensiveTest, UnorderedContainersFull) {
    lstl::unordered_map<int, std::string> um;
    um.insert(lstl::make_pair(1, std::string("one")));
    um[3] = "three";
    EXPECT_EQ(um.find(1)->second, "one");
    EXPECT_EQ(um.find(99), um.end());
    EXPECT_EQ(um.count(1), 1u);
    EXPECT_EQ(um.erase(1), 1u);
    EXPECT_EQ(um.erase(99), 0u);

    for (int i = 0; i < 500; ++i) um[i] = "v" + std::to_string(i);
    EXPECT_EQ(um.size(), 500u);
    EXPECT_EQ(um.at(250), "v250");
    EXPECT_THROW(um.at(9999), std::out_of_range);

    lstl::unordered_set<int> us;
    EXPECT_TRUE(us.insert(5).second);
    EXPECT_FALSE(us.insert(5).second);
    us.erase(5);
    us.clear();
    EXPECT_TRUE(us.empty());

    lstl::unordered_multimap<int, int> umm;
    umm.insert(lstl::make_pair(1, 10));
    umm.insert(lstl::make_pair(1, 11));
    EXPECT_EQ(umm.count(1), 2u);
    EXPECT_EQ(umm.erase(1), 2u);

    lstl::unordered_multiset<int> ums;
    ums.insert(1);
    ums.insert(1);
    EXPECT_EQ(ums.count(1), 2u);
    EXPECT_EQ(ums.erase(1), 2u);

    int s = 0;
    for (auto& x : us) (void)x;
    for (int i = 0; i < 200; ++i) us.insert(i);
    for (auto& x : us) s += x;
    EXPECT_GT(s, 0);
}

TEST(LstlComprehensiveTest, DequeAndListFull) {
    lstl::deque<int> dq;
    for (int i = 0; i < 50; ++i) dq.push_back(i);
    dq.push_front(-1);
    EXPECT_EQ(dq.front(), -1);
    dq.pop_front();
    dq.pop_back();
    for (int i = 0; i < 49; ++i) EXPECT_EQ(dq[i], i);
    int dsum = 0;
    for (auto& x : dq) dsum += x;
    dq.clear();
    EXPECT_TRUE(dq.empty());

    lstl::list<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_front(0);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(lst.front(), 0);
    EXPECT_EQ(lst.back(), 2);
    lst.pop_front();
    lst.pop_back();
    lst.insert(lst.begin(), -1);
    lst.erase(lst.begin());
    EXPECT_EQ(lst.size(), 1u);
    EXPECT_EQ(lst.front(), 1);
    lst.clear();
    EXPECT_TRUE(lst.empty());
}

TEST(LstlComprehensiveTest, MapSetRbTree) {
    lstl::map<int, std::string> m;
    m[30] = "c";
    m[10] = "a";
    m[20] = "b";
    auto it = m.begin();
    EXPECT_EQ(it->first, 10);
    ++it;
    EXPECT_EQ(it->first, 20);
    m.erase(10);
    EXPECT_EQ(m.size(), 2u);

    lstl::set<int> s;
    s.insert(3);
    s.insert(1);
    s.insert(3);
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ(*s.begin(), 1);
    s.erase(1);
    EXPECT_EQ(s.size(), 1u);
}

TEST(LstlComprehensiveTest, ConstructAllocatorUtility) {
    lstl::allocator<Counted> alloc;
    Counted* p = alloc.allocate(3);
    lstl::uninitialized_fill(p, p + 3, Counted(5));
    for (int i = 0; i < 3; ++i) EXPECT_EQ(p[i].v, 5);
    lstl::destroy(p, p + 3);
    alloc.deallocate(p, 3);

    lstl::allocator<int> ia;
    int* buf = ia.allocate(8);
    lstl::construct(buf, 1);
    lstl::construct(buf + 1, 2);
    EXPECT_EQ(buf[0], 1);
    lstl::destroy(buf);
    lstl::destroy(buf + 1);
    ia.deallocate(buf, 8);

    int a = 1, b = 2;
    lstl::swap(a, b);
    EXPECT_EQ(a, 2);
    auto pr = lstl::make_pair(1, 2);
    EXPECT_EQ(pr.first, 1);

    lstl::vector<int, lstl::allocator<int>> v;
    for (int i = 0; i < 200; ++i) v.push_back(i);
    v.insert(v.begin() + 50, 999);
    v.erase(v.begin() + 50);
    EXPECT_EQ(v.size(), 200u);
}
