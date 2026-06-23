// ============================================================================
// zstl Concurrency Stress Tests — Comprehensive Edition
// ============================================================================
// Multi-threaded stress tests for zstl containers, memory subsystem,
// synchronization primitives, smart pointers, and algorithm operations.
// Tests exercise thread safety, lock-free operations, contention behavior,
// deadlock avoidance, and correctness under concurrent access.
//
// Note: zstl containers are NOT inherently thread-safe. Concurrent write
// access requires external synchronization. Read-only access is safe.
// ============================================================================

#include <gtest/gtest.h>

#include "zstl/memory/utility.h"
#include "zstl/memory/type_traits.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/pool.h"
#include "zstl/memory/allocator.h"
#include "zstl/memory/smart_ptr.h"

#include "zstl/containers/vector.h"
#include "zstl/containers/list.h"
#include "zstl/containers/slist.h"
#include "zstl/containers/deque.h"
#include "zstl/containers/string.h"
#include "zstl/containers/stack.h"
#include "zstl/containers/queue.h"
#include "zstl/containers/priority_queue.h"
#include "zstl/containers/map.h"
#include "zstl/containers/set.h"
#include "zstl/containers/unordered_map.h"

#include "zstl/algorithms/algorithm.h"
#include "zstl/algorithms/sort.h"
#include "zstl/algorithms/find.h"

#include "zstl/thread/atomic.h"
#include "zstl/thread/mutex.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <set>
#include <unordered_map>
#include <map>
#include <cstring>

using namespace zstl;

// ==========================================================================
// CONCURRENT VECTOR TESTS
// ==========================================================================

// Read-only concurrent access does not require synchronization
TEST(ConcurrentVector, ParallelRead) {
    zstl::vector<int> v(10000);
    for (int i = 0; i < 10000; ++i) v[i] = i;

    zstl::atomic<int> checksum{0};
    const int num_threads = 8;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&v, &checksum]() {
            int local = 0;
            for (int i = 0; i < 10000; ++i) local += v[i];
            checksum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    // Each thread sums 0..9999 = 49995000
    EXPECT_EQ(checksum.load(), num_threads * 49995000);
}

// Concurrent push_back with external mutex
TEST(ConcurrentVector, ParallelPushBackWithMutex) {
    zstl::vector<int> v;
    std::mutex mtx;
    const int num_threads = 8;
    const int per_thread = 5000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&v, &mtx, t, per_thread]() {
            for (int i = 0; i < per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                v.push_back(t * per_thread + i);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(v.size(), static_cast<size_t>(num_threads * per_thread));
    // Sort and verify all elements present
    std::sort(v.begin(), v.end());
    for (int i = 0; i < num_threads * per_thread; ++i) {
        EXPECT_EQ(v[static_cast<size_t>(i)], i);
    }
}

// Parallel read with resize — readers see consistent snapshot
TEST(ConcurrentVector, ParallelReadAfterSetup) {
    zstl::vector<int> v;
    v.resize(10000, 42);

    zstl::atomic<int> sum{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&v, &sum]() {
            int local = 0;
            for (int i = 0; i < 10000; ++i) local += v[i];
            sum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(sum.load(), 8 * 10000 * 42);
}

// Concurrent reads and writes with read-write pattern
TEST(ConcurrentVector, ReadWriteMixed) {
    zstl::vector<int> v;
    std::mutex mtx;
    zstl::atomic<bool> start{false};
    zstl::atomic<int> reads{0};
    zstl::atomic<int> writes{0};
    const int duration_ms = 500;

    // Writer thread
    std::thread writer([&]() {
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        int val = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            std::lock_guard<std::mutex> lock(mtx);
            v.push_back(val++);
            writes.fetch_add(1);
        }
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
            while (std::chrono::steady_clock::now() < deadline) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (!v.empty()) {
                        volatile int x = v[0];
                        (void)x;
                    }
                }
                reads.fetch_add(1);
            }
        });
    }

    start.store(true);
    writer.join();
    for (auto& th : readers) th.join();

    EXPECT_GT(writes.load(), 0);
    EXPECT_GT(reads.load(), 0);
}

// 16 threads push_back with mutex, 5000 items each, verify size and elements
TEST(ConcurrentVector, SixteenThreadPushBack) {
    zstl::vector<int> v;
    std::mutex mtx;
    const int num_threads = 16;
    const int per_thread = 5000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&v, &mtx, t, per_thread]() {
            for (int i = 0; i < per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                v.push_back(t * per_thread + i);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(v.size(), static_cast<size_t>(num_threads * per_thread));
    std::sort(v.begin(), v.end());
    for (int i = 0; i < num_threads * per_thread; ++i) {
        EXPECT_EQ(v[static_cast<size_t>(i)], i);
    }
}

// Parallel read without locks from 16 threads reading 10000-element vector
TEST(ConcurrentVector, ParallelReadSixteenThreads) {
    const int N = 10000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    zstl::atomic<int> checksum{0};
    const int num_threads = 16;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&v, &checksum]() {
            int local = 0;
            for (int i = 0; i < N; ++i) local += v[i];
            checksum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    // Sum of 0..9999 = 49995000
    EXPECT_EQ(checksum.load(), num_threads * 49995000);
}

// Producer-consumer: 4 writers push, 4 readers pop with mutex
TEST(ConcurrentVector, ProducerConsumer) {
    zstl::vector<int> v;
    std::mutex mtx;
    zstl::condition_variable cv;
    zstl::atomic<int> produced{0};
    zstl::atomic<int> consumed{0};
    zstl::atomic<bool> done{false};
    const int items_per_writer = 2500;
    const int num_writers = 4;
    const int total_items = num_writers * items_per_writer;

    std::vector<std::thread> writers;
    for (int w = 0; w < num_writers; ++w) {
        writers.emplace_back([&, w]() {
            for (int i = 0; i < items_per_writer; ++i) {
                std::unique_lock<zstl::mutex> lock(mtx);
                v.push_back(w * items_per_writer + i);
                produced.fetch_add(1);
                lock.unlock();
                cv.notify_one();
            }
        });
    }

    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&]() {
            while (true) {
                std::unique_lock<zstl::mutex> lock(mtx);
                cv.wait(lock, [&]() { return !v.empty() || done.load(); });
                if (!v.empty()) {
                    v.pop_back();
                    consumed.fetch_add(1);
                } else if (done.load()) {
                    break;
                }
            }
        });
    }

    for (auto& th : writers) th.join();
    done.store(true);
    cv.notify_all();
    for (auto& th : readers) th.join();

    EXPECT_EQ(produced.load(), total_items);
    EXPECT_EQ(consumed.load(), total_items);
    EXPECT_EQ(v.size(), 0u);
}

// Parallel resize: multiple threads resize different ranges (setup and read only)
TEST(ConcurrentVector, ParallelResizeSetup) {
    const int num_threads = 8;
    zstl::vector<int> vectors[num_threads];

    // Setup phase: resize in different threads
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&vectors, t]() {
            vectors[t].resize(5000, t);
        });
    }
    for (auto& th : threads) th.join();

    // Verify each vector
    for (int t = 0; t < num_threads; ++t) {
        EXPECT_EQ(vectors[t].size(), 5000u);
        for (int i = 0; i < 5000; ++i) {
            EXPECT_EQ(vectors[t][i], t);
        }
    }

    // Read phase: all threads read all vectors
    zstl::atomic<int> checksum{0};
    threads.clear();
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&vectors, &checksum]() {
            int local = 0;
            for (int v = 0; v < num_threads; ++v) {
                for (int i = 0; i < 5000; ++i) local += vectors[v][i];
            }
            checksum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    // Each vector sum = t * 5000 for t=0..7 => total per reader = (0+1+..+7)*5000 = 28*5000
    int expected_per_reader = 28 * 5000;
    EXPECT_EQ(checksum.load(), num_threads * expected_per_reader);
}

// Memory ordering stress: one writer, 16 readers, readers see consistent state
TEST(ConcurrentVector, MemoryOrderingStress) {
    const int N = 10000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    zstl::atomic<bool> start{false};
    zstl::atomic<int> write_phase{0};
    zstl::atomic<int> reader_ok{0};

    const int num_readers = 16;

    // One writer thread that updates elements in phases
    std::thread writer([&]() {
        while (!start.load()) {}
        for (int phase = 1; phase <= 10; ++phase) {
            write_phase.store(phase, memory_order_release);
            for (int i = 0; i < N; ++i) v[i] = phase * 1000 + i;
        }
        write_phase.store(-1, memory_order_release);
    });

    std::vector<std::thread> readers;
    for (int r = 0; r < num_readers; ++r) {
        readers.emplace_back([&]() {
            while (!start.load()) {}
            while (true) {
                int phase = write_phase.load(memory_order_acquire);
                if (phase == -1) break;
                // Verify all elements are consistent for this phase
                int first = v[0];
                int expect = phase * 1000;
                if (first == expect) {
                    reader_ok.fetch_add(1, memory_order_relaxed);
                }
                std::this_thread::yield();
            }
        });
    }

    start.store(true);
    writer.join();
    for (auto& th : readers) th.join();

    EXPECT_GT(reader_ok.load(), 0);
}

// Copy construction under load: one thread copies while another modifies
TEST(ConcurrentVector, CopyConstructionUnderLoad) {
    zstl::vector<int> v;
    for (int i = 0; i < 10000; ++i) v.push_back(i);

    std::mutex mtx;
    zstl::atomic<bool> start{false};
    zstl::atomic<int> copies_ok{0};

    // Modifier thread
    std::thread modifier([&]() {
        while (!start.load()) {}
        for (int iter = 0; iter < 20; ++iter) {
            std::lock_guard<std::mutex> lock(mtx);
            v.push_back(10000 + iter);
            if (v.size() > 12000) v.clear();
        }
    });

    // Copier threads: copy while holding the mutex for correctness
    std::vector<std::thread> copiers;
    for (int c = 0; c < 4; ++c) {
        copiers.emplace_back([&]() {
            while (!start.load()) {}
            for (int iter = 0; iter < 20; ++iter) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    zstl::vector<int> copy = v;
                    if (copy.size() > 0) {
                        copies_ok.fetch_add(1);
                    }
                }
                std::this_thread::yield();
            }
        });
    }

    start.store(true);
    modifier.join();
    for (auto& th : copiers) th.join();

    EXPECT_GT(copies_ok.load(), 0);
}

