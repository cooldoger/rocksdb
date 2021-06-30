#include "table/block_based/mock_block_based_table.h"
#include "table/block_based/filter_policy_internal.h"
#include "benchmark/benchmark.h"
#include "db/db_test_util.h"

namespace ROCKSDB_NAMESPACE {

// TODO: remove
const uint32_t FLAGS_vary_key_size_log2_interval = 5;
const bool FLAGS_vary_key_alignment = true;

struct KeyMaker {
  KeyMaker(size_t avg_size)
      : smallest_size_(avg_size -
                       (FLAGS_vary_key_size_log2_interval >= 30 ? 2 : 0)),
        buf_size_(avg_size + 11),  // pad to vary key size and alignment
        buf_(new char[buf_size_]) {
    memset(buf_.get(), 0, buf_size_);
    assert(smallest_size_ > 8);
  }
  size_t smallest_size_;
  size_t buf_size_;
  std::unique_ptr<char[]> buf_;

  // Returns a unique(-ish) key based on the given parameter values. Each
  // call returns a Slice from the same buffer so previously returned
  // Slices should be considered invalidated.
  Slice Get(uint32_t filter_num, uint32_t val_num) {
    size_t start = FLAGS_vary_key_alignment ? val_num % 4 : 0;
    size_t len = smallest_size_;
    if (FLAGS_vary_key_size_log2_interval < 30) {
      // To get range [avg_size - 2, avg_size + 2]
      // use range [smallest_size, smallest_size + 4]
      len += FastRange32(
          (val_num >> FLAGS_vary_key_size_log2_interval) * 1234567891, 5);
    }
    char * data = buf_.get() + start;
    // Populate key data such that all data makes it into a key of at
    // least 8 bytes. We also don't want all the within-filter key
    // variance confined to a contiguous 32 bits, because then a 32 bit
    // hash function can "cheat" the false positive rate by
    // approximating a perfect hash.
    EncodeFixed32(data, val_num);
    EncodeFixed32(data + 4, filter_num + val_num);
    // ensure clearing leftovers from different alignment
    EncodeFixed32(data + 8, 0);
    return Slice(data, len);
  }
};

// arguments:
// 0. filter mode
// 1. filter config bits_per_key
// 2. average data key length
// 3. data entry number
static void CustomArguments(benchmark::internal::Benchmark* b) {
  for (int filterMode : {BloomFilterPolicy::kLegacyBloom, BloomFilterPolicy::kFastLocalBloom, BloomFilterPolicy::kStandard128Ribbon}) {
//    for (int bits_per_key : {4, 10, 20, 30}) {
    for (int bits_per_key : {10, 20}) {
      for (int key_len_avg : {10, 100}) {
        for (int64_t entry_num : {1<<10, 1<<20}) {
          b->Args({filterMode, bits_per_key, key_len_avg, entry_num});
        }
      }
    }
  }
}

static void FilterBuild(benchmark::State &state) {
  // setup data
  auto filter = new BloomFilterPolicy(state.range(1), static_cast<BloomFilterPolicy::Mode>(state.range(0)));
  auto tester = new mock::MockBlockBasedTableTester(filter);
  KeyMaker km(state.range(2));
  std::unique_ptr<const char[]> owner;
  const int64_t kEntryNum = state.range(3);
  auto rnd = Random32(12345);
  uint32_t filter_num = rnd.Next();
  // run the test
  for (auto _ : state) {
    std::unique_ptr<FilterBitsBuilder> builder(tester->GetBuilder());
    for (uint32_t i = 0; i < kEntryNum; i++) {
      builder->AddKey(km.Get(filter_num, i));
    }
    auto ret = builder->Finish(&owner);
    state.counters["size"] = ret.size();
  }
}
BENCHMARK(FilterBuild)->Apply(CustomArguments);

static void FilterQueryPositive(benchmark::State &state) {
  // setup data
  auto filter = new BloomFilterPolicy(state.range(1), static_cast<BloomFilterPolicy::Mode>(state.range(0)));
  auto tester = new mock::MockBlockBasedTableTester(filter);
  KeyMaker km(state.range(2));
  std::unique_ptr<const char[]> owner;
  const int64_t kEntryNum = state.range(3);
  auto rnd = Random32(12345);
  uint32_t filter_num = rnd.Next();
  std::unique_ptr<FilterBitsBuilder> builder(tester->GetBuilder());
  for (uint32_t i = 0; i < kEntryNum; i++) {
    builder->AddKey(km.Get(filter_num, i));
  }
  auto data = builder->Finish(&owner);
  auto reader = filter->GetFilterBitsReader(data);

  // run test
  uint32_t i = 0;
  for (auto _ : state) {
    i++;
    i = i % kEntryNum;
    reader->MayMatch(km.Get(filter_num, i));
  }
}
BENCHMARK(FilterQueryPositive)->Apply(CustomArguments);

static void FilterQueryNegative(benchmark::State &state) {
  // setup data
  auto filter = new BloomFilterPolicy(state.range(1), static_cast<BloomFilterPolicy::Mode>(state.range(0)));
  auto tester = new mock::MockBlockBasedTableTester(filter);
  KeyMaker km(state.range(2));
  std::unique_ptr<const char[]> owner;
  const int64_t kEntryNum = state.range(3);
  auto rnd = Random32(12345);
  uint32_t filter_num = rnd.Next();
  std::unique_ptr<FilterBitsBuilder> builder(tester->GetBuilder());
  for (uint32_t i = 0; i < kEntryNum; i++) {
    builder->AddKey(km.Get(filter_num, i));
  }
  auto data = builder->Finish(&owner);
  auto reader = filter->GetFilterBitsReader(data);

  // run test
  uint32_t i = 0;
  uint64_t fp_cnt = 0;
  for (auto _ : state) {
    i++;
    auto result = reader->MayMatch(km.Get(filter_num + 1, i));
    if (result) {
      fp_cnt++;
    }
  }
  state.counters["FP %"] = benchmark::Counter(fp_cnt * 100, benchmark::Counter::kAvgIterations);
}
BENCHMARK(FilterQueryNegative)->Apply(CustomArguments);

}

BENCHMARK_MAIN();
