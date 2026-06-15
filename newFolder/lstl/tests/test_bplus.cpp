/**
 * @file    test_bplus.cpp
 * @brief   Comprehensive tests for bmap and bset (B+ tree backed).
 */

#include <lstl/container/bmap.h>
#include <lstl/container/bset.h>
#include <cassert>

int main() {
    using namespace lstl;

    // =========================================================================
    // bmap basic
    // =========================================================================
    {
        bmap<int, int> bm;
        assert(bm.empty());

        bm.insert(lstl::make_pair(1, 100));
        bm.insert(lstl::make_pair(2, 200));
        bm.insert(lstl::make_pair(3, 300));
        assert(bm.size() == 3);

        auto it = bm.find(2);
        assert(it != bm.end());
        assert(it->second == 200);

        assert(bm.find(99) == bm.end());
    }

    // =========================================================================
    // bmap — operator[]
    // =========================================================================
    {
        bmap<int, int> bm;
        assert(bm[5] == 0);  // default construct
        bm[5] = 500;
        assert(bm[5] == 500);
        assert(bm.size() == 1);
    }

    // =========================================================================
    // bmap — erase
    // =========================================================================
    {
        bmap<int, int> bm;
        bm.insert(lstl::make_pair(1, 100));
        bm.insert(lstl::make_pair(2, 200));
        assert(bm.erase(1));
        assert(bm.size() == 1);
        assert(bm.find(1) == bm.end());
        assert(!bm.erase(99));  // non-existing
    }

    // =========================================================================
    // bmap — sorted iteration (linked leaf chain)
    // =========================================================================
    {
        bmap<int, int> bm;
        bm.insert(lstl::make_pair(5, 50));
        bm.insert(lstl::make_pair(3, 30));
        bm.insert(lstl::make_pair(8, 80));
        bm.insert(lstl::make_pair(1, 10));

        int expected[] = {1, 3, 5, 8};
        int idx = 0;
        for (auto& p : bm) {
            assert(p.first == expected[idx++]);
        }
        assert(idx == 4);
    }

    // =========================================================================
    // bset basic
    // =========================================================================
    {
        bset<int> bs;
        bs.insert(5); bs.insert(3); bs.insert(8); bs.insert(1);
        assert(bs.size() == 4);
        assert(bs.find(3) != bs.end());

        bs.erase(3);
        assert(bs.size() == 3);
        assert(bs.find(3) == bs.end());
    }

    // =========================================================================
    // bmap — multiple insertions (within single leaf capacity)
    // =========================================================================
    {
        bmap<int, int> bm;
        for (int i = 0; i < 50; ++i) {
            bm.insert(lstl::make_pair(i, i * 10));
        }
        assert(bm.size() == 50);
        assert(bm.find(25)->second == 250);

        // Erase some
        for (int i = 0; i < 25; ++i) bm.erase(i);
        assert(bm.size() == 25);
    }

    // =========================================================================
    // bset — clear and reinsert
    // =========================================================================
    {
        bset<int> bs;
        bs.insert(1); bs.insert(2);
        bs.clear();
        assert(bs.empty());
        bs.insert(3);
        assert(bs.size() == 1);
    }

    return 0;
}
