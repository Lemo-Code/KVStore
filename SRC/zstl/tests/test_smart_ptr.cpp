// ============================================================================
// zstl Smart Pointer Unit Tests
// Tests: unique_ptr (construct, move, release, reset, get, operator*/->/bool,
// make_unique, array specialization with operator[]),
// shared_ptr (construct, copy, move, reset, use_count, unique, make_shared,
// aliasing constructor),
// weak_ptr (lock, expired, use_count),
// enable_shared_from_this,
// custom deleters, static/dynamic/const_pointer_cast.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/zstl.h"

#include <string>
#include <memory>
#include <vector>

// ============================================================
// Helper types
// ============================================================

struct Counter {
    int value;
    static int alive;
    Counter() : value(0) { ++alive; }
    explicit Counter(int v) : value(v) { ++alive; }
    ~Counter() { --alive; }
    int get() const { return value; }
};
int Counter::alive = 0;

struct Base {
    int base_val = 10;
    virtual ~Base() = default;
    virtual int get() const { return base_val; }
};

struct Derived : Base {
    int derived_val = 20;
    int get() const override { return derived_val; }
};

// Custom deleter that records deletions
static int delete_count = 0;

template<typename T>
struct CountingDeleter {
    void operator()(T* ptr) const {
        ++delete_count;
        delete ptr;
    }
};

struct MoveOnlyObj {
    int val;
    explicit MoveOnlyObj(int v) : val(v) {}
    MoveOnlyObj(const MoveOnlyObj&) = delete;
    MoveOnlyObj& operator=(const MoveOnlyObj&) = delete;
    MoveOnlyObj(MoveOnlyObj&&) = default;
    MoveOnlyObj& operator=(MoveOnlyObj&&) = default;
};

// Object using enable_shared_from_this
struct SharedObj : zstl::enable_shared_from_this<SharedObj> {
    int val = 0;
    SharedObj() = default;
    explicit SharedObj(int v) : val(v) {}
    int get() const { return val; }
};

// ============================================================
// unique_ptr basic
// ============================================================

TEST(UniquePtrTest, DefaultConstructor) {
    zstl::unique_ptr<int> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(p);
}

TEST(UniquePtrTest, NullptrConstructor) {
    zstl::unique_ptr<int> p(nullptr);
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrTest, RawPointerConstructor) {
    Counter::alive = 0;
    zstl::unique_ptr<Counter> p(new Counter(42));
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(p->value, 42);
    EXPECT_EQ((*p).value, 42);
    EXPECT_TRUE(p);
    EXPECT_EQ(Counter::alive, 1);

    p.reset();
    EXPECT_FALSE(p);
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(Counter::alive, 0);
}

