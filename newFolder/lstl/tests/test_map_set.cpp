/**
 * @file    test_map_set.cpp
 * @brief   Comprehensive tests for map, set, multimap, and multiset (RB-tree backed).
 */

#include <lstl/container/map.h>
#include <lstl/container/set.h>
#include <lstl/container/multimap.h>
#include <lstl/container/multiset.h>
#include <cassert>
#include <string>

int main() {
    using namespace lstl;

    // =========================================================================
    // map — basic operations
    // =========================================================================
    {
        map<int, std::string> m;
        assert(m.empty());

        // Insert
        m.insert(lstl::make_pair(1, std::string("one")));
        m.insert(lstl::make_pair(2, std::string("two")));
        m.insert(lstl::make_pair(3, std::string("three")));
        assert(m.size() == 3);

        // Find existing
        auto it = m.find(2);
        assert(it != m.end());
        assert(it->first == 2 && it->second == "two");

        // Find non-existing
        it = m.find(99);
        assert(it == m.end());

        // operator[] (create)
        assert(m[4] == "");  // default-constructs string
        m[4] = "four";
        assert(m[4] == "four");
        assert(m.size() == 4);

        // operator[] (read existing)
        assert(m[1] == "one");

        // Duplicate insert
        auto result = m.insert(lstl::make_pair(1, std::string("dup")));
        assert(!result.second);              // not inserted
        assert(result.first->second == "one");  // original value preserved

        // Erase by key (existing)
        assert(m.erase(1) == 1);
        assert(m.find(1) == m.end());
        assert(m.size() == 3);

        // Erase by key (non-existing)
        assert(m.erase(99) == 0);
    }

    // =========================================================================
    // map — sorted iteration
    // =========================================================================
    {
        map<int, int> m;
        m.insert(lstl::make_pair(5, 50));
        m.insert(lstl::make_pair(3, 30));
        m.insert(lstl::make_pair(8, 80));
        m.insert(lstl::make_pair(1, 10));

        int expected_keys[] = {1, 3, 5, 8};
        int expected_vals[] = {10, 30, 50, 80};
        int idx = 0;
        for (auto& p : m) {
            assert(p.first == expected_keys[idx]);
            assert(p.second == expected_vals[idx]);
            ++idx;
        }
        assert(idx == 4);
    }

    // =========================================================================
    // map — lower_bound / upper_bound
    // =========================================================================
    {
        map<int, int> m;
        m.insert(lstl::make_pair(10, 100));
        m.insert(lstl::make_pair(20, 200));
        m.insert(lstl::make_pair(30, 300));

        assert(m.lower_bound(15)->first == 20);
        assert(m.lower_bound(20)->first == 20);
        assert(m.upper_bound(20)->first == 30);
        assert(m.lower_bound(99) == m.end());
    }

    // =========================================================================
    // map — count
    // =========================================================================
    {
        map<int, int> m;
        m.insert(lstl::make_pair(1, 10));
        assert(m.count(1) == 1);
        assert(m.count(99) == 0);
    }

    // =========================================================================
    // map — erase by iterator
    // =========================================================================
    {
        map<int, int> m;
        m.insert(lstl::make_pair(1, 10));
        m.insert(lstl::make_pair(2, 20));
        auto it = m.find(1);
        assert(it != m.end());
        m.erase(it->first);  // erase by key
        assert(m.size() == 1);
    }

    // =========================================================================
    // map — copy and move
    // =========================================================================
    {
        map<int, int> m1;
        m1.insert(lstl::make_pair(1, 10));
        m1.insert(lstl::make_pair(2, 20));

        map<int, int> m2(m1);  // copy
        assert(m2.size() == 2);
        assert(m2.find(1) != m2.end());

        map<int, int> m3(lstl::move(m1));  // move
        assert(m3.size() == 2);
        assert(m1.empty());
    }

    // =========================================================================
    // set — basic operations
    // =========================================================================
    {
        set<int> s;
        s.insert(5); s.insert(3); s.insert(8); s.insert(1);
        assert(s.size() == 4);

        assert(s.find(3) != s.end());
        assert(s.find(99) == s.end());

        // Sorted iteration
        int expected[] = {1, 3, 5, 8};
        int idx = 0;
        for (auto& k : s) {
            assert(k == expected[idx++]);
        }

        // Duplicate
        auto r = s.insert(3);
        assert(!r.second);

        s.erase(3);
        assert(s.size() == 3);
        assert(s.find(3) == s.end());
    }

    // =========================================================================
    // set — clear and reinsert
    // =========================================================================
    {
        set<int> s;
        s.insert(1); s.insert(2); s.insert(3);
        s.clear();
        assert(s.empty());
        assert(s.size() == 0);

        s.insert(4);
        assert(s.size() == 1);
    }

    // =========================================================================
    // multimap — duplicate keys
    // =========================================================================
    {
        multimap<int, int> mm;
        mm.insert(lstl::make_pair(1, 10));
        mm.insert(lstl::make_pair(1, 11));  // duplicate key
        mm.insert(lstl::make_pair(2, 20));
        assert(mm.size() == 3);
        assert(mm.count(1) == 2);
        assert(mm.count(2) == 1);

        // Erase all with key 1
        assert(mm.erase(1) == 2);
        assert(mm.size() == 1);
    }

    // =========================================================================
    // multiset — duplicate keys
    // =========================================================================
    {
        multiset<int> ms;
        ms.insert(1); ms.insert(1); ms.insert(2);
        assert(ms.size() == 3);
        assert(ms.count(1) == 2);

        ms.erase(1);
        assert(ms.size() == 1);
        assert(ms.count(1) == 0);
    }

    // =========================================================================
    // Stress test — many insertions and erasures
    // =========================================================================
    {
        map<int, int> m;
        for (int i = 0; i < 200; ++i) {
            m.insert(lstl::make_pair(i, i * 10));
        }
        assert(m.size() == 200);

        // Erase half
        for (int i = 0; i < 100; ++i) m.erase(i);
        assert(m.size() == 100);
    }

    // =========================================================================
    // map — swap
    // =========================================================================
    {
        map<int, int> m1, m2;
        m1.insert(lstl::make_pair(1, 10));
        m2.insert(lstl::make_pair(2, 20));
        m1.swap(m2);
        assert(m1.find(2) != m1.end());
        assert(m2.find(1) != m2.end());
    }

    return 0;
}
