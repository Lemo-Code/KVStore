#include <gtest/gtest.h>
#include <lstl/container/vector.h>
#include <lstl/container/list.h>
#include <lstl/container/deque.h>
#include <lstl/container/map.h>
#include <lstl/container/set.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>
#include <lstl/container/multimap.h>
#include <lstl/container/multiset.h>
#include <lstl/container/bmap.h>
#include <lstl/container/bset.h>
#include <lstl/container/skip_map.h>
#include <lstl/container/skip_set.h>
#include <lstl/container/slist.h>
#include <lstl/memory/construct.h>
#include <lstl/memory/uninitialized.h>
#include <lstl/memory/allocator.h>
#include <lstl/memory/pool.h>

TEST(LstlExhaustiveTest, VectorStress) {
    lstl::vector<int> v;
    for (int i = 0; i < 2000; ++i) v.push_back(i);
    v.insert(v.begin() + 500, 9999);
    v.erase(v.begin() + 500);
    v.resize(100);
    v.reserve(5000);
    for (int i = 0; i < 3000; ++i) v.push_back(i);
    v.pop_back();
    v.clear();
    for (int i = 0; i < 100; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 100u);
}

TEST(LstlExhaustiveTest, ListStress) {
    lstl::list<int> l;
    for (int i = 0; i < 500; ++i) l.push_back(i);
    for (int i = 0; i < 250; ++i) l.pop_front();
    l.remove(300);
    l.reverse();
    EXPECT_GT(l.size(), 0u);
}

TEST(LstlExhaustiveTest, DequeStress) {
    lstl::deque<int> d;
    for (int i = 0; i < 1000; ++i) {
        if (i % 2) d.push_back(i); else d.push_front(i);
    }
    while (d.size() > 100) d.pop_front();
    EXPECT_EQ(d.size(), 100u);
}

TEST(LstlExhaustiveTest, MapSetStress) {
    lstl::map<int, std::string> m;
    for (int i = 0; i < 500; ++i) m[i] = "v" + std::to_string(i);
    for (int i = 0; i < 250; ++i) m.erase(i);
    lstl::set<int> s;
    for (int i = 0; i < 500; ++i) s.insert(i);
    EXPECT_EQ(s.size(), 500u);
}

TEST(LstlExhaustiveTest, UnorderedStress) {
    lstl::unordered_map<int, int> um;
    for (int i = 0; i < 3000; ++i) um[i] = i * 2;
    for (int i = 0; i < 1500; ++i) um.erase(i);
    lstl::unordered_set<std::string> us;
    for (int i = 0; i < 1000; ++i) us.insert("k" + std::to_string(i));
    EXPECT_EQ(us.size(), 1000u);
}

TEST(LstlExhaustiveTest, MultiContainers) {
    lstl::multimap<int, int> mm;
    for (int i = 0; i < 100; ++i) mm.insert(lstl::make_pair(i % 10, i));
    lstl::multiset<int> ms;
    for (int i = 0; i < 100; ++i) ms.insert(i % 20);
    EXPECT_GT(mm.size(), 0u);
}

TEST(LstlExhaustiveTest, BTreeContainers) {
    lstl::bmap<int, std::string> bm;
    for (int i = 0; i < 50; ++i) bm[i] = std::to_string(i);
    EXPECT_EQ(bm.find(25)->second, "25");
}

TEST(LstlExhaustiveTest, SkipListContainers) {
    lstl::skip_map<int, int> sm;
    for (int i = 0; i < 200; ++i) sm[i] = i;
    lstl::skip_set<int> ss;
    for (int i = 0; i < 150; ++i) ss.insert(i);
    EXPECT_EQ(ss.size(), 150u);
}

TEST(LstlExhaustiveTest, SlistStress) {
    lstl::slist<int> sl;
    for (int i = 0; i < 200; ++i) sl.push_front(i);
    EXPECT_EQ(sl.size(), 200u);
}

TEST(LstlExhaustiveTest, MemoryConstruct) {
    lstl::allocator<std::string> alloc;
    std::string* p = alloc.allocate(5);
    lstl::uninitialized_fill(p, p + 5, std::string("x"));
    for (int i = 0; i < 5; ++i) EXPECT_EQ(p[i], "x");
    lstl::destroy(p, p + 5);
    alloc.deallocate(p, 5);
    lstl::pool_single pool(128, 8);
    void* a = pool.allocate();
    void* b = pool.allocate();
    pool.deallocate(a);
    pool.deallocate(b);
}

TEST(LstlExhaustiveTest, VectorString) {
    lstl::vector<std::string> vs;
    for (int i = 0; i < 500; ++i) vs.push_back(std::string(10, 'a' + (i % 26)));
    vs.erase(vs.begin() + 50);
    EXPECT_EQ(vs.size(), 499u);
}
