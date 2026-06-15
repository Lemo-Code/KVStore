/**
 * @file    test_unordered.cpp
 * @brief   Comprehensive tests for unordered_map, unordered_set, and multi variants.
 */

#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>
#include <lstl/container/unordered_multimap.h>
#include <lstl/container/unordered_multiset.h>
#include <cassert>
#include <string>

int main() {
    using namespace lstl;

    // =========================================================================
    // unordered_map basic
    // =========================================================================
    {
        unordered_map<int, std::string> um;
        assert(um.empty());
        assert(um.size() == 0);

        um.insert(lstl::make_pair(1, std::string("one")));
        um.insert(lstl::make_pair(2, std::string("two")));
        um[3] = "three";
        assert(um.size() == 3);

        assert(um.find(2) != um.end());
        assert(um.find(2)->second == "two");
        assert(um.find(99) == um.end());

        assert(um[1] == "one");
        assert(um.count(2) == 1);
        assert(um.count(99) == 0);
    }

    // =========================================================================
    // unordered_map duplicate / erase
    // =========================================================================
    {
        unordered_map<int, int> um;
        auto r = um.insert(lstl::make_pair(1, 100));
        assert(r.second);
        r = um.insert(lstl::make_pair(1, 999));
        assert(!r.second);  // not inserted
        assert(um[1] == 100);  // original value

        assert(um.erase(1) == 1);
        assert(um.size() == 0);
        assert(um.erase(99) == 0);
    }

    // =========================================================================
    // unordered_map — many insertions
    // =========================================================================
    {
        unordered_map<int, int> um;
        for (int i = 0; i < 500; ++i) um.insert(lstl::make_pair(i, i * 10));
        assert(um.size() == 500);
        assert(um.find(250)->second == 2500);
    }

    // =========================================================================
    // unordered_set basic
    // =========================================================================
    {
        unordered_set<int> us;
        us.insert(5); us.insert(3); us.insert(8);
        assert(us.size() == 3);
        assert(us.find(3) != us.end());
        assert(us.find(99) == us.end());
        assert(us.count(5) == 1);

        us.erase(3);
        assert(us.size() == 2);
        assert(us.find(3) == us.end());

        us.clear();
        assert(us.empty());
    }

    // =========================================================================
    // unordered_set — duplicates rejected
    // =========================================================================
    {
        unordered_set<int> us;
        auto r1 = us.insert(42);
        assert(r1.second);
        auto r2 = us.insert(42);
        assert(!r2.second);
        assert(us.size() == 1);
    }

    // =========================================================================
    // unordered_multimap — duplicate keys
    // =========================================================================
    {
        unordered_multimap<int, int> umm;
        umm.insert(lstl::make_pair(1, 10));
        umm.insert(lstl::make_pair(1, 11));
        umm.insert(lstl::make_pair(2, 20));
        assert(umm.size() == 3);
        assert(umm.count(1) == 2);

        // Erase all with key 1
        assert(umm.erase(1) == 2);
        assert(umm.size() == 1);
        assert(umm.count(1) == 0);
    }

    // =========================================================================
    // unordered_multiset — duplicate keys
    // =========================================================================
    {
        unordered_multiset<int> ums;
        ums.insert(1); ums.insert(1); ums.insert(2);
        assert(ums.size() == 3);
        assert(ums.count(1) == 2);
        assert(ums.erase(1) == 2);
        assert(ums.size() == 1);
    }

    // =========================================================================
    // unordered_map — at() with bounds check
    // =========================================================================
    {
        unordered_map<int, int> um;
        um[1] = 100;
        assert(um.at(1) == 100);

        bool caught = false;
        try { um.at(99); } catch (const std::out_of_range&) { caught = true; }
        assert(caught);
    }

    // =========================================================================
    // unordered containers — iterator traversal
    // =========================================================================
    {
        unordered_set<int> us;
        us.insert(1); us.insert(2); us.insert(3);
        int sum = 0;
        for (auto& x : us) sum += x;
        assert(sum == 6);  // 1+2+3
    }

    return 0;
}
