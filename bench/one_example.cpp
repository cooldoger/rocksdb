#include "benchmark/benchmark.h"
#include <set>
#include <vector>

#include "util/timer.h"
#include "db/db_test_util.h"

namespace ROCKSDB_NAMESPACE {
static void BM_VectorInsert(benchmark::State &state) {
  while (state.KeepRunning()) {
    Timer timer(Env::Default());
    timer.Start();
    timer.Shutdown();
  }
}

// Register the function as a benchmark
BENCHMARK(BM_VectorInsert)->Range(8, 8 << 10);

//~~~~~~~~~~~~~~~~

// Define another benchmark
static void BM_SetInsert(benchmark::State &state) {
  while (state.KeepRunning()) {
    std::set<int> insertion_test;
    for (int i = 0, i_end = state.range(0); i < i_end; i++) {
      insertion_test.insert(i);
    }
  }
}
BENCHMARK(BM_SetInsert)->Range(8, 8 << 10);
}

BENCHMARK_MAIN();