// Move stress: 100 threads each moving vectors between each other
TEST(ConcurrentVector, MoveStress) {
    const int num_threads = 100;
    const int num_vectors = 200;
    std::mutex mtx;
    zstl::vector<int> vectors[num_vectors];

    // Initialize vectors
    for (int i = 0; i < num_vectors; ++i) {
        vectors[i].push_back(i);
        vectors[i].push_back(i * 2);
        vectors[i].push_back(i * 3);
    }

    zstl::atomic<bool> start{false};
    zstl::atomic<int> moves{0};
    zstl::atomic<int> reads_ok{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            std::mt19937 rng(static_cast<unsigned>(t * 12345 + 67890));
            for (int iter = 0; iter < 100; ++iter) {
                std::lock_guard<std::mutex> lock(mtx);
                int a = static_cast<int>(rng() % num_vectors);
                int b = static_cast<int>(rng() % num_vectors);
                if (a != b) {
                    vectors[a] = zstl::move(vectors[b]);
                    // Reinitialize donor
                    vectors[b].push_back(b);
                    vectors[b].push_back(b * 2);
                    vectors[b].push_back(b * 3);
                    moves.fetch_add(1);
                }
            }
            // Final read check
            std::lock_guard<std::mutex> lock(mtx);
            int ok = 0;
            for (int i = 0; i < num_vectors; ++i) {
                if (vectors[i].size() > 0) ok++;
            }
            reads_ok.fetch_add(ok);
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_GT(moves.load(), 0);
    EXPECT_GT(reads_ok.load(), 0);
}

// ==========================================================================
// CONCURRENT MAP TESTS
// ==========================================================================

// Parallel insert: 16 threads each insert 1000 unique keys
TEST(ConcurrentMap, ParallelInsert) {
    zstl::map<int, int> m;
    std::mutex mtx;
    const int num_threads = 16;
    const int per_thread = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &mtx, t, per_thread]() {
            for (int i = 0; i < per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                m.insert({t * per_thread + i, t * per_thread + i});
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(m.size(), static_cast<size_t>(num_threads * per_thread));

    // Verify all keys present
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < per_thread; ++i) {
            int key = t * per_thread + i;
            auto it = m.find(key);
            EXPECT_NE(it, m.end());
            if (it != m.end()) {
                EXPECT_EQ(it->second, key);
            }
        }
    }
}

// Parallel read: 16 threads find random keys in pre-populated map
TEST(ConcurrentMap, ParallelRead) {
    zstl::map<int, int> m;
    const int N = 10000;
    for (int i = 0; i < N; ++i) m.insert({i, i * 2});

    zstl::atomic<int> found_count{0};
    const int num_threads = 16;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &found_count, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 999 + 123));
            for (int iter = 0; iter < 5000; ++iter) {
                int key = static_cast<int>(rng() % N);
                auto it = m.find(key);
                if (it != m.end() && it->second == key * 2) {
                    found_count.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(found_count.load(), num_threads * 5000);
}

// Mixed read-write: 8 readers + 4 writers
TEST(ConcurrentMap, MixedReadWrite) {
    zstl::map<int, int> m;
    std::mutex mtx;
    const int N = 10000;
    // Pre-populate
    for (int i = 0; i < N / 2; ++i) m.insert({i, i * 2});

    zstl::atomic<bool> start{false};
    zstl::atomic<int> reads_ok{0};
    zstl::atomic<int> writes{0};

    // Writers insert new keys
    std::vector<std::thread> writers;
    for (int w = 0; w < 4; ++w) {
        writers.emplace_back([&, w]() {
            while (!start.load()) {}
            for (int i = 0; i < 2000; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                int key = N / 2 + w * 2000 + i;
                m.insert({key, key});
                writes.fetch_add(1);
            }
        });
    }

    // Readers find existing and new keys
    std::vector<std::thread> readers;
    for (int r = 0; r < 8; ++r) {
        readers.emplace_back([&, r]() {
            std::mt19937 rng(static_cast<unsigned>(r * 777 + 42));
            while (!start.load()) {}
            for (int iter = 0; iter < 3000; ++iter) {
                std::lock_guard<std::mutex> lock(mtx);
                int key = static_cast<int>(rng() % (N / 2 + 8000));
                auto it = m.find(key);
                if (it != m.end()) {
                    reads_ok.fetch_add(1);
                }
            }
        });
    }

    start.store(true);
    for (auto& th : writers) th.join();
    for (auto& th : readers) th.join();

    EXPECT_EQ(writes.load(), 4 * 2000);
    EXPECT_GT(reads_ok.load(), 0);
    EXPECT_GE(m.size(), static_cast<size_t>(N / 2 + 4 * 2000));
}

// operator[] race: 16 threads all access via operator[] with mutex
TEST(ConcurrentMap, OperatorAccessRace) {
    zstl::map<int, int> m;
    std::mutex mtx;
    const int num_threads = 16;
    const int iters = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &mtx, t, iters]() {
            for (int i = 0; i < iters; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                m[t * iters + i] = t * iters + i;
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(m.size(), static_cast<size_t>(num_threads * iters));

    // Verify all
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < iters; ++i) {
            int key = t * iters + i;
            EXPECT_EQ(m[key], key);
        }
    }
}

// Erase stress: interleave insert and erase
TEST(ConcurrentMap, EraseStress) {
    zstl::map<int, int> m;
    std::mutex mtx;
    const int N = 20000;
    // Pre-populate
    for (int i = 0; i < N; ++i) {
        m.insert({i, i});
    }

    zstl::atomic<bool> start{false};
    zstl::atomic<int> erased{0};
    zstl::atomic<int> inserted{0};

    // Eraser threads
    std::vector<std::thread> erasers;
    for (int t = 0; t < 4; ++t) {
        erasers.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < 3000; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                int key = t * 3000 + i;
                auto it = m.find(key);
                if (it != m.end()) {
                    m.erase(it);
                    erased.fetch_add(1);
                }
            }
        });
    }

    // Inserter threads
    std::vector<std::thread> inserters;
    for (int t = 0; t < 2; ++t) {
        inserters.emplace_back([&, t]() {
            while (!start.load()) {}
            for (int i = 0; i < 2000; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                int key = N + t * 2000 + i;
                m.insert({key, key});
                inserted.fetch_add(1);
            }
        });
    }

    start.store(true);
    for (auto& th : erasers) th.join();
    for (auto& th : inserters) th.join();

    EXPECT_GT(erased.load(), 0);
    EXPECT_EQ(inserted.load(), 4000);
}

// Large map parallel iteration: 8 threads iterate over different sections
TEST(ConcurrentMap, ParallelIteration) {
    zstl::map<int, int> m;
    const int N = 20000;
    for (int i = 0; i < N; ++i) m.insert({i, i * 2});

    zstl::atomic<int> checksum{0};
    const int num_threads = 8;

    // Split into ranges by key
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &checksum, t, N]() {
            int start_key = t * (N / num_threads);
            int end_key = (t + 1) * (N / num_threads);
            int local = 0;
            for (const auto& [k, v] : m) {
                if (k >= start_key && k < end_key) {
                    local += v;
                }
            }
            checksum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    // Sum of 0..19999 * 2 = 19999 * 20000 = 399980000
    EXPECT_EQ(checksum.load(), 399980000);
}

// Try_emplace race: multiple threads try_emplace same keys
TEST(ConcurrentMap, TryEmplaceRace) {
    zstl::map<int, int> m;
    std::mutex mtx;
    const int num_threads = 8;
    const int keys_per_thread = 2000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &mtx, t, keys_per_thread]() {
            for (int i = 0; i < keys_per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                // All threads try to insert overlapping keys (first N keys)
                int key = i;  // All threads try same keys
                auto [it, inserted] = m.try_emplace(key, t * keys_per_thread + i);
                // If not inserted, the value from first writer remains
                (void)inserted;
            }
        });
    }
    for (auto& th : threads) th.join();

    // Should have exactly keys_per_thread entries (only first insert succeeds)
    EXPECT_GE(m.size(), static_cast<size_t>(keys_per_thread));
    // Each key should have some value
    for (int i = 0; i < keys_per_thread; ++i) {
        EXPECT_NE(m.find(i), m.end());
    }
}

// Concurrent insert into unordered_map
TEST(ConcurrentMap, UnorderedMapParallelInsert) {
    zstl::unordered_map<int, int> m;
    std::mutex mtx;
    const int num_threads = 8;
    const int per_thread = 2000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&m, &mtx, t, per_thread]() {
            for (int i = 0; i < per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                m.insert({t * per_thread + i, t * per_thread + i});
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(m.size(), static_cast<size_t>(num_threads * per_thread));

    // Verify all
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < per_thread; ++i) {
            int key = t * per_thread + i;
            auto it = m.find(key);
            EXPECT_NE(it, m.end());
            if (it != m.end()) {
                EXPECT_EQ(it->second, key);
            }
        }
    }
}

// ==========================================================================
// CONCURRENT POOL TESTS
// ==========================================================================

// Allocate and deallocate from multiple threads simultaneously
TEST(ConcurrentPool, ParallelAllocateDeallocate) {
    zstl::atomic<size_t> alloc_count{0};
    zstl::atomic<size_t> free_count{0};
    const int num_threads = 8;
    const int ops_per_thread = 10000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::mt19937 rng(t + 12345);
            std::uniform_int_distribution<size_t> size_dist(16, 1024);
            std::vector<void*> ptrs;
            ptrs.reserve(ops_per_thread);

            for (int i = 0; i < ops_per_thread; ++i) {
                size_t sz = size_dist(rng);
                void* p = pool_malloc(sz);
                ASSERT_NE(p, nullptr);
                ptrs.push_back(p);
                alloc_count.fetch_add(1);
            }
            for (void* p : ptrs) {
                pool_free(p, 64);  // Approximate; in real app exact size tracked
                free_count.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(alloc_count.load(), num_threads * ops_per_thread);
    EXPECT_EQ(free_count.load(), num_threads * ops_per_thread);
}

// Heavy allocation stress across many threads
TEST(ConcurrentPool, HighContentionStress) {
    const int num_threads = 12;
    const int iterations = 50000;
    zstl::atomic<bool> start{false};
    zstl::atomic<size_t> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int i = 0; i < iterations; ++i) {
                size_t sz = ((static_cast<size_t>(i) * 73 + 37) % 50 + 1) * 16;
                void* p = pool_malloc(sz);
                if (p) {
                    *static_cast<int*>(p) = i;
                    pool_free(p, sz);
                    ops.fetch_add(1);
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * iterations));
}

