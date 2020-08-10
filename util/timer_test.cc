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

#if defined(OS_MACOSX) && !defined(NDEBUG)
  // On MacOS, `CondVar.TimedWait()` doesn't use the time from MockTimeEnv,
  // instead it still uses the system time.
  // This is just a mitigation that always trigger the CV timeout. It is not
  // perfect, only works for this test.
  void SetUp() override {
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->DisableProcessing();
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->ClearAllCallBacks();
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
        "InstrumentedCondVar::TimedWaitInternal", [&](void* arg) {
          uint64_t* time_us = reinterpret_cast<uint64_t*>(arg);
          if (*time_us < mock_env_->RealNowMicros()) {
            *time_us = mock_env_->RealNowMicros() + 1000;
          }
        });
    ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();
  }
#endif  // OS_MACOSX && !NDEBUG

  const uint64_t kSecond = 1000000;  // 1sec = 1000000us
};

TEST_F(TimerTest, SingleScheduleOnceTest) {
  const int kIterations = 1;
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);

  InstrumentedMutex mutex;
  InstrumentedCondVar test_cv(&mutex);

  Timer timer(mock_env_.get());
  int count = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex);
        count++;
        if (count >= kIterations) {
          test_cv.SignalAll();
        }
      },
      "fn_sch_test", 1 * kSecond, 0);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    InstrumentedMutexLock l(&mutex);
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
  const int kIterations = 1;
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);
  InstrumentedMutex mutex1;
  InstrumentedCondVar test_cv1(&mutex1);

  Timer timer(mock_env_.get());
  int count1 = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex1);
        count1++;
        if (count1 >= kIterations) {
          test_cv1.SignalAll();
        }
      },
      "fn_sch_test1", 1 * kSecond, 0);

  InstrumentedMutex mutex2;
  InstrumentedCondVar test_cv2(&mutex2);
  int count2 = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex2);
        count2 += 5;
        if (count2 >= kIterations) {
          test_cv2.SignalAll();
        }
      },
      "fn_sch_test2", 3 * kSecond, 0);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    InstrumentedMutexLock l(&mutex1);
    while (count1 < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv1.TimedWait(time_counter);
      }
  }

  // Wait for execution to finish
  {
    InstrumentedMutexLock l(&mutex2);
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

TEST_F(TimerTest, SingleScheduleRepeatedlyTest) {
  const int kIterations = 5;
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);

  InstrumentedMutex mutex;
  InstrumentedCondVar test_cv(&mutex);

  Timer timer(mock_env_.get());
  int count = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex);
        count++;
        if (count >= kIterations) {
          test_cv.SignalAll();
        }
      },
      "fn_sch_test", 1 * kSecond, 1 * kSecond);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    InstrumentedMutexLock l(&mutex);
    while(count < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv.TimedWait(time_counter);
    }
  }

  ASSERT_TRUE(timer.Shutdown());

  ASSERT_EQ(5, count);
}