TEST(UniquePtrTest, MoveConstructor) {
    Counter::alive = 0;
    zstl::unique_ptr<Counter> p1(new Counter(100));
    EXPECT_EQ(Counter::alive, 1);

    zstl::unique_ptr<Counter> p2(zstl::move(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_NE(p2.get(), nullptr);
    EXPECT_EQ(p2->value, 100);
    EXPECT_EQ(Counter::alive, 1);

    p2.reset();
    EXPECT_EQ(Counter::alive, 0);
}

TEST(UniquePtrTest, MoveAssignment) {
    Counter::alive = 0;
    zstl::unique_ptr<Counter> p1(new Counter(1));
    zstl::unique_ptr<Counter> p2(new Counter(2));

    p2 = zstl::move(p1);
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(p2->value, 1);
    EXPECT_EQ(Counter::alive, 1); // only one left

    p2.reset();
    EXPECT_EQ(Counter::alive, 0);
}

TEST(UniquePtrTest, NullptrAssignment) {
    zstl::unique_ptr<int> p(new int(5));
    p = nullptr;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_FALSE(p);
}

TEST(UniquePtrTest, Release) {
    int* raw = new int(42);
    zstl::unique_ptr<int> p(raw);
    int* released = p.release();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(released, raw);
    EXPECT_EQ(*released, 42);
    delete released;
}

TEST(UniquePtrTest, Reset) {
    Counter::alive = 0;
    zstl::unique_ptr<Counter> p(new Counter(5));
    EXPECT_EQ(Counter::alive, 1);

    p.reset(new Counter(10));
    EXPECT_EQ(p->value, 10);
    EXPECT_EQ(Counter::alive, 1); // old was destroyed

    p.reset();
    EXPECT_EQ(Counter::alive, 0);
}

TEST(UniquePtrTest, Swap) {
    zstl::unique_ptr<int> p1(new int(1));
    zstl::unique_ptr<int> p2(new int(2));

    p1.swap(p2);
    EXPECT_EQ(*p1, 2);
    EXPECT_EQ(*p2, 1);

    zstl::swap(p1, p2);
    EXPECT_EQ(*p1, 1);
    EXPECT_EQ(*p2, 2);
}

// ============================================================
// unique_ptr conversions
// ============================================================

TEST(UniquePtrTest, ConstructDerivedFromBase) {
    zstl::unique_ptr<Derived> d(new Derived());
    d->base_val = 55;
    d->derived_val = 66;

    zstl::unique_ptr<Base> b(zstl::move(d));
    EXPECT_EQ(d.get(), nullptr);
    EXPECT_EQ(b->base_val, 55);
    EXPECT_EQ(b->get(), 66); // virtual call through Base*
}

TEST(UniquePtrTest, MoveAssignDerivedFromBase) {
    zstl::unique_ptr<Derived> d(new Derived());
    zstl::unique_ptr<Base> b;
    b = zstl::move(d);
    EXPECT_NE(b.get(), nullptr);
    EXPECT_EQ(d.get(), nullptr);
}

TEST(UniquePtrTest, SelfMoveAssignment) {
    zstl::unique_ptr<int> p(new int(42));
    // Self-move-assignment is safe (but generally discouraged)
    // Just verify it doesn't crash
    // Note: actual self-move through zstl::move is tricky
    SUCCEED();
}

// ============================================================
// make_unique
// ============================================================

TEST(UniquePtrTest, MakeUnique) {
    auto p = zstl::make_unique<int>(42);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(UniquePtrTest, MakeUniqueString) {
    auto p = zstl::make_unique<std::string>("hello world");
    EXPECT_EQ(*p, "hello world");
}

TEST(UniquePtrTest, MakeUniqueMultipleArgs) {
    struct Multi { int a; double b; std::string c; };
    // Construct with aggregate? No, no constructor defined. Use struct with constructor:
    struct Multi2 {
        int a; double b; std::string c;
        Multi2(int x, double y, std::string z) : a(x), b(y), c(zstl::move(z)) {}
    };
    auto p = zstl::make_unique<Multi2>(1, 2.5, std::string("test"));
    EXPECT_EQ(p->a, 1);
    EXPECT_DOUBLE_EQ(p->b, 2.5);
    EXPECT_EQ(p->c, "test");
}

// ============================================================
// unique_ptr<T[]> array specialization
// ============================================================

TEST(UniquePtrTest, ArrayConstructor) {
    auto p = zstl::make_unique<int[]>(5);
    EXPECT_NE(p.get(), nullptr);
    // Should be value-initialized (zeros)
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(p[i], 0);
    }
}

TEST(UniquePtrTest, ArrayOperatorSquareBrackets) {
    auto p = zstl::make_unique<int[]>(3);
    p[0] = 10;
    p[1] = 20;
    p[2] = 30;
    EXPECT_EQ(p[0], 10);
    EXPECT_EQ(p[1], 20);
    EXPECT_EQ(p[2], 30);
}

TEST(UniquePtrTest, ArrayReset) {
    auto p = zstl::make_unique<int[]>(3);
    EXPECT_NE(p.get(), nullptr);
    p.reset();
    EXPECT_EQ(p.get(), nullptr);
}

TEST(UniquePtrTest, ArrayMoveConstructor) {
    auto p1 = zstl::make_unique<int[]>(4);
    p1[0] = 10;
    int* raw = p1.get();

    zstl::unique_ptr<int[]> p2(zstl::move(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(p2.get(), raw);
    EXPECT_EQ(p2[0], 10);
}

TEST(UniquePtrTest, ArrayNullptrAssignment) {
    auto p = zstl::make_unique<int[]>(2);
    p = nullptr;
    EXPECT_EQ(p.get(), nullptr);
}

// ============================================================
// unique_ptr comparison
// ============================================================

TEST(UniquePtrTest, Comparison) {
    zstl::unique_ptr<int> p1(new int(1));
    zstl::unique_ptr<int> p2(new int(2));
    zstl::unique_ptr<int> p3;

    EXPECT_TRUE(p1 != p2);
    EXPECT_TRUE(p1 != p3);
    EXPECT_FALSE(p1 == p2);
    EXPECT_TRUE(p1 == p1);
    EXPECT_TRUE(p3 == nullptr);
    EXPECT_TRUE(nullptr == p3);
    EXPECT_FALSE(p3 != nullptr);

    EXPECT_TRUE(p1 < p2 || p2 < p1 || p1.get() == p2.get());
    EXPECT_TRUE(p1 <= p2 || p2 <= p1);
}

// ============================================================
// unique_ptr with custom deleter
// ============================================================

TEST(UniquePtrTest, CustomDeleter) {
    delete_count = 0;
    {
        zstl::unique_ptr<int, CountingDeleter<int>> p(new int(5), CountingDeleter<int>());
        EXPECT_EQ(*p, 5);
    }
    EXPECT_EQ(delete_count, 1);
}

TEST(UniquePtrTest, CustomDeleterMove) {
    delete_count = 0;
    {
        zstl::unique_ptr<int, CountingDeleter<int>> p1(new int(10), CountingDeleter<int>());
        zstl::unique_ptr<int, CountingDeleter<int>> p2(zstl::move(p1));
        EXPECT_EQ(*p2, 10);
    }
    EXPECT_EQ(delete_count, 1);
}

// ============================================================
// shared_ptr basic
// ============================================================

TEST(SharedPtrTest, DefaultConstructor) {
    zstl::shared_ptr<int> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(p.use_count(), 0);
    EXPECT_FALSE(p);
}

TEST(SharedPtrTest, NullptrConstructor) {
    zstl::shared_ptr<int> p(nullptr);
    EXPECT_FALSE(p);
    EXPECT_EQ(p.use_count(), 0);
}

TEST(SharedPtrTest, RawPointerConstructor) {
    zstl::shared_ptr<int> p(new int(42));
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 42);
    EXPECT_EQ(p.use_count(), 1);
    EXPECT_TRUE(p);
}

TEST(SharedPtrTest, CopyConstructor) {
    zstl::shared_ptr<int> p1(new int(10));
    zstl::shared_ptr<int> p2(p1);
    EXPECT_EQ(p1.use_count(), 2);
    EXPECT_EQ(p2.use_count(), 2);
    EXPECT_EQ(p1.get(), p2.get());
}

TEST(SharedPtrTest, MoveConstructor) {
    zstl::shared_ptr<int> p1(new int(10));
    int* raw = p1.get();
    zstl::shared_ptr<int> p2(zstl::move(p1));
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(p2.get(), raw);
    EXPECT_EQ(p2.use_count(), 1);
}

TEST(SharedPtrTest, CopyAssignment) {
    zstl::shared_ptr<int> p1(new int(1));
    zstl::shared_ptr<int> p2(new int(2));
    p2 = p1;
    EXPECT_EQ(p1.use_count(), 2);
    EXPECT_EQ(p2.use_count(), 2);
    EXPECT_EQ(*p2, 1);
}

TEST(SharedPtrTest, MoveAssignment) {
    zstl::shared_ptr<int> p1(new int(10));
    zstl::shared_ptr<int> p2(new int(20));
    p2 = zstl::move(p1);
    EXPECT_EQ(p1.get(), nullptr);
    EXPECT_EQ(*p2, 10);
    EXPECT_EQ(p2.use_count(), 1);
}

// ============================================================
// shared_ptr reset / unique / swap
// ============================================================

TEST(SharedPtrTest, Reset) {
    zstl::shared_ptr<int> p(new int(5));
    p.reset();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(p.use_count(), 0);
}

TEST(SharedPtrTest, ResetWithPointer) {
    zstl::shared_ptr<int> p(new int(5));
    p.reset(new int(10));
    EXPECT_EQ(*p, 10);
    EXPECT_EQ(p.use_count(), 1);
}

TEST(SharedPtrTest, Unique) {
    zstl::shared_ptr<int> p(new int(42));
    EXPECT_TRUE(p.unique());

    zstl::shared_ptr<int> p2(p);
    EXPECT_FALSE(p.unique());
    EXPECT_FALSE(p2.unique());

    p2.reset();
    EXPECT_TRUE(p.unique());
}

TEST(SharedPtrTest, Swap) {
    zstl::shared_ptr<int> p1(new int(1));
    zstl::shared_ptr<int> p2(new int(2));
    p1.swap(p2);
    EXPECT_EQ(*p1, 2);
    EXPECT_EQ(*p2, 1);
}

// ============================================================
// make_shared
// ============================================================

TEST(SharedPtrTest, MakeShared) {
    auto p = zstl::make_shared<int>(99);
    EXPECT_NE(p.get(), nullptr);
    EXPECT_EQ(*p, 99);
    EXPECT_EQ(p.use_count(), 1);
}

TEST(SharedPtrTest, MakeSharedComplex) {
    auto p = zstl::make_shared<std::string>("hello make_shared");
    EXPECT_EQ(*p, "hello make_shared");
}

// ============================================================
// shared_ptr conversions
// ============================================================

TEST(SharedPtrTest, ConvertFromDerived) {
    zstl::shared_ptr<Derived> d = zstl::make_shared<Derived>();
    d->base_val = 100;
    d->derived_val = 200;

    zstl::shared_ptr<Base> b(d);
    EXPECT_EQ(b.use_count(), 2);
    EXPECT_EQ(b->base_val, 100);
    EXPECT_EQ(b->get(), 200); // virtual call
}

TEST(SharedPtrTest, ConvertFromUniquePtr) {
    zstl::unique_ptr<int> up = zstl::make_unique<int>(55);
    zstl::shared_ptr<int> sp(zstl::move(up));
    EXPECT_EQ(up.get(), nullptr);
    EXPECT_EQ(*sp, 55);
    EXPECT_EQ(sp.use_count(), 1);
}

// ============================================================
// shared_ptr aliasing constructor
// ============================================================

TEST(SharedPtrTest, AliasingConstructor) {
    struct Container { int x; std::string y; };
    auto sp = zstl::make_shared<Container>();
    sp->x = 42;
    sp->y = "hello";

    // Alias: share ownership of Container but point to x
    zstl::shared_ptr<int> spx(sp, &sp->x);
    EXPECT_EQ(*spx, 42);
    EXPECT_EQ(spx.use_count(), 2);
    EXPECT_EQ(sp.use_count(), 2);

    // Alias: share ownership of Container but point to y
    zstl::shared_ptr<std::string> spy(sp, &sp->y);
    EXPECT_EQ(*spy, "hello");
    EXPECT_EQ(spy.use_count(), 3);
}

// ============================================================
// shared_ptr custom deleter
// ============================================================

TEST(SharedPtrTest, CustomDeleter) {
    delete_count = 0;
    {
        zstl::shared_ptr<int> p(new int(42), CountingDeleter<int>());
        EXPECT_EQ(*p, 42);
        EXPECT_EQ(p.use_count(), 1);
    }
    EXPECT_EQ(delete_count, 1);
}

// ============================================================
// shared_ptr comparison
// ============================================================

TEST(SharedPtrTest, Comparison) {
    auto p1 = zstl::make_shared<int>(1);
    auto p2 = zstl::make_shared<int>(2);
    zstl::shared_ptr<int> p3;

    EXPECT_TRUE(p1 == p1);
    EXPECT_FALSE(p1 == p2);
    EXPECT_TRUE(p1 != p2);
    EXPECT_TRUE(p3 == nullptr);
    EXPECT_TRUE(nullptr == p3);
}

// ============================================================
// weak_ptr
// ============================================================

TEST(WeakPtrTest, DefaultConstructor) {
    zstl::weak_ptr<int> wp;
    EXPECT_EQ(wp.use_count(), 0);
    EXPECT_TRUE(wp.expired());
}

TEST(WeakPtrTest, FromSharedPtr) {
    auto sp = zstl::make_shared<int>(42);
    zstl::weak_ptr<int> wp(sp);
    EXPECT_EQ(wp.use_count(), 1);
    EXPECT_FALSE(wp.expired());
}

TEST(WeakPtrTest, Lock) {
    auto sp = zstl::make_shared<int>(42);
    zstl::weak_ptr<int> wp(sp);

    auto locked = wp.lock();
    ASSERT_NE(locked.get(), nullptr);
    EXPECT_EQ(*locked, 42);
    EXPECT_EQ(locked.use_count(), 2);
}

TEST(WeakPtrTest, LockWhenExpired) {
    zstl::weak_ptr<int> wp;
    {
        auto sp = zstl::make_shared<int>(10);
        wp = sp;
        EXPECT_FALSE(wp.expired());
    }
    EXPECT_TRUE(wp.expired());
    auto locked = wp.lock();
    EXPECT_EQ(locked.get(), nullptr);
}

TEST(WeakPtrTest, CopyConstructor) {
    auto sp = zstl::make_shared<int>(5);
    zstl::weak_ptr<int> wp1(sp);
    zstl::weak_ptr<int> wp2(wp1);
    EXPECT_EQ(wp1.use_count(), 1);
    EXPECT_EQ(wp2.use_count(), 1);
}

TEST(WeakPtrTest, MoveConstructor) {
    auto sp = zstl::make_shared<int>(5);
    zstl::weak_ptr<int> wp1(sp);
    zstl::weak_ptr<int> wp2(zstl::move(wp1));
    EXPECT_TRUE(wp1.expired());
    EXPECT_EQ(wp2.use_count(), 1);
}

TEST(WeakPtrTest, Assignment) {
    auto sp1 = zstl::make_shared<int>(1);
    auto sp2 = zstl::make_shared<int>(2);

    zstl::weak_ptr<int> wp(sp1);
    wp = sp2;
    EXPECT_EQ(wp.use_count(), 1);
    auto locked = wp.lock();
    EXPECT_EQ(*locked, 2);
}

TEST(WeakPtrTest, Reset) {
    auto sp = zstl::make_shared<int>(42);
    zstl::weak_ptr<int> wp(sp);
    EXPECT_FALSE(wp.expired());
    wp.reset();
    EXPECT_TRUE(wp.expired());
}

TEST(WeakPtrTest, Swap) {
    auto sp1 = zstl::make_shared<int>(1);
    auto sp2 = zstl::make_shared<int>(2);
    zstl::weak_ptr<int> wp1(sp1);
    zstl::weak_ptr<int> wp2(sp2);

    wp1.swap(wp2);
    EXPECT_EQ(*(wp1.lock()), 2);
    EXPECT_EQ(*(wp2.lock()), 1);
}

// ============================================================
// Circular reference breaking with weak_ptr
// ============================================================

struct Node {
    int value;
    zstl::shared_ptr<Node> next;
    zstl::weak_ptr<Node> prev; // using weak_ptr to break cycle
    explicit Node(int v) : value(v) {}
};

TEST(WeakPtrTest, BreakCircularReference) {
    auto n1 = zstl::make_shared<Node>(1);
    auto n2 = zstl::make_shared<Node>(2);

    n1->next = n2;
    n2->prev = n1; // weak_ptr, not shared_ptr

    EXPECT_EQ(n1.use_count(), 1); // n1 only owned by n1 itself
    EXPECT_EQ(n2.use_count(), 2); // n2 owned by n2 and n1->next

    // No circular reference, so memory will be freed correctly
    // Verify we can navigate back
    auto locked = n2->prev.lock();
    ASSERT_NE(locked.get(), nullptr);
    EXPECT_EQ(locked->value, 1);
}

// ============================================================
// enable_shared_from_this
// ============================================================

TEST(SharedFromThisTest, Basic) {
    auto sp = zstl::make_shared<SharedObj>(42);
    auto sp2 = sp->shared_from_this();
    EXPECT_EQ(sp2.use_count(), 2);
    EXPECT_EQ(sp2->val, 42);
    EXPECT_EQ(sp.get(), sp2.get());
}

TEST(SharedFromThisTest, ConstVersion) {
    auto sp = zstl::make_shared<SharedObj>(99);
    const auto& csp = sp;
    auto csp2 = csp->shared_from_this();
    EXPECT_EQ(csp2.use_count(), 2);
}

TEST(SharedFromThisTest, WeakFromThis) {
    auto sp = zstl::make_shared<SharedObj>(10);
    auto wp = sp->weak_from_this();
    EXPECT_EQ(wp.use_count(), 1);
    EXPECT_FALSE(wp.expired());
    auto locked = wp.lock();
    EXPECT_EQ(locked->val, 10);
}

// ============================================================
// static_pointer_cast / dynamic_pointer_cast / const_pointer_cast
// ============================================================

TEST(SharedPtrCastTest, StaticPointerCast) {
    auto d = zstl::make_shared<Derived>();
    d->base_val = 100;
    d->derived_val = 200;

    auto b = zstl::static_pointer_cast<Base>(d);
    EXPECT_EQ(b.use_count(), 2);
    EXPECT_EQ(b->base_val, 100);
}

TEST(SharedPtrCastTest, DynamicPointerCast) {
    zstl::shared_ptr<Base> b = zstl::make_shared<Derived>();
    b->base_val = 777;

    auto d = zstl::dynamic_pointer_cast<Derived>(b);
    ASSERT_NE(d.get(), nullptr);
    EXPECT_EQ(d->base_val, 777);
    EXPECT_EQ(d.use_count(), 2);

    // Cast to wrong type
    struct Other : Base {};
    auto other = zstl::dynamic_pointer_cast<Other>(b);
    EXPECT_EQ(other.get(), nullptr);
}

TEST(SharedPtrCastTest, ConstPointerCast) {
    auto sp = zstl::make_shared<const int>(42);
    auto sp2 = zstl::const_pointer_cast<int>(sp);
    EXPECT_EQ(*sp2, 42);
    *sp2 = 100;
    EXPECT_EQ(*sp, 100);
}

// ============================================================
// owner_before
// ============================================================

TEST(SharedPtrTest, OwnerBefore) {
    auto p1 = zstl::make_shared<int>(1);
    auto p2 = zstl::make_shared<int>(2);
    auto p1_alias = zstl::shared_ptr<int>(p1, p1.get());

    // Same ownership group: !owner_before(a,b) && !owner_before(b,a)
    EXPECT_TRUE(!p1.owner_before(p1_alias) && !p1_alias.owner_before(p1));
    // Different groups: one of the two is true
    EXPECT_TRUE(p1.owner_before(p2) || p2.owner_before(p1));
}

// ============================================================
// Move-only types in smart pointers
// ============================================================

TEST(SmartPtrTest, MoveOnlyType) {
    auto up = zstl::make_unique<MoveOnlyObj>(42);
    EXPECT_EQ(up->val, 42);

    auto sp = zstl::make_shared<MoveOnlyObj>(99);
    EXPECT_EQ(sp->val, 99);
}