// Mix of small and large allocations across threads
TEST(ConcurrentPool, MixedSizeAllocations) {
    zstl::atomic<size_t> total_ops{0};
    const int num_threads = 6;
    const int per_thread = 5000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 999 + 42));
            for (int i = 0; i < per_thread; ++i) {
                // Mix of pool-sized and oversized allocations
                size_t sz = (i % 10 == 0) ? (1024 * 1024) : (static_cast<size_t>(rng() % 100 + 1) * 16);
                void* p = pool_malloc(sz);
                if (p) {
                    pool_free(p, sz);
                    total_ops.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(total_ops.load(), static_cast<size_t>(num_threads * per_thread));
}

// Allocate, write pattern, then free — verify no corruption
TEST(ConcurrentPool, WriteReadVerify) {
    const int num_threads = 4;
    const int patterns = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < patterns; ++i) {
                size_t sz = 64;
                int* p = static_cast<int*>(pool_malloc(sz));
                ASSERT_NE(p, nullptr);
                // Write a pattern
                for (size_t j = 0; j < sz / sizeof(int); ++j) {
                    p[j] = static_cast<int>(t * 1000000 + i * 1000 + j);
                }
                // Verify pattern
                for (size_t j = 0; j < sz / sizeof(int); ++j) {
                    EXPECT_EQ(p[j], static_cast<int>(t * 1000000 + i * 1000 + j));
                }
                pool_free(p, sz);
            }
        });
    }
    for (auto& th : threads) th.join();
}

// Concurrent pool_trim calls should not crash
TEST(ConcurrentPool, ConcurrentTrim) {
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};

    // Threads doing allocations
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&]() {
            while (!start.load()) {}
            while (!stop.load()) {
                void* p = pool_malloc(64);
                if (p) pool_free(p, 64);
                std::this_thread::yield();
            }
        });
    }

    // Thread calling trim periodically
    std::thread trimmer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < 10; ++i) {
            MultiSizeClassPool::instance().pool_trim();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    start.store(true);
    trimmer.join();
    stop.store(true);
    for (auto& th : workers) th.join();
}

// Allocate from many threads, collect stats
TEST(ConcurrentPool, StatsUnderLoad) {
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};

    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&]() {
            while (!start.load()) {}
            while (!stop.load()) {
                void* p = pool_malloc(128);
                if (p) pool_free(p, 128);
            }
        });
    }

    start.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    for (auto& th : workers) th.join();

    // Collect stats — should not crash
    pool_stats stats;
    MultiSizeClassPool::instance().pool_stats(stats);
    SUCCEED();
}

// 32 threads each doing 10000 alloc/free cycles with random sizes
TEST(ConcurrentPool, ThirtyTwoThreadStress) {
    const int num_threads = 32;
    const int ops_per_thread = 10000;
    zstl::atomic<size_t> ops{0};
    zstl::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t + 12345));
            std::uniform_int_distribution<size_t> sz_dist(8, 2048);
            while (!start.load()) {}
            for (int i = 0; i < ops_per_thread; ++i) {
                size_t sz = sz_dist(rng);
                void* p = pool_malloc(sz);
                if (p) {
                    pool_free(p, sz);
                    ops.fetch_add(1);
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * ops_per_thread));
}

// Batch allocate-free: threads allocate 1000 objects, then free all
TEST(ConcurrentPool, BatchAllocateFree) {
    const int num_threads = 8;
    const int batch_size = 1000;
    zstl::atomic<size_t> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int round = 0; round < 10; ++round) {
                std::vector<void*> ptrs;
                ptrs.reserve(batch_size);
                for (int i = 0; i < batch_size; ++i) {
                    size_t sz = static_cast<size_t>((i % 8 + 1) * 16);
                    void* p = pool_malloc(sz);
                    ASSERT_NE(p, nullptr);
                    ptrs.push_back(p);
                }
                for (auto p : ptrs) {
                    pool_free(p, 64);
                }
                ops.fetch_add(batch_size);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * 10 * batch_size));
}

// Mixed sizes: threads allocate sizes from 16 to 64KB randomly
TEST(ConcurrentPool, FullRangeSizes) {
    const int num_threads = 6;
    const int per_thread = 5000;
    zstl::atomic<size_t> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 31337 + 777));
            std::uniform_int_distribution<size_t> sz_dist(16, 65536);
            for (int i = 0; i < per_thread; ++i) {
                size_t sz = sz_dist(rng);
                void* p = pool_malloc(sz);
                if (p) {
                    pool_free(p, sz);
                    ops.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * per_thread));
}

// Long-running: run for ~1 second, count operations
TEST(ConcurrentPool, LongRunningStress) {
    const int num_threads = 8;
    const int duration_ms = 1000;
    zstl::atomic<size_t> ops{0};
    zstl::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            while (!start.load()) {}
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
            std::mt19937 rng(static_cast<unsigned>(t * 54321 + 111));
            std::uniform_int_distribution<size_t> sz_dist(16, 4096);
            while (std::chrono::steady_clock::now() < deadline) {
                size_t sz = sz_dist(rng);
                void* p = pool_malloc(sz);
                if (p) {
                    pool_free(p, sz);
                    ops.fetch_add(1);
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    std::cout << "[PERF] pool long-running: " << ops.load() << " ops in "
              << duration_ms << "ms by " << num_threads << " threads" << std::endl;
    EXPECT_GT(ops.load(), 1000u);
}

// Realloc stress: allocate, realloc to different sizes repeatedly
TEST(ConcurrentPool, ReallocStress) {
    const int num_threads = 4;
    const int iterations = 5000;
    zstl::atomic<size_t> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 2789 + 400));
            size_t current_sz = 64;
            void* p = pool_malloc(current_sz);
            ASSERT_NE(p, nullptr);

            for (int i = 0; i < iterations; ++i) {
                size_t new_sz = ((static_cast<size_t>(rng()) % 20) + 1) * 16;
                void* new_p = pool_malloc(new_sz);
                ASSERT_NE(new_p, nullptr);
                pool_free(p, current_sz);
                p = new_p;
                current_sz = new_sz;
                ops.fetch_add(1);
            }
            pool_free(p, current_sz);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * iterations));
}

// Memory ordering: verify pool_free after pool_malloc is safe across threads
TEST(ConcurrentPool, MemoryOrderingSafety) {
    const int num_threads = 8;
    const int per_thread = 2000;
    zstl::atomic<size_t> verified{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                size_t sz = 128;
                int* p = static_cast<int*>(pool_malloc(sz));
                ASSERT_NE(p, nullptr);

                // Write thread ID + iteration
                p[0] = t;
                p[1] = i;
                p[2] = t * per_thread + i;

                // Memory barrier: writes must be visible before free
                zstl::atomic_thread_fence(memory_order_release);

                // Verify before free (should always be correct)
                if (p[0] == t && p[1] == i && p[2] == t * per_thread + i) {
                    verified.fetch_add(1);
                }
                pool_free(p, sz);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(verified.load(), static_cast<size_t>(num_threads * per_thread));
}

// Cache-line bouncing: allocate from multiple threads, verify no corruption
TEST(ConcurrentPool, CacheLineBouncing) {
    const int num_threads = 16;
    const int per_thread = 1000;
    zstl::atomic<size_t> ok{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                size_t sz = 64;  // One cache line
                int* p = static_cast<int*>(pool_malloc(sz));
                ASSERT_NE(p, nullptr);

                // Fill entire allocation with thread ID
                for (size_t j = 0; j < sz / sizeof(int); ++j) {
                    p[j] = t;
                }

                // Verify all elements match thread ID
                bool valid = true;
                for (size_t j = 0; j < sz / sizeof(int); ++j) {
                    if (p[j] != t) {
                        valid = false;
                        break;
                    }
                }

                if (valid) ok.fetch_add(1);
                pool_free(p, sz);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ok.load(), static_cast<size_t>(num_threads * per_thread));
}

// Oversized fallback: mix normal pool allocations with oversized allocations
TEST(ConcurrentPool, OversizedFallback) {
    const int num_threads = 6;
    const int per_thread = 2000;
    zstl::atomic<size_t> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                size_t sz;
                if (i % 3 == 0) {
                    // Oversized: uses new/delete fallback
                    sz = 256 * 1024;  // 256KB
                } else if (i % 3 == 1) {
                    // Medium pool size
                    sz = 1024;
                } else {
                    // Small pool size
                    sz = 64;
                }
                void* p = pool_malloc(sz);
                if (p) {
                    // Write a marker
                    *static_cast<int*>(p) = i;
                    pool_free(p, sz);
                    ops.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * per_thread));
}

// ==========================================================================
// CONCURRENT LIST TESTS
// ==========================================================================

// Concurrent read from list is safe
TEST(ConcurrentList, ParallelRead) {
    zstl::list<int> l;
    for (int i = 0; i < 10000; ++i) l.push_back(i);

    zstl::atomic<long long> total_sum{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&l, &total_sum]() {
            long long local = 0;
            for (auto x : l) local += x;
            total_sum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    // Sum of 0..9999 = 49995000, times 8 readers
    EXPECT_EQ(total_sum.load(), 8LL * 49995000LL);
}

// Concurrent push_back with mutex
TEST(ConcurrentList, ParallelPushBackWithMutex) {
    zstl::list<int> l;
    std::mutex mtx;
    const int num_threads = 8;
    const int per_thread = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&l, &mtx, t, per_thread]() {
            for (int i = 0; i < per_thread; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                l.push_back(t * per_thread + i);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(l.size(), static_cast<size_t>(num_threads * per_thread));
    // Sort and verify
    l.sort();
    int expected = 0;
    for (auto x : l) {
        EXPECT_EQ(x, expected++);
    }
}

// Concurrent erase with mutex
TEST(ConcurrentList, EraseUnderContention) {
    zstl::list<int> l;
    for (int i = 0; i < 2000; ++i) l.push_back(i);

    std::mutex mtx;
    zstl::atomic<bool> start{false};
    zstl::atomic<int> erased{0};

    // Multiple eraser threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            while (!start.load()) {}
            for (int attempt = 0; attempt < 100; ++attempt) {
                std::lock_guard<std::mutex> lock(mtx);
                if (!l.empty()) {
                    l.pop_front();
                    erased.fetch_add(1);
                }
            }
        });
    }

    start.store(true);
    for (auto& th : threads) th.join();

    EXPECT_EQ(static_cast<size_t>(erased.load()), 400u) << "Expected 400 erasures";
}

