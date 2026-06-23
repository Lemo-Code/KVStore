#include <gtest/gtest.h>
#include <lstl/container/vector.h>
#include <lstl/container/list.h>
#include <lstl/container/deque.h>
#include <lstl/container/map.h>
#include <lstl/container/set.h>
#include <lstl/container/multimap.h>
#include <lstl/container/multiset.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>
#include <lstl/container/unordered_multimap.h>
#include <lstl/container/unordered_multiset.h>
#include <lstl/container/stack.h>
#include <lstl/container/queue.h>
#include <lstl/container/priority_queue.h>
#include <lstl/container/slist.h>
#include <lstl/container/skip_map.h>
#include <lstl/container/skip_set.h>
#include <lstl/container/bmap.h>
#include <lstl/container/bset.h>
#include <lstl/memory/construct.h>
#include <lstl/memory/uninitialized.h>
#include <lstl/memory/allocator.h>
#include <lstl/memory/pool.h>
#include <lstl/memory/utility.h>
#include <lstl/memory/type_traits.h>

// lstl 公开容器 / 内存 API 冒烟 — 供 api_coverage.py 扫描

TEST(LstlApiCoverageTest, AllContainersConstructAndMutate) {
    lstl::vector<int> v{1, 2, 3};
    v.reserve(10);
    v.shrink_to_fit();
    EXPECT_EQ(v.front(), 1);
    EXPECT_EQ(v.back(), 3);

    lstl::list<int> l{1, 2};
    l.splice(l.end(), l, l.begin());
    EXPECT_EQ(l.size(), 2u);

    lstl::deque<int> d;
    d.push_front(1);
    d.push_back(2);
    EXPECT_EQ(d[0], 1);

    lstl::map<int, int> m;
    m.insert(lstl::make_pair(1, 10));
    EXPECT_EQ(m.find(1)->second, 10);

    lstl::set<int> s;
    s.insert(7);
    EXPECT_TRUE(s.count(7));

    lstl::multimap<int, int> mm;
    mm.insert(lstl::make_pair(1, 1));
    mm.insert(lstl::make_pair(1, 2));
    EXPECT_EQ(mm.count(1), 2u);

    lstl::multiset<int> ms;
    ms.insert(1);
    ms.insert(1);
    EXPECT_EQ(ms.size(), 2u);

    lstl::unordered_map<int, int> um;
    um[1] = 2;
    EXPECT_EQ(um[1], 2);

    lstl::unordered_set<int> us;
    us.insert(3);
    EXPECT_TRUE(us.find(3) != us.end());

    lstl::unordered_multimap<int, int> umm;
    umm.insert(lstl::make_pair(1, 9));
    EXPECT_EQ(umm.count(1), 1u);

    lstl::unordered_multiset<int> ums;
    ums.insert(4);
    EXPECT_EQ(ums.count(4), 1u);

    lstl::stack<int> st;
    st.push(1);
    st.pop();

    lstl::queue<int> q;
    q.push(1);
    q.pop();

    lstl::priority_queue<int> pq;
    pq.push(5);
    pq.pop();

    lstl::slist<int> sl;
    sl.push_front(1);
    sl.pop_front();

    lstl::skip_map<int, int> skm;
    skm[1] = 2;

    lstl::skip_set<int> sks;
    sks.insert(8);

    lstl::bmap<int, int> bm;
    bm[1] = 2;

    lstl::bset<int> bs;
    bs.insert(6);
}

TEST(LstlApiCoverageTest, MemoryAndTypeTraitsApi) {
    EXPECT_TRUE((lstl::is_same<int, int>::value));
    EXPECT_TRUE((lstl::is_integral<int>::value));

    lstl::allocator<int> alloc;
    int* p = alloc.allocate(4);
    lstl::construct(p, 42);
    lstl::destroy(p);
    alloc.deallocate(p, 4);

    int* raw = alloc.allocate(3);
    lstl::uninitialized_fill(raw, raw + 3, 7);
    lstl::destroy(raw, raw + 3);
    alloc.deallocate(raw, 3);

    lstl::vector<int> src{1, 2, 3};
    int dst[5] = {};
    lstl::uninitialized_copy(src.begin(), src.end(), dst);
    EXPECT_EQ(dst[0], 1);
    lstl::destroy(dst, dst + 3);

    int a = 1, b = 2;
    lstl::swap(a, b);
    auto pr = lstl::make_pair(1, 2);
    EXPECT_EQ(pr.first, 1);

    lstl::pool_single pool(64, 4);
    void* blk = pool.allocate();
    pool.deallocate(blk);
}
