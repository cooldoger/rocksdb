//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "monitoring/stats_dump_scheduler.h"

#include "db/db_test_util.h"

namespace ROCKSDB_NAMESPACE {

class StatsDumpSchedulerTest : public DBTestBase {
 public:
  StatsDumpSchedulerTest()
      : DBTestBase("/stats_dump_scheduler_test"),
        mock_env_(new MockTimeEnv(Env::Default())) {}

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
};

#ifndef ROCKSDB_LITE

TEST_F(StatsDumpSchedulerTest, BasicTest) {
  Options options;
  options.stats_dump_period_sec = 5;
  options.stats_persist_period_sec = 10;
  options.create_if_missing = true;
  mock_env_->set_current_time(0);
  options.env = mock_env_.get();

  int dump_st_counter = 0;
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "DBImpl::DumpStats:Entry2", [&](void*) { dump_st_counter++; });

  int pst_st_counter = 0;
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "DBImpl::PersistStats:Entry2", [&](void*) { pst_st_counter++; });
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();

  Close();
  Reopen(options);

  ASSERT_EQ(5u, dbfull()->GetDBOptions().stats_dump_period_sec);
  ASSERT_EQ(10u, dbfull()->GetDBOptions().stats_persist_period_sec);

  dbfull()->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(1); });

  auto scheduler = dbfull()->TEST_GetStatsDumpScheduler();
  ASSERT_NE(nullptr, scheduler);
  ASSERT_EQ(2, scheduler->TEST_GetValidTaskNum());

  ASSERT_EQ(dump_st_counter, 1);
  ASSERT_EQ(pst_st_counter, 1);

  dbfull()->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(6); });

  ASSERT_EQ(dump_st_counter, 2);
  ASSERT_EQ(pst_st_counter, 1);

  dbfull()->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(11); });

  ASSERT_EQ(dump_st_counter, 3);
  ASSERT_EQ(pst_st_counter, 2);

  // Disable scheduler with SetOption
  ASSERT_OK(dbfull()->SetDBOptions(
      {{"stats_dump_period_sec", "0"}, {"stats_persist_period_sec", "0"}}));
  ASSERT_EQ(0, dbfull()->GetDBOptions().stats_dump_period_sec);
  ASSERT_EQ(0, dbfull()->GetDBOptions().stats_persist_period_sec);

  scheduler = dbfull()->TEST_GetStatsDumpScheduler();
  ASSERT_EQ(nullptr, scheduler);

  // Reenable one task
  ASSERT_OK(dbfull()->SetDBOptions({{"stats_dump_period_sec", "5"}}));
  ASSERT_EQ(5u, dbfull()->GetDBOptions().stats_dump_period_sec);
  ASSERT_EQ(0, dbfull()->GetDBOptions().stats_persist_period_sec);

  scheduler = dbfull()->TEST_GetStatsDumpScheduler();
  ASSERT_NE(nullptr, scheduler);
  ASSERT_EQ(1, scheduler->TEST_GetValidTaskNum());

  dump_st_counter = 0;
  dbfull()->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(16); });
  ASSERT_EQ(dump_st_counter, 1);

  Close();
}

TEST_F(StatsDumpSchedulerTest, MultiInstancesTest) {
  Close();

  const int kInstanceNum = 10;

  Options options;
  options.stats_dump_period_sec = 5;
  options.stats_persist_period_sec = 10;
  options.create_if_missing = true;
  mock_env_->set_current_time(0);
  options.env = mock_env_.get();

  int dump_st_counter = 0;
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "DBImpl::DumpStats:2", [&](void*) { dump_st_counter++; });

  int pst_st_counter = 0;
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->SetCallBack(
      "DBImpl::PersistStats:Entry2", [&](void*) { pst_st_counter++; });
  ROCKSDB_NAMESPACE::SyncPoint::GetInstance()->EnableProcessing();

  auto dbs = std::vector<DB*>(kInstanceNum);
  for (int i = 0; i < kInstanceNum; i++) {
    ASSERT_OK(
        DB::Open(options, test::PerThreadDBPath(std::to_string(i)), &(dbs[i])));
  }

  auto dbi = static_cast_with_check<DBImpl>(dbs[kInstanceNum - 1]);
  auto scheduler = dbi->TEST_GetStatsDumpScheduler();
  ASSERT_EQ(scheduler->TEST_GetValidTaskNum(), kInstanceNum * 2);

  int dump_st_expected_run = kInstanceNum;
  int pst_st_expected_run = kInstanceNum;
  dbi->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(1); });
  ASSERT_EQ(dump_st_counter, dump_st_expected_run);
  ASSERT_EQ(pst_st_counter, pst_st_expected_run);

  dump_st_expected_run += kInstanceNum;
  dbi->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(6); });
  ASSERT_EQ(dump_st_counter, dump_st_expected_run);
  ASSERT_EQ(pst_st_counter, pst_st_expected_run);

  dump_st_expected_run += kInstanceNum;
  pst_st_expected_run += kInstanceNum;
  dbi->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(11); });
  ASSERT_EQ(dump_st_counter, dump_st_expected_run);
  ASSERT_EQ(pst_st_counter, pst_st_expected_run);

  int half = kInstanceNum / 2;
  for (int i = 0; i < half; i++) {
    dbs[i]->Close();
  }

  dump_st_expected_run += (kInstanceNum - half) * 2;
  pst_st_expected_run += kInstanceNum - half;

  dbi->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(16); });
  dbi->TEST_WaitForStatsDumpRun([&] { mock_env_->set_current_time(21); });
  ASSERT_EQ(dump_st_counter, dump_st_expected_run);
  ASSERT_EQ(pst_st_counter, pst_st_expected_run);

  for (int i = half; i < kInstanceNum; i++) {
    dbs[i]->Close();
  }
}

TEST_F(StatsDumpSchedulerTest, MultiEnvTest) {
  Options options1;
  options1.stats_dump_period_sec = 5;
  options1.stats_persist_period_sec = 10;
  options1.create_if_missing = true;
  mock_env_->set_current_time(0);
  options1.env = mock_env_.get();
  Reopen(options1);

  MockTimeEnv* mock_env2 = new MockTimeEnv(Env::Default());
  Options options2;
  options2.stats_dump_period_sec = 5;
  options2.stats_persist_period_sec = 10;
  options2.create_if_missing = true;
  mock_env2->set_current_time(0);
  options1.env = mock_env2;

  std::string dbname = test::PerThreadDBPath("multi_env_test");
  DB* db;
  DB::Open(options2, dbname, &db);
  DBImpl* dbi = static_cast_with_check<DBImpl>(db);

  ASSERT_EQ(dbi->TEST_GetStatsDumpScheduler(),
            dbfull()->TEST_GetStatsDumpScheduler());

  db->Close();
  Close();
}
#endif  // !ROCKSDB_LITE
}  // namespace ROCKSDB_NAMESPACE

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