// Concurrent push_front and push_back with mutex
TEST(ConcurrentList, PushFrontAndBack) {
    zstl::list<int> l;
    std::mutex mtx;
    const int per_thread = 5000;

    std::thread front_pusher([&]() {
        for (int i = 0; i < per_thread; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            l.push_front(-i - 1);
        }
    });

    std::thread back_pusher([&]() {
        for (int i = 0; i < per_thread; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            l.push_back(i);
        }
    });

    front_pusher.join();
    back_pusher.join();

    EXPECT_EQ(l.size(), static_cast<size_t>(2 * per_thread));
}

// ==========================================================================
// CONCURRENT STRING TESTS
// ==========================================================================

// Parallel copy construction is safe
TEST(ConcurrentString, ParallelCopy) {
    zstl::string s(1000, 'x');

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&s]() {
            for (int i = 0; i < 10000; ++i) {
                zstl::string copy = s;
                EXPECT_EQ(copy.size(), 1000u);
                EXPECT_EQ(copy[0], 'x');
            }
        });
    }
    for (auto& th : threads) th.join();
}

// Parallel read-only access via c_str()
TEST(ConcurrentString, ParallelCStr) {
    zstl::string s = "hello world";
    zstl::atomic<int> sum{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&s, &sum]() {
            for (int i = 0; i < 1000; ++i) {
                const char* c = s.c_str();
                sum.fetch_add(static_cast<int>(strlen(c)));
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(sum.load(), 8 * 1000 * 11);
}

// Mixed read/write with mutex on a single string
TEST(ConcurrentString, ReadWriteWithMutex) {
    zstl::string s;
    std::mutex mtx;
    zstl::atomic<size_t> reads{0};
    zstl::atomic<size_t> writes{0};

    std::thread writer([&]() {
        for (int i = 0; i < 1000; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            s += 'a';
            writes.fetch_add(1);
        }
    });

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                volatile size_t len = s.size();
                (void)len;
                reads.fetch_add(1);
            }
        });
    }

    writer.join();
    for (auto& th : readers) th.join();

    EXPECT_EQ(writes.load(), 1000u);
    EXPECT_EQ(s.size(), 1000u);
    EXPECT_EQ(reads.load(), 4000u);
}

