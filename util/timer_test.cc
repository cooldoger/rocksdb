//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "util/timer.h"

#include "db/db_test_util.h"

namespace ROCKSDB_NAMESPACE {

class TimerTest : public testing::Test {
 public:
  TimerTest() : mock_env_(new MockTimeEnv(Env::Default())) {}

 protected:
  std::unique_ptr<MockTimeEnv> mock_env_;
};

TEST_F(TimerTest, ThreadTest) {
  Options options;
  options.stats_dump_period_sec = 1;
  options.create_if_missing = true;
  DB* db;

  std::cout << "test start" << std::endl;

  // Open DB
  Status s = DB::Open(options, "/tmp/db_test", &db);
  ASSERT_TRUE(s.ok());

  // Add data
  db->Put(WriteOptions(), "k1", "val1");

  // Read data
  std::string res;
  db->Get(ReadOptions(), "k1", &res);
  std::cout << res << std::endl;

  std::cout << "ref count: " << options.stats_dump_scheduler.use_count() << std::endl;
  std::cout << "end" << std::endl;

  std::cout << "sleep done" << std::endl;

  db->SetDBOptions({{"stats_dump_period_sec", "3"}});
  std::this_thread::sleep_for(std::chrono::seconds(10));

  delete db;
}

TEST_F(TimerTest, SingleScheduleOnceTest) {
  const uint64_t kSecond = 1000000;  // 1sec = 1000000us
  const int kIterations = 1;
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);
  port::Mutex mutex;
  port::CondVar test_cv(&mutex);

  Timer timer(mock_env_.get());
  int count = 0;
  timer.Add(
      [&] {
        MutexLock l(&mutex);
        count++;
        if (count >= kIterations) {
          test_cv.SignalAll();
        }
      },
      "fn_sch_test", 1 * kSecond, 0);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    MutexLock l(&mutex);
    while(count < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv.TimedWait(time_counter);
    }
  }

  ASSERT_TRUE(timer.Shutdown());

  ASSERT_EQ(1, count);
}

TEST_F(TimerTest, MultipleScheduleOnceTest) {
  const uint64_t kSecond = 1000000;  // 1sec = 1000000us
  const int kIterations = 1;
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);
  port::Mutex mutex1;
  port::CondVar test_cv1(&mutex1);

  Timer timer(mock_env_.get());
  int count1 = 0;
  timer.Add(
      [&] {
        MutexLock l(&mutex1);
        count1++;
        if (count1 >= kIterations) {
          test_cv1.SignalAll();
        }
      },
      "fn_sch_test1", 1 * kSecond, 0);

  port::Mutex mutex2;
  port::CondVar test_cv2(&mutex2);
  int count2 = 0;
  timer.Add(
      [&] {
        MutexLock l(&mutex2);
        count2 += 5;
        if (count2 >= kIterations) {
          test_cv2.SignalAll();
        }
      },
      "fn_sch_test2", 3 * kSecond, 0);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    MutexLock l(&mutex1);
    while (count1 < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv1.TimedWait(time_counter);
      }
  }

  // Wait for execution to finish
  {
    MutexLock l(&mutex2);
    while(count2 < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv2.TimedWait(time_counter);
    }
  }

  ASSERT_TRUE(timer.Shutdown());

  ASSERT_EQ(1, count1);
  ASSERT_EQ(5, count2);
}

TEST_F(TimerTest, DISABLED_SingleScheduleRepeatedlyTest) {
  const uint64_t kSecond = 1000000;  // 1sec = 1000000us
  const int kIterations = 5;
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);
  port::Mutex mutex;
  port::CondVar test_cv(&mutex);

  Timer timer(mock_env_.get());
  int count = 0;
  timer.Add(
      [&] {
        MutexLock l(&mutex);
        count++;
        fprintf(stderr, "%d\n", count);
        if (count >= kIterations) {
          test_cv.SignalAll();
        }
      },
      "fn_sch_test", 1 * kSecond, 1 * kSecond);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    MutexLock l(&mutex);
    while(count < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv.TimedWait(time_counter);
    }
  }

  ASSERT_TRUE(timer.Shutdown());

  ASSERT_EQ(5, count);
}

TEST_F(TimerTest, DISABLED_MultipleScheduleRepeatedlyTest) {
  const uint64_t kSecond = 1000000;  // 1sec = 1000000us
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);
  Timer timer(mock_env_.get());

  port::Mutex mutex1;
  port::CondVar test_cv1(&mutex1);
  const int kIterations1 = 5;
  int count1 = 0;
  timer.Add(
      [&] {
        MutexLock l(&mutex1);
        count1++;
        fprintf(stderr, "hello\n");
        if (count1 >= kIterations1) {
          test_cv1.SignalAll();
        }
      },
      "fn_sch_test1", 0, 2 * kSecond);

  port::Mutex mutex2;
  port::CondVar test_cv2(&mutex2);
  const int kIterations2 = 5;
  int count2 = 0;
  timer.Add(
      [&] {
        MutexLock l(&mutex2);
        count2++;
        fprintf(stderr, "world\n");
        if (count2 >= kIterations2) {
          test_cv2.SignalAll();
        }
      },
      "fn_sch_test2", 1 * kSecond, 2 * kSecond);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    MutexLock l(&mutex1);
    while(count1 < kIterations1) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv1.TimedWait(time_counter);
    }
  }

  timer.Cancel("fn_sch_test1");

  // Wait for execution to finish
  {
    MutexLock l(&mutex2);
    while(count2 < kIterations2) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv2.TimedWait(time_counter);
    }
  }

  timer.Cancel("fn_sch_test2");

  ASSERT_TRUE(timer.Shutdown());

  ASSERT_EQ(count1, 5);
  ASSERT_EQ(count2, 5);
}

TEST_F(TimerTest, TimerTest1) {
  const uint64_t kSecond = 1000000;  // 1sec = 1000000us
  uint64_t time_counter = 0;
  Timer timer(Env::Default());

  ASSERT_TRUE(timer.Start());
  std::cout << "start" << std::endl;
  std::cout << "sleep 1" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "end sleep 1" << std::endl;
  timer.Add(
      [] {
    fprintf(stdout, "hello\n");
  },
      "test1",
      0,
      1 * kSecond);

  std::cout << "sleep 10" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  std::cout << "end sleep 10" << std::endl;
  ASSERT_TRUE(timer.Shutdown());

}

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
