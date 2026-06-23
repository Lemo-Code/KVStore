/**
 * @file    test_vector.cpp
 * @brief   Comprehensive tests for lstl::vector.
 *
 * Covers: construction, element access, capacity management,
 * modifiers, iterators, edge cases, exception safety, and move semantics.
 */

#include <lstl/container/vector.h>
#include <cassert>
#include <string>
#include <vector>  // for comparison baseline

// Helper: count constructions/destructions
static int g_constructs = 0;
static int g_destructs = 0;

struct Counted {
    int v;
    Counted() : v(0) { ++g_constructs; }
    explicit Counted(int x) : v(x) { ++g_constructs; }
    Counted(const Counted& o) : v(o.v) { ++g_constructs; }
    Counted(Counted&& o) noexcept : v(o.v) { o.v = -1; ++g_constructs; }
    ~Counted() { ++g_destructs; }
    Counted& operator=(const Counted& o) { v = o.v; return *this; }
    bool operator==(const Counted& o) const { return v == o.v; }
};

int main() {
    using namespace lstl;

    // =========================================================================
    // 1. Default construction
    // =========================================================================
    {
        vector<int> v;
        assert(v.empty());
        assert(v.size() == 0);
        assert(v.capacity() == 0);
        assert(v.begin() == v.end());
    }

    // =========================================================================
    // 2. Sized construction
    // =========================================================================
    {
        vector<int> v(5, 42);
        assert(v.size() == 5);
        assert(v.capacity() >= 5);
        for (size_t i = 0; i < v.size(); ++i) assert(v[i] == 42);
    }

    {
        vector<int> v(3);  // defaults to 0
        assert(v.size() == 3);
        for (size_t i = 0; i < 3; ++i) assert(v[i] == 0);
    }

    // =========================================================================
    // 3. Initializer list construction
    // =========================================================================
    {
        vector<int> v = {1, 2, 3, 4, 5};
        assert(v.size() == 5);
        assert(v[0] == 1 && v[4] == 5);
    }

    // =========================================================================
    // 4. Copy construction
    // =========================================================================
    {
        vector<int> v1 = {10, 20, 30};
        vector<int> v2(v1);
        assert(v2.size() == 3);
        assert(v2[0] == 10 && v2[2] == 30);
        v1[0] = 99;  // verify deep copy
        assert(v2[0] == 10);
    }

    // =========================================================================
    // 5. Move construction
    // =========================================================================
    {
        vector<int> v1 = {1, 2, 3};
        vector<int> v2(lstl::move(v1));
        assert(v2.size() == 3);
        assert(v1.empty());  // source emptied
        assert(v1.size() == 0);
    }

    // =========================================================================
    // 6. push_back / pop_back
    // =========================================================================
    {
        vector<int> v;
        v.push_back(1); v.push_back(2); v.push_back(3);
        assert(v.size() == 3);
        assert(v[0] == 1 && v[1] == 2 && v[2] == 3);
        assert(v.front() == 1 && v.back() == 3);

        v.pop_back();
        assert(v.size() == 2);
        assert(v.back() == 2);

        // Push many elements to trigger multiple reallocations
        for (int i = 0; i < 1000; ++i) v.push_back(i);
        assert(v.size() == 1002);
        assert(v[0] == 1 && v[1] == 2);
        assert(v[1001] == 999);
    }

    // =========================================================================
    // 7. Reserve
    // =========================================================================
    {
        vector<int> v;
        v.reserve(100);
        assert(v.capacity() >= 100);
        assert(v.empty());  // reserve doesn't change size
        size_t cap_before = v.capacity();

        v.reserve(50);  // smaller reserve should be no-op
        assert(v.capacity() == cap_before);
    }

    // =========================================================================
    // 8. Resize
    // =========================================================================
    {
        vector<int> v = {1, 2, 3};
        v.resize(5, 99);
        assert(v.size() == 5);
        assert(v[0] == 1 && v[3] == 99 && v[4] == 99);

        v.resize(2);  // truncate
        assert(v.size() == 2);
        assert(v[0] == 1 && v[1] == 2);
    }

    // =========================================================================
    // 9. Insert (single element)
    // =========================================================================
    {
        vector<int> v = {1, 3, 4};
        v.insert(v.begin() + 1, 2);  // [1,2,3,4]
        assert(v.size() == 4);
        assert(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 4);

        v.insert(v.begin(), 0);  // [0,1,2,3,4]
        assert(v.size() == 5);
        assert(v[0] == 0);

        v.insert(v.end(), 5);  // [0,1,2,3,4,5]
        assert(v.size() == 6);
        assert(v[5] == 5);
    }

    // =========================================================================
    // 10. Erase
    // =========================================================================
    {
        vector<int> v = {1, 2, 3, 4, 5};
        v.erase(v.begin());  // remove first
        assert(v.size() == 4);
        assert(v[0] == 2);

        v.erase(v.begin() + 1);  // remove middle (3)
        assert(v.size() == 3);
        assert(v[0] == 2 && v[1] == 4);

        v.erase(v.end() - 1);  // remove last
        assert(v.size() == 2);
        assert(v[0] == 2 && v[1] == 4);
    }

    // =========================================================================
    // 11. Clear
    // =========================================================================
    {
        vector<int> v = {1, 2, 3};
        size_t cap = v.capacity();
        v.clear();
        assert(v.empty());
        assert(v.size() == 0);
        assert(v.capacity() == cap);  // capacity preserved
    }

    // =========================================================================
    // 12. Iterator operations
    // =========================================================================
    {
        vector<int> v = {10, 20, 30, 40, 50};

        // Forward iteration
        int sum = 0;
        for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
        assert(sum == 150);

        // Range-based for
        sum = 0;
        for (auto x : v) sum += x;
        assert(sum == 150);

        // Reverse iteration
        sum = 0;
        for (auto it = v.rbegin(); it != v.rend(); ++it) sum += *it;
        assert(sum == 150);

        // Random access
        auto it = v.begin();
        assert(*(it + 2) == 30);
        assert(*(it += 3) == 40);
        assert(*(it - 1) == 30);
        assert(it[1] == 50);
    }

    // =========================================================================
    // 13. at() with bounds checking
    // =========================================================================
    {
        vector<int> v = {1, 2, 3};
        assert(v.at(0) == 1);
        assert(v.at(2) == 3);

        bool caught = false;
        try { v.at(3); } catch (const std::out_of_range&) { caught = true; }
        assert(caught);

        caught = false;
        try { v.at(999); } catch (const std::out_of_range&) { caught = true; }
        assert(caught);
    }

    // =========================================================================
    // 14. shrink_to_fit
    // =========================================================================
    {
        vector<int> v = {1, 2, 3};
        v.reserve(100);
        assert(v.capacity() >= 100);
        v.shrink_to_fit();
        assert(v.capacity() == 3);
        assert(v.size() == 3);
    }

    // =========================================================================
    // 15. Copy assignment
    // =========================================================================
    {
        vector<int> v1 = {1, 2, 3};
        vector<int> v2 = {10, 20};
        v2 = v1;
        assert(v2.size() == 3);
        assert(v2[0] == 1 && v2[2] == 3);

        // Self assignment
        v2 = v2;
        assert(v2.size() == 3);
    }

    // =========================================================================
    // 16. Move assignment
    // =========================================================================
    {
        vector<int> v1 = {1, 2, 3};
        vector<int> v2;
        v2 = lstl::move(v1);
        assert(v2.size() == 3);
        assert(v1.empty());
    }

    // =========================================================================
    // 17. Empty vector edge cases
    // =========================================================================
    {
        vector<int> v;
        // pop_back on empty is UB, skip
        // front/back on empty is UB, skip
        // erase on empty
        // at() on empty
        bool caught = false;
        try { v.at(0); } catch (const std::out_of_range&) { caught = true; }
        assert(caught);
    }

    // =========================================================================
    // 18. Large dataset
    // =========================================================================
    {
        vector<int> v;
        for (int i = 0; i < 10000; ++i) v.push_back(i);
        assert(v.size() == 10000);
        for (int i = 0; i < 10000; ++i) assert(v[i] == i);

        // pop all
        while (!v.empty()) v.pop_back();
        assert(v.empty());
    }

    // =========================================================================
    // 19. String elements (non-POD)
    // =========================================================================
    {
        vector<std::string> v;
        v.push_back(std::string("hello"));
        v.push_back(std::string("world"));
        assert(v.size() == 2);
        assert(v[0] == "hello");
        assert(v[1] == "world");

        vector<std::string> v2(v);
        assert(v2[0] == "hello");

        vector<std::string> v3(lstl::move(v2));
        assert(v3[1] == "world");
        assert(v2.empty());
    }

    // =========================================================================
    // 20. Const vector access
    // =========================================================================
    {
        const vector<int> v = {1, 2, 3};
        assert(v[0] == 1);
        assert(v.front() == 1);
        assert(v.back() == 3);
        assert(v.data()[0] == 1);
        assert(v.size() == 3);

        int sum = 0;
        for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
        assert(sum == 6);
    }

    return 0;
}