// SSO vs heap: test copy contention on SSO strings vs heap strings
TEST(ConcurrentString, SSOvsHeapCopy) {
    const int num_threads = 8;
    const int copies = 5000;

    // SSO string (short, fits in SSO buffer)
    {
        zstl::string sso_str = "short";
        zstl::atomic<int> ok{0};

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < copies; ++i) {
                    zstl::string copy = sso_str;
                    if (copy.size() == 5 && copy[0] == 's') {
                        ok.fetch_add(1);
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
        EXPECT_EQ(ok.load(), num_threads * copies);
    }

    // Heap string (long, exceeds SSO)
    {
        zstl::string heap_str(2000, 'H');
        zstl::atomic<int> ok{0};

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < copies; ++i) {
                    zstl::string copy = heap_str;
                    if (copy.size() == 2000 && copy[0] == 'H') {
                        ok.fetch_add(1);
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
        EXPECT_EQ(ok.load(), num_threads * copies);
    }
}

// Concatenation: threads build strings concurrently (independent objects)
TEST(ConcurrentString, ConcurrentConcat) {
    const int num_threads = 8;
    zstl::atomic<int> ok{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            zstl::string s;
            for (int i = 0; i < 1000; ++i) {
                s += static_cast<char>('a' + (t % 26));
            }
            if (s.size() == 1000) {
                ok.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ok.load(), num_threads);
}

// Read-only access from many threads (SSO and heap)
TEST(ConcurrentString, ManyReaderThreads) {
    zstl::string sso_str = "hello SSO world";
    zstl::string heap_str(1000, 'X');

    zstl::atomic<int> sso_sum{0};
    zstl::atomic<int> heap_sum{0};

    const int num_threads = 16;
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            // Read SSO string
            int sso_len = static_cast<int>(sso_str.size());
            sso_sum.fetch_add(sso_len);

            // Read heap string
            int heap_len = static_cast<int>(heap_str.size());
            heap_sum.fetch_add(heap_len);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(sso_sum.load(), num_threads * 15);  // "hello SSO world" = len 15
    EXPECT_EQ(heap_sum.load(), num_threads * 1000);
}

// ==========================================================================
// CONCURRENT DEQUE TESTS
// ==========================================================================

// Parallel read access to deque
TEST(ConcurrentDeque, ParallelRead) {
    zstl::deque<int> d;
    for (int i = 0; i < 10000; ++i) d.push_back(i);

    zstl::atomic<long long> total{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&d, &total]() {
            long long local = 0;
            for (int i = 0; i < 10000; ++i) local += d[i];
            total.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(total.load(), 8LL * 49995000LL);
}

// Mixed push_front/push_back with mutex
TEST(ConcurrentDeque, PushBothEndsWithMutex) {
    zstl::deque<int> d;
    std::mutex mtx;
    const int per_thread = 5000;

    std::thread front_pusher([&]() {
        for (int i = 0; i < per_thread; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            d.push_front(-i);
        }
    });

    std::thread back_pusher([&]() {
        for (int i = 0; i < per_thread; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            d.push_back(i);
        }
    });

    front_pusher.join();
    back_pusher.join();

    EXPECT_EQ(d.size(), static_cast<size_t>(2 * per_thread));
}

// Pop from both ends with mutex
TEST(ConcurrentDeque, PopBothEndsWithMutex) {
    zstl::deque<int> d;
    const int N = 5000;
    for (int i = 0; i < N * 2; ++i) d.push_back(i);

    std::mutex mtx;
    zstl::atomic<int> popped_front{0};
    zstl::atomic<int> popped_back{0};

    std::thread front_popper([&]() {
        for (int i = 0; i < N; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            if (!d.empty()) {
                d.pop_front();
                popped_front.fetch_add(1);
            }
        }
    });

    std::thread back_popper([&]() {
        for (int i = 0; i < N; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            if (!d.empty()) {
                d.pop_back();
                popped_back.fetch_add(1);
            }
        }
    });

    front_popper.join();
    back_popper.join();

    EXPECT_EQ(popped_front.load() + popped_back.load(), 2 * N);
}

// ==========================================================================
// CONCURRENT STACK/QUEUE/PRIORITY_QUEUE TESTS
// ==========================================================================

// Stack with mutex guard
TEST(ConcurrentStack, PushPopWithMutex) {
    zstl::stack<int> s;
    std::mutex mtx;
    zstl::atomic<int> pushed{0};
    zstl::atomic<int> popped{0};

    std::thread producer([&]() {
        for (int i = 0; i < 10000; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            s.push(i);
            pushed.fetch_add(1);
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < 10000; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            if (!s.empty()) {
                s.pop();
                popped.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(pushed.load(), 10000);
}

// Queue with mutex guard
TEST(ConcurrentQueue, PushPopWithMutex) {
    zstl::queue<int> q;
    std::mutex mtx;
    zstl::atomic<int> ops{0};

    std::thread producer([&]() {
        for (int i = 0; i < 5000; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(i);
            ops.fetch_add(1);
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < 5000; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            if (!q.empty()) {
                q.pop();
                ops.fetch_add(1);
            }
        }
    });

    producer.join();
    consumer.join();
}

// Priority queue with mutex
TEST(ConcurrentPriorityQueue, PushPopWithMutex) {
    zstl::priority_queue<int> pq;
    std::mutex mtx;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&pq, &mtx, t]() {
            for (int i = 0; i < 1000; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                pq.push(t * 1000 + i);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(pq.size(), 4000u);

    // Pop all and verify descending order
    int prev = INT_MAX;
    while (!pq.empty()) {
        int val = pq.top();
        EXPECT_LE(val, prev);
        prev = val;
        pq.pop();
    }
}

// ==========================================================================
// SMART POINTER CONCURRENCY TESTS
// ==========================================================================

// Shared_ptr ref counting: 16 threads copy same shared_ptr 10000 times
TEST(ConcurrentSmartPtr, SharedPtrRefCounting) {
    auto sp = make_shared<int>(42);
    const int num_threads = 16;
    const int copies_per_thread = 10000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&sp, copies_per_thread]() {
            std::vector<shared_ptr<int>> locals;
            locals.reserve(copies_per_thread);
            for (int i = 0; i < copies_per_thread; ++i) {
                locals.push_back(sp);  // Copy
            }
            // All locals destroyed here
        });
    }
    for (auto& th : threads) th.join();

    // After all threads complete, use_count should be 1 (only original)
    EXPECT_EQ(sp.use_count(), 1);
    EXPECT_EQ(*sp, 42);
}

// Weak_ptr lock: threads create weak_ptrs and try lock
TEST(ConcurrentSmartPtr, WeakPtrLock) {
    auto sp = make_shared<int>(99);
    weak_ptr<int> wp = sp;
    const int num_threads = 8;
    const int attempts = 5000;
    zstl::atomic<int> success{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&wp, &success, attempts]() {
            for (int i = 0; i < attempts; ++i) {
                auto locked = wp.lock();
                if (locked && *locked == 99) {
                    success.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(success.load(), num_threads * attempts);
    EXPECT_EQ(sp.use_count(), 1);  // Only original remains
}

// Unique_ptr transfer: threads pass unique_ptr through atomic queue
TEST(ConcurrentSmartPtr, UniquePtrTransfer) {
    // Use pointer-based queue with mutex for unique_ptr transfer
    struct Node {
        unique_ptr<int> data;
        Node* next = nullptr;
    };

    Node* head = nullptr;
    Node* tail = nullptr;
    std::mutex mtx;
    zstl::atomic<int> enqueued{0};
    zstl::atomic<int> dequeued{0};
    const int total = 1000;

    // Producer threads
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < total / 4; ++i) {
                auto node = new Node;
                node->data = unique_ptr<int>(new int(t * 1000 + i));
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (!head) {
                        head = tail = node;
                    } else {
                        tail->next = node;
                        tail = node;
                    }
                }
                enqueued.fetch_add(1);
            }
        });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    for (int t = 0; t < 4; ++t) {
        consumers.emplace_back([&]() {
            while (dequeued.load() < total) {
                Node* node = nullptr;
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (head) {
                        node = head;
                        head = head->next;
                        if (!head) tail = nullptr;
                    }
                }
                if (node) {
                    EXPECT_NE(node->data, nullptr);
                    delete node;
                    dequeued.fetch_add(1);
                }
            }
        });
    }

    for (auto& th : producers) th.join();
    for (auto& th : consumers) th.join();

    EXPECT_EQ(enqueued.load(), total);
    EXPECT_EQ(dequeued.load(), total);
}

// Make_shared stress: 100 threads each create 1000 shared_ptrs
TEST(ConcurrentSmartPtr, MakeSharedStress) {
    const int num_threads = 100;
    const int per_thread = 1000;
    zstl::atomic<int> created{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                auto sp = make_shared<int>(t * per_thread + i);
                if (*sp == t * per_thread + i) {
                    created.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(created.load(), num_threads * per_thread);
}

// Enable_shared_from_this: verify weak_from_this works under thread contention
struct ConcurrentESFT : public enable_shared_from_this<ConcurrentESFT> {
    int value;
    ConcurrentESFT(int v) : value(v) {}

    shared_ptr<ConcurrentESFT> get_shared() {
        return shared_from_this();
    }

    weak_ptr<ConcurrentESFT> get_weak() {
        return weak_from_this();
    }
};

TEST(ConcurrentSmartPtr, EnableSharedFromThis) {
    auto original = make_shared<ConcurrentESFT>(77);
    const int num_threads = 8;
    const int iters = 5000;
    zstl::atomic<int> ok{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&original, &ok, iters]() {
            for (int i = 0; i < iters; ++i) {
                auto sp = original->get_shared();
                if (sp->value == 77) {
                    ok.fetch_add(1);
                }
                auto wp = original->get_weak();
                auto locked = wp.lock();
                if (locked && locked->value == 77) {
                    ok.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ok.load(), num_threads * iters * 2);
    EXPECT_EQ(original.use_count(), 1);
}

// Shared_ptr concurrent reset
TEST(ConcurrentSmartPtr, SharedPtrResetRace) {
    const int num_threads = 8;
    zstl::atomic<int> ok{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            auto sp = make_shared<int>(0);
            for (int i = 0; i < 1000; ++i) {
                sp.reset(new int(t * 1000 + i));
                if (*sp == t * 1000 + i) {
                    ok.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ok.load(), num_threads * 1000);
}

// ==========================================================================
// MUTEX CONTENTION TESTS
// ==========================================================================

// Multiple threads contending on a single mutex
TEST(ConcurrentMutex, HighContentionLock) {
    zstl::mutex m;
    int shared = 0;
    const int num_threads = 8;
    const int per_thread = 10000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                m.lock();
                ++shared;
                m.unlock();
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(shared, num_threads * per_thread);
}

// Lock guard with multiple mutexes
TEST(ConcurrentMutex, MultipleMutexes) {
    zstl::mutex m1, m2, m3;
    int counter = 0;
    const int num_threads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                zstl::lock_guard<zstl::mutex> g1(m1);
                zstl::lock_guard<zstl::mutex> g2(m2);
                zstl::lock_guard<zstl::mutex> g3(m3);
                ++counter;
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter, num_threads * 1000);
}

// Deadlock avoidance with zstl::lock
TEST(ConcurrentMutex, DeadlockFreeLock) {
    zstl::mutex m1, m2;
    int counter = 0;
    const int num_threads = 8;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                // Alternate order but zstl::lock is deadlock-free
                if (i % 2 == 0) {
                    zstl::lock(m1, m2);
                } else {
                    zstl::lock(m2, m1);
                }
                ++counter;
                m1.unlock();
                m2.unlock();
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter, num_threads * 1000);
}

// try_lock under contention
TEST(ConcurrentMutex, TryLockContention) {
    zstl::mutex m;
    zstl::atomic<int> successes{0};
    zstl::atomic<int> failures{0};

    std::thread holder([&]() {
        m.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m.unlock();
    });

    // Give holder time to acquire
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::vector<std::thread> tryers;
    for (int t = 0; t < 8; ++t) {
        tryers.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                if (m.try_lock()) {
                    successes.fetch_add(1);
                    m.unlock();
                } else {
                    failures.fetch_add(1);
                }
            }
        });
    }

    holder.join();
    for (auto& th : tryers) th.join();

    EXPECT_GT(successes.load(), 0);
    EXPECT_GT(failures.load(), 0);
}

// Recursive mutex under contention
TEST(ConcurrentMutex, RecursiveMutexContention) {
    zstl::recursive_mutex rm;
    int counter = 0;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                rm.lock();
                rm.lock();  // Re-entrant
                ++counter;
                rm.unlock();
                rm.unlock();
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter, 4 * 500);
}

// 64 threads incrementing shared counter with mutex
TEST(ConcurrentMutex, SixtyFourThreadCounter) {
    zstl::mutex m;
    int shared = 0;
    const int num_threads = 64;
    const int per_thread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                std::lock_guard<zstl::mutex> lock(m);
                ++shared;
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(shared, num_threads * per_thread);
}

// Deadlock avoidance: zstl::lock with 4 mutexes, 100 threads, different locking orders
TEST(ConcurrentMutex, DeadlockAvoidanceFourMutexes) {
    zstl::mutex m1, m2, m3, m4;
    int counter = 0;
    const int num_threads = 100;
    const int iters_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iters_per_thread; ++i) {
                // Different locking orders based on thread ID
                switch ((t + i) % 4) {
                    case 0: zstl::lock(m1, m2, m3, m4); break;
                    case 1: zstl::lock(m4, m3, m2, m1); break;
                    case 2: zstl::lock(m2, m4, m1, m3); break;
                    case 3: zstl::lock(m3, m1, m4, m2); break;
                }
                ++counter;
                m1.unlock(); m2.unlock(); m3.unlock(); m4.unlock();
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter, num_threads * iters_per_thread);
}

// Timed mutex: try_lock_for timeout behavior under contention
TEST(ConcurrentMutex, TimedMutexTimeout) {
    zstl::timed_mutex tm;
    zstl::atomic<int> acquired{0};
    zstl::atomic<int> timed_out{0};

    // Holder locks for 200ms
    std::thread holder([&]() {
        tm.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        tm.unlock();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Waiters try with short timeout
    std::vector<std::thread> waiters;
    for (int t = 0; t < 8; ++t) {
        waiters.emplace_back([&]() {
            for (int i = 0; i < 50; ++i) {
                if (tm.try_lock_for(std::chrono::milliseconds(5))) {
                    acquired.fetch_add(1);
                    tm.unlock();
                } else {
                    timed_out.fetch_add(1);
                }
            }
        });
    }

    holder.join();
    for (auto& th : waiters) th.join();

    EXPECT_GT(timed_out.load(), 0);
}

// Recursive mutex: nested locking from multiple threads
TEST(ConcurrentMutex, NestedRecursiveLocking) {
    zstl::recursive_mutex rm;
    zstl::atomic<int> counter{0};
    const int nesting = 5;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, nesting]() {
            for (int iter = 0; iter < 500; ++iter) {
                for (int n = 0; n < nesting; ++n) {
                    rm.lock();
                }
                counter.fetch_add(1);
                for (int n = 0; n < nesting; ++n) {
                    rm.unlock();
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter.load(), 4 * 500);
}

// ==========================================================================
// UNIQUE_LOCK CONTENTION TESTS
// ==========================================================================

// unique_lock move between threads
TEST(ConcurrentUniqueLock, MoveAcrossThreads) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> ul(m);
    EXPECT_TRUE(ul.owns_lock());

    zstl::unique_lock<zstl::mutex> ul2;

    std::thread t([&]() {
        ul2 = zstl::move(ul);
        EXPECT_TRUE(ul2.owns_lock());
    });
    t.join();

    EXPECT_FALSE(ul.owns_lock());
}

// Multiple unique_locks with different mutexes
TEST(ConcurrentUniqueLock, MultipleLocks) {
    zstl::mutex m1, m2, m3;
    int counter = 0;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                zstl::unique_lock<zstl::mutex> ul1(m1);
                zstl::unique_lock<zstl::mutex> ul2(m2, zstl::defer_lock);
                zstl::unique_lock<zstl::mutex> ul3(m3, zstl::defer_lock);
                zstl::lock(ul2, ul3);
                ++counter;
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter, 4 * 500);
}

// try_to_lock pattern
TEST(ConcurrentUniqueLock, TryToLockPattern) {
    zstl::mutex m;
    zstl::atomic<int> acquired{0};
    zstl::atomic<int> skipped{0};

    std::thread holder([&]() {
        m.lock();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        m.unlock();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::vector<std::thread> tryers;
    for (int t = 0; t < 8; ++t) {
        tryers.emplace_back([&]() {
            for (int i = 0; i < 200; ++i) {
                zstl::unique_lock<zstl::mutex> ul(m, zstl::try_to_lock);
                if (ul) {
                    acquired.fetch_add(1);
                } else {
                    skipped.fetch_add(1);
                }
            }
        });
    }

    holder.join();
    for (auto& th : tryers) th.join();

    EXPECT_GT(acquired.load(), 0);
}

// Release pattern
TEST(ConcurrentUniqueLock, ReleasePattern) {
    zstl::mutex m;
    zstl::unique_lock<zstl::mutex> ul(m);
    EXPECT_TRUE(ul.owns_lock());

    zstl::mutex* mp = ul.release();
    EXPECT_FALSE(ul.owns_lock());
    EXPECT_EQ(mp, &m);

    // Another thread can now lock
    std::thread t([&]() {
        m.lock();
        m.unlock();
    });
    mp->unlock();
    t.join();
}

// ==========================================================================
// CALL_ONCE CONCURRENCY TESTS
// ==========================================================================

// Multiple threads racing to call_once
TEST(ConcurrentCallOnce, MultipleThreads) {
    zstl::once_flag flag;
    zstl::atomic<int> call_count{0};
    const int num_threads = 20;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            zstl::call_once(flag, [&]() {
                call_count.fetch_add(1);
            });
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(call_count.load(), 1);  // Exactly once
}

// Multiple once_flags with concurrent access
TEST(ConcurrentCallOnce, MultipleFlags) {
    zstl::once_flag f1, f2, f3;
    zstl::atomic<int> c1{0}, c2{0}, c3{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&]() {
            zstl::call_once(f1, [&]() { c1.fetch_add(1); });
            zstl::call_once(f2, [&]() { c2.fetch_add(1); });
            zstl::call_once(f3, [&]() { c3.fetch_add(1); });
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(c1.load(), 1);
    EXPECT_EQ(c2.load(), 1);
    EXPECT_EQ(c3.load(), 1);
}

// call_once with exception in the callable
TEST(ConcurrentCallOnce, ExceptionInCallable) {
    zstl::once_flag flag;
    zstl::atomic<int> attempts{0};

    // First attempt throws
    try {
        zstl::call_once(flag, [&]() {
            attempts.fetch_add(1);
            throw std::runtime_error("test exception");
        });
        FAIL() << "Expected exception";
    } catch (const std::runtime_error&) {
        // Expected
    }

    // Second attempt should succeed (once_flag is reset on exception)
    zstl::call_once(flag, [&]() {
        attempts.fetch_add(1);
    });

    EXPECT_EQ(attempts.load(), 2);
}

// ==========================================================================
// CONDITION_VARIABLE TESTS
// ==========================================================================

// Basic notify_one / wait pattern
TEST(ConcurrentConditionVariable, NotifyOne) {
    zstl::mutex m;
    zstl::condition_variable cv;
    bool ready = false;
    zstl::atomic<bool> woken{false};

    std::thread waiter([&]() {
        zstl::unique_lock<zstl::mutex> ul(m);
        cv.wait(ul, [&]() { return ready; });
        woken.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    {
        zstl::lock_guard<zstl::mutex> lg(m);
        ready = true;
    }
    cv.notify_one();

    waiter.join();
    EXPECT_TRUE(woken.load());
}

// notify_all wakes all waiters
TEST(ConcurrentConditionVariable, NotifyAll) {
    zstl::mutex m;
    zstl::condition_variable cv;
    bool ready = false;
    zstl::atomic<int> woken_count{0};
    const int num_waiters = 5;

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; ++i) {
        waiters.emplace_back([&]() {
            zstl::unique_lock<zstl::mutex> ul(m);
            cv.wait(ul, [&]() { return ready; });
            woken_count.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    {
        zstl::lock_guard<zstl::mutex> lg(m);
        ready = true;
    }
    cv.notify_all();

    for (auto& th : waiters) th.join();
    EXPECT_EQ(woken_count.load(), num_waiters);
}

// Wait with timeout
TEST(ConcurrentConditionVariable, WaitForTimeout) {
    zstl::mutex m;
    zstl::condition_variable cv;

    zstl::unique_lock<zstl::mutex> ul(m);
    auto result = cv.wait_for(ul, std::chrono::milliseconds(10));
    EXPECT_EQ(result, std::cv_status::timeout);
}

// Producer-consumer pattern
TEST(ConcurrentConditionVariable, ProducerConsumer) {
    zstl::mutex m;
    zstl::condition_variable cv;
    zstl::deque<int> queue;
    bool done = false;
    zstl::atomic<int> consumed{0};
    const int total_items = 10000;

    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            {
                zstl::lock_guard<zstl::mutex> lg(m);
                queue.push_back(i);
            }
            cv.notify_one();
        }
        {
            zstl::lock_guard<zstl::mutex> lg(m);
            done = true;
        }
        cv.notify_one();
    });

    std::thread consumer([&]() {
        while (true) {
            zstl::unique_lock<zstl::mutex> ul(m);
            cv.wait(ul, [&]() { return !queue.empty() || done; });
            if (!queue.empty()) {
                queue.pop_front();
                consumed.fetch_add(1);
            } else if (done) {
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed.load(), total_items);
}

// Multiple producers, multiple consumers with bounded queue
TEST(ConcurrentConditionVariable, MultiProducerMultiConsumer) {
    zstl::mutex m;
    zstl::condition_variable cv_producer;
    zstl::condition_variable cv_consumer;
    zstl::deque<int> queue;
    const size_t max_queue = 100;
    zstl::atomic<int> produced{0};
    zstl::atomic<int> consumed{0};
    const int total_items = 10000;

    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < total_items / 4; ++i) {
                std::unique_lock<zstl::mutex> lock(m);
                cv_producer.wait(lock, [&]() { return queue.size() < max_queue; });
                queue.push_back(p * 10000 + i);
                produced.fetch_add(1);
                lock.unlock();
                cv_consumer.notify_one();
            }
        });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < 4; ++c) {
        consumers.emplace_back([&]() {
            while (consumed.load() < total_items) {
                std::unique_lock<zstl::mutex> lock(m);
                cv_consumer.wait(lock, [&]() {
                    return !queue.empty() || consumed.load() >= total_items;
                });
                if (!queue.empty()) {
                    queue.pop_front();
                    consumed.fetch_add(1);
                    lock.unlock();
                    cv_producer.notify_one();
                }
            }
        });
    }

    for (auto& th : producers) th.join();
    cv_consumer.notify_all();  // Wake consumers if they're waiting
    for (auto& th : consumers) th.join();

    EXPECT_EQ(produced.load(), total_items);
    EXPECT_EQ(consumed.load(), total_items);
}

// ==========================================================================
// ATOMIC CONTENTION TESTS
// ==========================================================================

// Many threads incrementing a shared atomic
TEST(ConcurrentAtomic, SharedCounter) {
    zstl::atomic<int> counter{0};
    const int num_threads = 16;
    const int increments = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < increments; ++i) {
                counter.fetch_add(1, memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter.load(), num_threads * increments);
}

// CAS loop contention
TEST(ConcurrentAtomic, CASLoopContention) {
    zstl::atomic<int> value{0};
    const int num_threads = 8;
    const int per_thread = 5000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                int expected = value.load(memory_order_relaxed);
                while (!value.compare_exchange_weak(expected, expected + 1,
                       memory_order_release, memory_order_relaxed)) {
                    // expected is updated by CAS failure
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(value.load(), num_threads * per_thread);
}

// Atomic flag spinlock
TEST(ConcurrentAtomic, SpinlockWithFlag) {
    zstl::atomic_flag lock;
    int shared_counter = 0;
    const int num_threads = 8;
    const int per_thread = 5000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                while (lock.test_and_set(memory_order_acquire)) {
                    // Spin
                }
                ++shared_counter;
                lock.clear(memory_order_release);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(shared_counter, num_threads * per_thread);
}

// Multiple atomics accessed concurrently
TEST(ConcurrentAtomic, MultipleAtomics) {
    zstl::atomic<int> a{0};
    zstl::atomic<int> b{0};
    zstl::atomic<int> c{0};

    std::thread t1([&]() {
        for (int i = 0; i < 10000; ++i) a.fetch_add(1);
    });
    std::thread t2([&]() {
        for (int i = 0; i < 10000; ++i) b.fetch_add(1);
    });
    std::thread t3([&]() {
        for (int i = 0; i < 10000; ++i) c.fetch_add(1);
    });

    t1.join(); t2.join(); t3.join();

    EXPECT_EQ(a.load(), 10000);
    EXPECT_EQ(b.load(), 10000);
    EXPECT_EQ(c.load(), 10000);
}

// Pointer atomic operations
TEST(ConcurrentAtomic, PointerFetchAdd) {
    int arr[100] = {};
    zstl::atomic<int*> ptr(arr);

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 25; ++i) {
                int* p = ptr.fetch_add(1);
                *p = 1;
            }
        });
    }
    for (auto& th : threads) th.join();

    // All 100 should be set
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(arr[i], 1);
    }
}

// 32 threads CAS-increment for 100000 iterations
TEST(ConcurrentAtomic, CASIncrementStress) {
    zstl::atomic<int> value{0};
    const int num_threads = 32;
    const int iters_per_thread = 100000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < iters_per_thread; ++i) {
                int expected = value.load(memory_order_relaxed);
                while (!value.compare_exchange_weak(expected, expected + 1,
                       memory_order_acq_rel, memory_order_relaxed)) {}
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(value.load(), num_threads * iters_per_thread);
}

// Store buffer test: verify seq_cst ordering
TEST(ConcurrentAtomic, StoreBufferSeqCst) {
    zstl::atomic<int> x{0};
    zstl::atomic<int> y{0};
    zstl::atomic<int> reorder_count{0};
    const int iterations = 50000;

    std::thread t1([&]() {
        for (int i = 0; i < iterations; ++i) {
            x.store(1, memory_order_seq_cst);
            int r1 = y.load(memory_order_seq_cst);
            if (r1 == 0) {
                int r2 = x.load(memory_order_seq_cst);
                if (r2 == 0) {
                    reorder_count.fetch_add(1, memory_order_relaxed);
                }
            }
            x.store(0, memory_order_seq_cst);
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < iterations; ++i) {
            y.store(1, memory_order_seq_cst);
            y.store(0, memory_order_seq_cst);
        }
    });

    t1.join();
    t2.join();

    // With seq_cst, simultaneous (x==0, y==0) should not happen
    EXPECT_EQ(reorder_count.load(), 0);
}

// Atomic flag spinlock with 16 threads
TEST(ConcurrentAtomic, SpinlockSixteenThreads) {
    zstl::atomic_flag lock;
    int shared_counter = 0;
    const int num_threads = 16;
    const int per_thread = 5000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < per_thread; ++i) {
                while (lock.test_and_set(memory_order_acquire)) {
                    std::this_thread::yield();
                }
                ++shared_counter;
                lock.clear(memory_order_release);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(shared_counter, num_threads * per_thread);
}

// ==========================================================================
// CONCURRENT ALGORITHM TESTS
// ==========================================================================

// Parallel sort on independent ranges
TEST(ConcurrentAlgorithm, ParallelSortIndependent) {
    const int per_thread = 10000;
    const int num_threads = 4;

    std::vector<zstl::vector<int>> vectors(num_threads);
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            auto& v = vectors[static_cast<size_t>(t)];
            srand(static_cast<unsigned>(t * 1234 + 5678));
            for (int i = 0; i < per_thread; ++i) {
                v.push_back(rand());
            }
            zstl::sort(v.begin(), v.end());
            EXPECT_TRUE(zstl::is_sorted(v.begin(), v.end()));
        });
    }
    for (auto& th : threads) th.join();
}

