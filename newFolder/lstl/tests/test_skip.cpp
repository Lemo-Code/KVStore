/**
 * @file    test_skip.cpp
 * @brief   Comprehensive tests for skip_map and skip_set.
 */

#include <lstl/container/skip_map.h>
#include <lstl/container/skip_set.h>
#include <cassert>

int main() {
    using namespace lstl;

    // =========================================================================
    // skip_map basic
    // =========================================================================
    {
        skip_map<int, int> sm;
        assert(sm.empty());

        sm.insert(lstl::make_pair(1, 100));
        sm.insert(lstl::make_pair(2, 200));
        sm.insert(lstl::make_pair(3, 300));
        assert(sm.size() == 3);

        auto it = sm.find(2);
        assert(it != sm.end());
        assert(it->second == 200);

        assert(sm.find(99) == sm.end());
    }

    // =========================================================================
    // skip_map — operator[]
    // =========================================================================
    {
        skip_map<int, int> sm;
        sm[4] = 400;
        assert(sm[4] == 400);
        assert(sm.size() == 1);

        sm[4] = 444;  // overwrite
        assert(sm[4] == 444);
    }

    // =========================================================================
    // skip_map — erase
    // =========================================================================
    {
        skip_map<int, int> sm;
        sm.insert(lstl::make_pair(1, 100));
        sm.insert(lstl::make_pair(2, 200));
        assert(sm.erase(1) == 1);
        assert(sm.size() == 1);
        assert(sm.find(1) == sm.end());
        assert(sm.erase(99) == 0);
    }

    // =========================================================================
    // skip_map — sorted iteration
    // =========================================================================
    {
        skip_map<int, int> sm;
        sm.insert(lstl::make_pair(5, 50));
        sm.insert(lstl::make_pair(3, 30));
        sm.insert(lstl::make_pair(8, 80));
        sm.insert(lstl::make_pair(1, 10));

        int expected[] = {1, 3, 5, 8};
        int idx = 0;
        for (auto& p : sm) {
            assert(p.first == expected[idx++]);
        }
        assert(idx == 4);
    }

    // =========================================================================
    // skip_set basic
    // =========================================================================
    {
        skip_set<int> ss;
        ss.insert(5); ss.insert(3); ss.insert(8); ss.insert(1);
        assert(ss.size() == 4);
        assert(ss.find(3) != ss.end());

        auto r = ss.insert(3);  // duplicate
        assert(!r.second);

        ss.erase(3);
        assert(ss.size() == 3);
    }

    // =========================================================================
    // skip containers — stress
    // =========================================================================
    {
        skip_map<int, int> sm;
        for (int i = 0; i < 500; ++i) sm.insert(lstl::make_pair(i, i * 10));
        assert(sm.size() == 500);

        // Ordered iteration check
        int prev = -1;
        for (auto& p : sm) {
            assert(p.first > prev);
            prev = p.first;
        }

        // Erase half
        for (int i = 0; i < 250; ++i) sm.erase(i);
        assert(sm.size() == 250);
    }

    // =========================================================================
    // skip_set — clear and reinsert
    // =========================================================================
    {
        skip_set<int> ss;
        ss.insert(1); ss.insert(2);
        ss.clear();
        assert(ss.empty());
        ss.insert(3);
        assert(ss.size() == 1);
    }

    return 0;
}
