#include "forgedb/concurrency/thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <vector>

namespace forgedb {
namespace {

TEST(ThreadPoolTest, ExecuteTasks) {
  ThreadPool pool(4);
  std::atomic<int> counter{0};

  std::vector<std::future<void>> futures;
  for (int i = 0; i < 100; ++i) {
    futures.push_back(pool.submit([&counter] {
      counter.fetch_add(1, std::memory_order_relaxed);
    }));
  }

  for (auto& f : futures) {
    f.get();
  }

  EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPoolTest, TaskReturnValues) {
  ThreadPool pool(2);

  auto f1 = pool.submit([] { return 42; });
  auto f2 = pool.submit([](int a, int b) { return a + b; }, 10, 20);

  EXPECT_EQ(f1.get(), 42);
  EXPECT_EQ(f2.get(), 30);
}

}  // namespace
}  // namespace forgedb