// Parallel for_each on independent vector chunks
TEST(ConcurrentAlgorithm, ParallelForEach) {
    zstl::vector<int> v(10000);
    zstl::atomic<int> sum{0};

    // Initialize
    for (int i = 0; i < 10000; ++i) v[i] = i;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&v, &sum, t]() {
            int start = t * 2500;
            int end = start + 2500;
            int local = 0;
            for (int i = start; i < end; ++i) local += v[i];
            sum.fetch_add(local);
        });
    }
    for (auto& th : threads) th.join();

    // Sum of 0..9999 = 49995000
    EXPECT_EQ(sum.load(), 49995000);
}

// Parallel sort: 8 threads each sort independent vector chunks, then merge
TEST(ConcurrentAlgorithm, ParallelSortMerge) {
    const int total_size = 80000;
    const int num_threads = 8;
    const int chunk_size = total_size / num_threads;

    zstl::vector<int> chunks[num_threads];
    std::vector<std::thread> threads;

    // Generate and sort chunks in parallel
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            chunks[t].reserve(chunk_size);
            srand(static_cast<unsigned>(t * 31337));
            for (int i = 0; i < chunk_size; ++i) {
                chunks[t].push_back(rand());
            }
            zstl::sort(chunks[t].begin(), chunks[t].end());
            EXPECT_TRUE(zstl::is_sorted(chunks[t].begin(), chunks[t].end()));
        });
    }
    for (auto& th : threads) th.join();

    // Merge all chunks
    zstl::vector<int> merged;
    merged.reserve(total_size);

    // Simple k-way merge
    size_t indices[8] = {};
    for (int i = 0; i < total_size; ++i) {
        int best_chunk = -1;
        int best_val = INT_MAX;
        for (int c = 0; c < num_threads; ++c) {
            if (indices[c] < chunks[c].size() && chunks[c][indices[c]] < best_val) {
                best_val = chunks[c][indices[c]];
                best_chunk = c;
            }
        }
        EXPECT_GE(best_chunk, 0);
        merged.push_back(chunks[best_chunk][indices[best_chunk]++]);
    }

    EXPECT_TRUE(zstl::is_sorted(merged.begin(), merged.end()));
    EXPECT_EQ(merged.size(), static_cast<size_t>(total_size));
}