TEST_F(TimerTest, MultipleScheduleRepeatedlyTest) {
  uint64_t time_counter = 0;
  mock_env_->set_current_time(0);
  Timer timer(mock_env_.get());

  InstrumentedMutex mutex1;
  InstrumentedCondVar test_cv1(&mutex1);
  const int kIterations1 = 5;
  int count1 = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex1);
        count1++;
        if (count1 >= kIterations1) {
          test_cv1.SignalAll();
        }
      },
      "fn_sch_test1", 0, 2 * kSecond);

  InstrumentedMutex mutex2;
  InstrumentedCondVar test_cv2(&mutex2);
  const int kIterations2 = 5;
  int count2 = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex2);
        count2++;
        if (count2 >= kIterations2) {
          test_cv2.SignalAll();
        }
      },
      "fn_sch_test2", 1 * kSecond, 2 * kSecond);

  ASSERT_TRUE(timer.Start());

  // Wait for execution to finish
  {
    InstrumentedMutexLock l(&mutex1);
    while(count1 < kIterations1) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv1.TimedWait(time_counter);
    }
  }

  timer.Cancel("fn_sch_test1");

  // Wait for execution to finish
  {
    InstrumentedMutexLock l(&mutex2);
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

TEST_F(TimerTest, AddAfterStartTest) {
  const int kIterations = 5;
  InstrumentedMutex mutex;
  InstrumentedCondVar test_cv(&mutex);

  // wait timer to run and then add a new job
  SyncPoint::GetInstance()->LoadDependency(
      {{"Timer::Run::Waiting", "TimerTest:AddAfterStartTest:1"}});
  SyncPoint::GetInstance()->EnableProcessing();

  mock_env_->set_current_time(0);
  Timer timer(mock_env_.get());

  ASSERT_TRUE(timer.Start());

  TEST_SYNC_POINT("TimerTest:AddAfterStartTest:1");
  int count = 0;
  timer.Add(
      [&] {
        InstrumentedMutexLock l(&mutex);
        count++;
        if (count >= kIterations) {
          test_cv.SignalAll();
        }
      },
      "fn_sch_test", 1 * kSecond, 1 * kSecond);

  // Wait for execution to finish
  uint64_t time_counter = 0;
  {
    InstrumentedMutexLock l(&mutex);
    while (count < kIterations) {
      time_counter += kSecond;
      mock_env_->set_current_time(time_counter);
      test_cv.TimedWait(time_counter);
    }
  }

  ASSERT_TRUE(timer.Shutdown());

  ASSERT_EQ(kIterations, count);
}

TEST_F(TimerTest, CancelRunningTask) {
  constexpr char kTestFuncName[] = "test_func";
  mock_env_->set_current_time(0);
  Timer timer(mock_env_.get());
  ASSERT_TRUE(timer.Start());
  int* value = new int;
  ASSERT_NE(nullptr, value);  // make linter happy
  *value = 0;
  SyncPoint::GetInstance()->DisableProcessing();
  SyncPoint::GetInstance()->LoadDependency({
      {"TimerTest::CancelRunningTask:test_func:0",
       "TimerTest::CancelRunningTask:BeforeCancel"},
      {"Timer::WaitForTaskCompleteIfNecessary:TaskExecuting",
       "TimerTest::CancelRunningTask:test_func:1"},
  });
  SyncPoint::GetInstance()->EnableProcessing();
  timer.Add(
      [&]() {
        *value = 1;
        TEST_SYNC_POINT("TimerTest::CancelRunningTask:test_func:0");
        TEST_SYNC_POINT("TimerTest::CancelRunningTask:test_func:1");
      },
      kTestFuncName, 0, 1 * kSecond);
  port::Thread control_thr([&]() {
    TEST_SYNC_POINT("TimerTest::CancelRunningTask:BeforeCancel");
    timer.Cancel(kTestFuncName);
    // Verify that *value has been set to 1.
    ASSERT_EQ(1, *value);
    delete value;
    value = nullptr;
  });
  mock_env_->set_current_time(1);
  control_thr.join();
  ASSERT_TRUE(timer.Shutdown());
}

TEST_F(TimerTest, ShutdownRunningTest) {
  mock_env_->set_current_time(0);
  Timer timer(mock_env_.get());

  SyncPoint::GetInstance()->DisableProcessing();
  SyncPoint::GetInstance()->LoadDependency({
     {"TimerTest::ShutdownRunningTest:test_func:0",
         "TimerTest::ShutdownRunningTest:BeforeShutdown"},
     {"Timer::WaitForTaskCompleteIfNecessary:TaskExecuting",
         "TimerTest::ShutdownRunningTest:test_func:1"},
  });
  SyncPoint::GetInstance()->EnableProcessing();

  ASSERT_TRUE(timer.Start());

  timer.Add(
      [&]() {
        TEST_SYNC_POINT("TimerTest::ShutdownRunningTest:test_func:0");
        std::cout << "hi" << std::endl;
        TEST_SYNC_POINT("TimerTest::ShutdownRunningTest:test_func:1");
      },
      "fn_test", 0, 1 * kSecond);

  timer.Add(
      [&]() {
        TEST_SYNC_POINT("TimerTest::ShutdownRunningTest:test_func2:0");
        std::cout << "hi2" << std::endl;
        TEST_SYNC_POINT("TimerTest::ShutdownRunningTest:test_func2:1");
      },
      "fn_test2", 0, 1 * kSecond);

  port::Thread control_thr([&]() {
    TEST_SYNC_POINT("TimerTest::ShutdownRunningTest:BeforeShutdown");
    timer.Shutdown();
  });
  mock_env_->set_current_time(1);
  control_thr.join();
}

}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