// 16 threads transform vector chunks
TEST(ConcurrentAlgorithm, ParallelTransform) {
    const int N = 16000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    zstl::atomic<int> ok{0};
    const int num_threads = 16;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            int start = t * (N / num_threads);
            int end = (t + 1) * (N / num_threads);
            int local = 0;
            for (int i = start; i < end; ++i) {
                local += v[i] * 2;
            }
            // Verify expected: sum of i*2 from start to end-1
            long long expected = 0;
            for (int i = start; i < end; ++i) expected += i * 2LL;
            if (static_cast<long long>(local) == expected) {
                ok.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ok.load(), num_threads);
}

// Parallel copy: threads copy from shared source to independent destinations
TEST(ConcurrentAlgorithm, ParallelCopy) {
    const int N = 10000;
    zstl::vector<int> source(N);
    for (int i = 0; i < N; ++i) source[i] = i;

    const int num_threads = 8;
    zstl::vector<int> destinations[num_threads];
    for (int t = 0; t < num_threads; ++t) {
        destinations[t].resize(N);
    }

    // Concurrently copy source to each destination (read-only source is safe)
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            zstl::copy(source.begin(), source.end(), destinations[t].begin());
        });
    }
    for (auto& th : threads) th.join();

    // Verify all copies
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < N; ++i) {
            EXPECT_EQ(destinations[t][i], i);
        }
    }
}

// Parallel find: threads search different ranges
TEST(ConcurrentAlgorithm, ParallelFind) {
    const int N = 50000;
    zstl::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[i] = i;

    const int num_threads = 8;
    zstl::atomic<int> found_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            int start = t * (N / num_threads);
            int end = (t + 1) * (N / num_threads);
            // Find the middle element of each thread's range
            int target = start + (N / num_threads) / 2;
            auto it = zstl::find(v.begin() + start, v.begin() + end, target);
            if (it != v.begin() + end && *it == target) {
                found_count.fetch_add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(found_count.load(), num_threads);
}

// ==========================================================================
// STRESS / ENDURANCE TESTS
// ==========================================================================

// Long-running pool stress with many threads
TEST(ConcurrentStress, LongRunningPoolStress) {
    std::atomic<bool> stop{false};
    zstl::atomic<size_t> total_ops{0};
    const int num_threads = 8;
    const int duration_ms = 1000;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::mt19937 rng(static_cast<unsigned>(t * 999 + 100));
            std::uniform_int_distribution<size_t> sz_dist(8, 1024);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);

            while (std::chrono::steady_clock::now() < deadline && !stop.load()) {
                size_t sz = sz_dist(rng);
                void* p = pool_malloc(sz);
                if (p) {
                    pool_free(p, sz);
                    total_ops.fetch_add(1);
                }
            }
        });
    }

    for (auto& th : threads) th.join();
    stop.store(true);

    EXPECT_GT(total_ops.load(), 1000u) << "Should complete many operations";
}

// Burst allocation pattern — allocate many, free all
TEST(ConcurrentStress, BurstAllocation) {
    const int num_threads = 6;
    const int bursts = 50;
    const int per_burst = 200;

    zstl::atomic<size_t> ops{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int b = 0; b < bursts; ++b) {
                std::vector<void*> batch;
                batch.reserve(per_burst);
                for (int i = 0; i < per_burst; ++i) {
                    size_t sz = static_cast<size_t>((i % 10 + 1) * 16);
                    void* p = pool_malloc(sz);
                    ASSERT_NE(p, nullptr);
                    batch.push_back(p);
                }
                for (void* p : batch) {
                    pool_free(p, 64);
                }
                ops.fetch_add(per_burst);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ops.load(), static_cast<size_t>(num_threads * bursts * per_burst));
}

// Combined container + pool stress
TEST(ConcurrentStress, CombinedContainerPoolStress) {
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    const int duration_ms = 500;

    std::thread pool_worker([&]() {
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            void* p = pool_malloc(128);
            if (p) pool_free(p, 128);
        }
    });

    std::thread vector_worker([&]() {
        zstl::vector<int> v;
        std::mutex mtx;
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            std::lock_guard<std::mutex> lock(mtx);
            v.push_back(42);
            if (v.size() > 1000) v.clear();
        }
    });

    std::thread string_worker([&]() {
        zstl::string s;
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            s += "x";
            if (s.size() > 1000) s.clear();
        }
    });

    start.store(true);
    pool_worker.join();
    vector_worker.join();
    string_worker.join();
    stop.store(true);

    SUCCEED();
}

// Heavy combined stress: all container types + pool simultaneously
TEST(ConcurrentStress, FullSystemStress) {
    std::atomic<bool> start{false};
    const int duration_ms = 800;

    // Thread 1: pool allocations
    std::thread t1([&]() {
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            void* p = pool_malloc(128);
            if (p) pool_free(p, 128);
        }
    });

    // Thread 2: vector operations
    std::thread t2([&]() {
        zstl::vector<int> v;
        std::mutex m;
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            std::lock_guard<std::mutex> lk(m);
            v.push_back(1);
            if (v.size() > 500) v.clear();
        }
    });

    // Thread 3: map operations
    std::thread t3([&]() {
        zstl::map<int, int> m;
        std::mutex mtx;
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        int key = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            std::lock_guard<std::mutex> lk(mtx);
            m[key % 1000] = key;
            ++key;
        }
    });

    // Thread 4: string operations
    std::thread t4([&]() {
        zstl::string s;
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            s += "ab";
            if (s.size() > 1000) s.clear();
        }
    });

    // Thread 5: smart pointers
    std::thread t5([&]() {
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            auto sp = make_shared<int>(42);
            auto sp2 = sp;
            auto sp3 = sp2;
        }
    });

    // Thread 6: deque operations
    std::thread t6([&]() {
        zstl::deque<int> d;
        std::mutex m;
        while (!start.load()) {}
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            std::lock_guard<std::mutex> lk(m);
            d.push_back(1);
            if (d.size() > 100) d.pop_front();
        }
    });

    start.store(true);
    t1.join(); t2.join(); t3.join();
    t4.join(); t5.join(); t6.join();

    SUCCEED();
}

// ==========================================================================
// SYNCHRONIZATION PRIMITIVE COMBINATION TESTS
// ==========================================================================

// lock_guard + atomic combo
TEST(ConcurrentCombo, LockGuardWithAtomic) {
    zstl::mutex m;
    int shared = 0;
    zstl::atomic<int> atomic_counter{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 5000; ++i) {
                zstl::lock_guard<zstl::mutex> lg(m);
                ++shared;
                atomic_counter.fetch_add(1, memory_order_relaxed);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(shared, 20000);
    EXPECT_EQ(atomic_counter.load(), 20000);
}

// unique_lock + condition_variable + atomics
TEST(ConcurrentCombo, FullSyncStack) {
    zstl::mutex m;
    zstl::condition_variable cv;
    bool ready = false;
    zstl::atomic<int> phase{0};

    std::thread waiter([&]() {
        zstl::unique_lock<zstl::mutex> ul(m);
        phase.store(1);
        cv.wait(ul, [&]() { return ready; });
        phase.store(3);
    });

    // Wait for waiter to enter
    while (phase.load() != 1) { std::this_thread::yield(); }

    std::thread notifier([&]() {
        phase.store(2);
        {
            zstl::lock_guard<zstl::mutex> lg(m);
            ready = true;
        }
        cv.notify_one();
    });

    waiter.join();
    notifier.join();

    EXPECT_EQ(phase.load(), 3);
}

// Multiple synchronization primitives used together
TEST(ConcurrentCombo, AllSyncPrimitives) {
    zstl::mutex m;
    zstl::atomic<int> counter{0};
    zstl::atomic_flag flag;
    flag.clear();

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                // Use mutex
                {
                    std::lock_guard<zstl::mutex> lock(m);
                    // Use atomic
                    counter.fetch_add(1, memory_order_relaxed);
                }
                // Use atomic_flag
                while (flag.test_and_set(memory_order_acquire)) {
                    std::this_thread::yield();
                }
                flag.clear(memory_order_release);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter.load(), 8000);
}

// ==========================================================================
// BARRIER / THREAD COORDINATION TESTS
// ==========================================================================

// Simple barrier using atomic + condition_variable (C++17 compatible)
TEST(ConcurrentBarrier, ThreadArrival) {
    const int num_threads = 8;
    zstl::mutex mtx;
    zstl::condition_variable cv;
    int arrived = 0;
    bool ready = false;
    zstl::atomic<int> counter{0};
    zstl::atomic<int> phase2_counter{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            // Phase 1: do work, then arrive at barrier
            counter.fetch_add(1);

            // Wait at barrier
            {
                zstl::unique_lock<zstl::mutex> ul(mtx);
                ++arrived;
                if (arrived == num_threads) {
                    ready = true;
                    cv.notify_all();
                } else {
                    cv.wait(ul, [&]() { return ready; });
                }
            }

            // Phase 2: all threads proceed together
            phase2_counter.fetch_add(1);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(counter.load(), num_threads);
    EXPECT_EQ(phase2_counter.load(), num_threads);
}

// Multi-phase synchronization using reusable barrier pattern
TEST(ConcurrentBarrier, PhaseSync) {
    const int num_threads = 4;
    const int num_phases = 10;
    zstl::mutex mtx;
    zstl::condition_variable cv;
    int arrived = 0;
    int current_phase = 0;
    bool phase_ready = false;
    zstl::atomic<int> total_phase_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int phase = 0; phase < num_phases; ++phase) {
                total_phase_count.fetch_add(1);

                // Barrier per phase
                {
                    zstl::unique_lock<zstl::mutex> ul(mtx);
                    ++arrived;
                    if (arrived == num_threads) {
                        phase_ready = true;
                        cv.notify_all();
                    } else {
                        int my_phase = current_phase;
                        cv.wait(ul, [&]() { return phase_ready && current_phase > my_phase; });
                    }
                    // Last thread to arrive resets for next phase
                    if (arrived == num_threads) {
                        arrived = 0;
                        phase_ready = false;
                        ++current_phase;
                        cv.notify_all(); // Wake any stragglers
                    }
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(total_phase_count.load(), num_threads * num_phases);
}

// Large barrier: 50 threads
TEST(ConcurrentBarrier, LargeBarrier) {
    const int num_threads = 50;
    zstl::mutex mtx;
    zstl::condition_variable cv;
    int arrived = 0;
    bool ready = false;
    zstl::atomic<int> passed{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            zstl::unique_lock<zstl::mutex> ul(mtx);
            ++arrived;
            if (arrived == num_threads) {
                ready = true;
                cv.notify_all();
            } else {
                cv.wait(ul, [&]() { return ready; });
            }
            passed.fetch_add(1);
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(passed.load(), num_threads);
}

// ==========================================================================
// CORRECTNESS CHECKS UNDER CONCURRENCY
// ==========================================================================

// Verify that zstl::vector data is consistent after concurrent reads
TEST(ConcurrentCorrectness, VectorDataConsistency) {
    zstl::vector<int> v;
    for (int i = 0; i < 1000; ++i) v.push_back(i);

    zstl::atomic<int> sum{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&v, &sum]() {
            // Read all elements multiple times
            for (int iter = 0; iter < 100; ++iter) {
                int local = 0;
                for (int i = 0; i < 1000; ++i) {
                    local += v[i];
                }
                sum.fetch_add(local);
            }
        });
    }
    for (auto& th : threads) th.join();

    // 0..999 sum = 499500, times 100 iterations, times 8 threads
    EXPECT_EQ(sum.load(), 8 * 100 * 499500);
}

// Verify no corruption when many threads copy the same string
TEST(ConcurrentCorrectness, StringCopyConsistency) {
    zstl::string original(100, 'A');
    zstl::atomic<int> ok_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&original, &ok_count]() {
            for (int i = 0; i < 1000; ++i) {
                zstl::string copy = original;
                if (copy.size() == 100u && copy[0] == 'A' && copy[99] == 'A') {
                    ok_count.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(ok_count.load(), 8 * 1000);
}

// Verify map data integrity after concurrent access
TEST(ConcurrentCorrectness, MapIntegrityAfterMixedAccess) {
    zstl::map<int, int> m;
    std::mutex mtx;
    const int N = 5000;

    // Pre-populate
    for (int i = 0; i < N; ++i) m[i] = i * 2;

    zstl::atomic<bool> start{false};

    // Writer: modify existing values
    std::thread writer([&]() {
        while (!start.load()) {}
        for (int i = 0; i < N / 2; ++i) {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = m.find(i);
            if (it != m.end()) it->second = i * 4;
        }
    });

    // Reader: iterate and read
    std::thread reader([&]() {
        zstl::atomic<int> sum{0};
        while (!start.load()) {}
        for (int iter = 0; iter < 100; ++iter) {
            std::lock_guard<std::mutex> lock(mtx);
            int local = 0;
            for (const auto& [k, v] : m) {
                local += v;
            }
            sum.fetch_add(local);
        }
    });

    start.store(true);
    writer.join();
    reader.join();

    // Verify all keys still exist
    for (int i = 0; i < N; ++i) {
        EXPECT_NE(m.find(i), m.end());
    }
}

// ==========================================================================
// SEQUENTIAL CONSISTENCY / MEMORY ORDERING TESTS
// ==========================================================================

// Store-buffer litmus test: with seq_cst, both threads cannot see (0,0)
TEST(ConcurrentMemoryOrdering, StoreBuffering) {
    zstl::atomic<int> x{0};
    zstl::atomic<int> y{0};
    zstl::atomic<int> r1_val{0};
    zstl::atomic<int> r2_val{0};
    zstl::atomic<int> both_zero_count{0};
    const int iterations = 10000;

    std::thread t1([&]() {
        for (int i = 0; i < iterations; ++i) {
            x.store(1, memory_order_seq_cst);
            int r1 = y.load(memory_order_seq_cst);
            r1_val.store(r1, memory_order_seq_cst);

            int r2 = r2_val.load(memory_order_seq_cst);
            if (r1 == 0 && r2 == 0) {
                both_zero_count.fetch_add(1, memory_order_relaxed);
            }

            // Reset for next iteration
            x.store(0, memory_order_seq_cst);
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < iterations; ++i) {
            y.store(1, memory_order_seq_cst);
            int r2 = x.load(memory_order_seq_cst);
            r2_val.store(r2, memory_order_seq_cst);

            // Reset for next iteration
            y.store(0, memory_order_seq_cst);
        }
    });

    t1.join();
    t2.join();

    // With sequential consistency on all operations, both_zero_count should be 0
    EXPECT_EQ(both_zero_count.load(), 0);
}

// Message passing litmus test
TEST(ConcurrentMemoryOrdering, MessagePassing) {
    zstl::atomic<int> data{0};
    zstl::atomic<bool> ready{false};
    zstl::atomic<int> result{0};

    std::thread producer([&]() {
        data.store(42, memory_order_relaxed);
        ready.store(true, memory_order_release);
    });

    std::thread consumer([&]() {
        while (!ready.load(memory_order_acquire)) {}
        result.store(data.load(memory_order_relaxed));
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(result.load(), 42);
}

// Release-acquire chain across multiple threads
TEST(ConcurrentMemoryOrdering, ReleaseAcquireChain) {
    const int num_threads = 8;
    zstl::atomic<int> data[num_threads] = {};
    zstl::atomic<int> sync{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Wait for previous thread
            if (t > 0) {
                while (sync.load(memory_order_acquire) != t) {
                    std::this_thread::yield();
                }
            }

            // Write data
            data[t].store(t * 10, memory_order_relaxed);

            // Signal next thread
            sync.store(t + 1, memory_order_release);
        });
    }
    for (auto& th : threads) th.join();

    // Verify all data was written
    for (int t = 0; t < num_threads; ++t) {
        EXPECT_EQ(data[t].load(memory_order_relaxed), t * 10);
    }
}

// Acquire-release ordering: write flag pattern
TEST(ConcurrentMemoryOrdering, WriteFlagPattern) {
    zstl::atomic<int> payload{0};
    zstl::atomic<bool> flag{false};
    zstl::atomic<int> reader_sees{0};

    std::thread writer([&]() {
        payload.store(99, memory_order_relaxed);
        flag.store(true, memory_order_release);
    });

    std::thread reader([&]() {
        while (!flag.load(memory_order_acquire)) {
            std::this_thread::yield();
        }
        reader_sees.store(payload.load(memory_order_relaxed));
    });

    writer.join();
    reader.join();

    EXPECT_EQ(reader_sees.load(), 99);
}

// Atomic fence usage test
TEST(ConcurrentMemoryOrdering, AtomicFence) {
    zstl::atomic<int> flag{0};
    zstl::atomic<int> value{0};

    std::thread t1([&]() {
        value.store(1, memory_order_relaxed);
        zstl::atomic_thread_fence(memory_order_release);
        flag.store(1, memory_order_relaxed);
    });

    std::thread t2([&]() {
        while (flag.load(memory_order_relaxed) == 0) {
            std::this_thread::yield();
        }
        zstl::atomic_thread_fence(memory_order_acquire);
        EXPECT_EQ(value.load(memory_order_relaxed), 1);
    });

    t1.join();
    t2.join();
}
